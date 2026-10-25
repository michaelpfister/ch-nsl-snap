// Capture overlay (docs/02-decisions.md, D-011): the screen is frozen first,
// then the user selects on the frozen picture. Two modes, switchable with
// W and R (F-47): window (hover and click) and region (drag a rectangle).
// Resolves with the selection, or null when the user cancels.

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

export const Mode = Object.freeze({WINDOW: 'window', REGION: 'region'});

// A drag smaller than this (logical px) is a click, not a region.
const MIN_REGION = 4;

const PICKABLE_TYPES = new Set([
    Meta.WindowType.NORMAL,
    Meta.WindowType.DIALOG,
    Meta.WindowType.MODAL_DIALOG,
    Meta.WindowType.UTILITY,
]);

function isPickable(window) {
    const activeWorkspace = global.workspace_manager.get_active_workspace();
    return !window.is_override_redirect() &&
        !window.minimized &&
        PICKABLE_TYPES.has(window.get_window_type()) &&
        window.located_on_workspace(activeWorkspace);
}

function contains(r, x, y) {
    return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

/**
 * Freezes the screen: a picture of the whole stage plus every pickable
 * window rendered offscreen, like GNOME's own screenshot UI. Everything
 * the user selects afterwards comes from these pictures, so nothing changes
 * while selecting. The pointer is not part of it (F-05).
 *
 * @returns {Promise<object>} content and scale of the stage, and the
 *   windows topmost first with their contents and geometry
 */
export async function freeze() {
    const windows = [];
    // get_window_actors() is in stacking order, bottom first.
    for (const actor of global.get_window_actors().slice().reverse()) {
        const window = actor.meta_window;
        if (!isPickable(window))
            continue;
        try {
            windows.push({
                window,
                title: window.get_title() ?? '',
                frameRect: window.get_frame_rect(),
                bufferRect: window.get_buffer_rect(),
                scale: actor.get_resource_scale(),
                content: actor.paint_to_content(null),
            });
        } catch (e) {
            logError(e, `NSL Snap: could not render window "${window.get_title()}"`);
        }
    }

    const shooter = new Shell.Screenshot();
    const [content, scale] = await shooter.screenshot_stage_to_content();
    return {content, scale, windows};
}

const CaptureOverlay = GObject.registerClass(
class CaptureOverlay extends St.Widget {
    _init(frozen, mode, resolve) {
        super._init({reactive: true, can_focus: true});
        this._frozen = frozen;
        this._mode = mode;
        this._resolve = resolve;
        this._hovered = null;     // window entry under the pointer (window mode)
        this._region = null;      // {x, y, width, height} in logical px
        this._dragStart = null;   // [x, y] while dragging a new region
        this._pressInside = false; // press inside the region: a click accepts it

        const fillStage = () => new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        });
        this.add_constraint(fillStage());

        // The frozen picture covers the live screen.
        this._picture = new Clutter.Actor({content: frozen.content});
        this._picture.add_constraint(fillStage());
        this.add_child(this._picture);

        // Region mode: four dark bands around the region, or one over the
        // whole screen while there is none.
        this._shades = [];
        for (let i = 0; i < 4; i++) {
            const shade = new St.Widget({style_class: 'nsl-snap-shade', visible: false});
            this._shades.push(shade);
            this.add_child(shade);
        }
        this._outline = new St.Widget({style_class: 'nsl-snap-region', visible: false});
        this.add_child(this._outline);

        // Window mode: the picked window's frame.
        this._highlight = new St.Widget({style_class: 'nsl-snap-highlight', visible: false});
        this.add_child(this._highlight);

        // Hint bar at the top of the primary monitor (F1 hides it, F-47).
        this._hint = new St.Label({style_class: 'nsl-snap-hint'});
        this.add_child(this._hint);
    }

    open() {
        Main.uiGroup.add_child(this);
        this._grab = Main.pushModal(this);
        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        const [x, y] = global.get_pointer();
        this._setMode(this._mode, x, y);
    }

    _setMode(mode, x, y) {
        this._mode = mode;
        this._region = null;
        this._dragStart = null;
        this._hovered = mode === Mode.WINDOW ? this._windowAt(x, y) : null;
        this._sync();
    }

    _windowAt(x, y) {
        return this._frozen.windows.find(entry => contains(entry.frameRect, x, y)) ?? null;
    }

    _sync() {
        const [stageWidth, stageHeight] = global.stage.get_size();

        // Window mode.
        const entry = this._mode === Mode.WINDOW ? this._hovered : null;
        if (entry) {
            const r = entry.frameRect;
            this._highlight.set_position(r.x, r.y);
            this._highlight.set_size(r.width, r.height);
        }
        this._highlight.visible = entry !== null;

        // Region mode.
        const region = this._mode === Mode.REGION ? this._region : null;
        if (this._mode !== Mode.REGION) {
            this._shades.forEach(shade => shade.hide());
        } else if (!region) {
            this._shades.forEach((shade, i) => {
                shade.visible = i === 0;
            });
            this._shades[0].set_position(0, 0);
            this._shades[0].set_size(stageWidth, stageHeight);
        } else {
            const {x, y, width, height} = region;
            const bands = [
                [0, 0, stageWidth, y],                                   // above
                [0, y + height, stageWidth, stageHeight - y - height],   // below
                [0, y, x, height],                                       // left
                [x + width, y, stageWidth - x - width, height],          // right
            ];
            bands.forEach(([bx, by, bw, bh], i) => {
                this._shades[i].set_position(bx, by);
                this._shades[i].set_size(Math.max(0, bw), Math.max(0, bh));
                this._shades[i].show();
            });
        }
        if (region) {
            this._outline.set_position(region.x, region.y);
            this._outline.set_size(region.width, region.height);
        }
        this._outline.visible = region !== null;

        let text;
        if (this._mode === Mode.WINDOW)
            text = 'Fenster anklicken · R: Bereich · Esc: Abbrechen';
        else if (region)
            text = 'Klick in den Bereich oder Enter: Aufnehmen · Ziehen: neu · Rücktaste: verwerfen · Esc: Abbrechen';
        else
            text = 'Bereich aufziehen · W: Fenster · Esc: Abbrechen';
        this._hint.text = text;
        const monitor = Main.layoutManager.primaryMonitor;
        if (monitor) {
            const [, natWidth] = this._hint.get_preferred_width(-1);
            this._hint.set_position(
                Math.round(monitor.x + (monitor.width - natWidth) / 2), monitor.y + 24);
        }
    }

    _clampToStage(x, y) {
        const [stageWidth, stageHeight] = global.stage.get_size();
        return [Math.min(Math.max(x, 0), stageWidth), Math.min(Math.max(y, 0), stageHeight)];
    }

    vfunc_motion_event(event) {
        const [x, y] = event.get_coords();
        if (this._mode === Mode.WINDOW) {
            this._hovered = this._windowAt(x, y);
        } else if (this._dragStart) {
            const [sx, sy] = this._dragStart;
            const [ex, ey] = this._clampToStage(Math.round(x), Math.round(y));
            const region = {
                x: Math.min(sx, ex),
                y: Math.min(sy, ey),
                width: Math.abs(ex - sx),
                height: Math.abs(ey - sy),
            };
            if (region.width >= MIN_REGION && region.height >= MIN_REGION) {
                this._region = region;
                this._pressInside = false; // it became a drag
            }
        }
        this._sync();
        return Clutter.EVENT_STOP;
    }

    vfunc_button_press_event(event) {
        const button = event.get_button();
        if (button !== Clutter.BUTTON_PRIMARY) {
            this._finish(null);
            return Clutter.EVENT_STOP;
        }
        const [x, y] = event.get_coords();
        if (this._mode === Mode.WINDOW) {
            this._hovered = this._windowAt(x, y);
            if (this._hovered)
                this._finish({mode: Mode.WINDOW, window: this._hovered});
            return Clutter.EVENT_STOP;
        }
        // Region mode: a click inside the region accepts it; a drag
        // anywhere starts a new one.
        this._pressInside = this._region !== null && contains(this._region, x, y);
        this._dragStart = this._clampToStage(Math.round(x), Math.round(y));
        if (!this._pressInside)
            this._region = null;
        this._sync();
        return Clutter.EVENT_STOP;
    }

    vfunc_button_release_event(event) {
        if (event.get_button() !== Clutter.BUTTON_PRIMARY || this._mode !== Mode.REGION)
            return Clutter.EVENT_STOP;
        this._dragStart = null;
        if (this._pressInside && this._region) {
            this._finish({mode: Mode.REGION, region: this._region});
            return Clutter.EVENT_STOP;
        }
        this._pressInside = false;
        this._sync();
        return Clutter.EVENT_STOP;
    }

    vfunc_key_press_event(event) {
        const key = event.get_key_symbol();
        switch (key) {
        case Clutter.KEY_Escape:
            this._finish(null);
            break;
        case Clutter.KEY_Return:
        case Clutter.KEY_KP_Enter:
        case Clutter.KEY_space:
            if (this._mode === Mode.WINDOW && this._hovered)
                this._finish({mode: Mode.WINDOW, window: this._hovered});
            else if (this._mode === Mode.REGION && this._region)
                this._finish({mode: Mode.REGION, region: this._region});
            break;
        case Clutter.KEY_BackSpace:
        case Clutter.KEY_Delete:
            this._region = null;
            this._dragStart = null;
            this._sync();
            break;
        case Clutter.KEY_w:
        case Clutter.KEY_W:
        case Clutter.KEY_r:
        case Clutter.KEY_R: {
            const [x, y] = global.get_pointer();
            const wanted = key === Clutter.KEY_w || key === Clutter.KEY_W ? Mode.WINDOW : Mode.REGION;
            if (wanted !== this._mode)
                this._setMode(wanted, x, y);
            break;
        }
        case Clutter.KEY_F1:
            this._hint.visible = !this._hint.visible;
            break;
        }
        return Clutter.EVENT_STOP;
    }

    _finish(selection) {
        if (!this._resolve)
            return;
        const resolve = this._resolve;
        this._resolve = null;
        this.close();
        resolve(selection);
    }

    close() {
        if (this._closed)
            return;
        this._closed = true;
        if (this._grab) {
            Main.popModal(this._grab);
            this._grab = null;
        }
        global.display.set_cursor(Meta.Cursor.DEFAULT);
        this.destroy();
        if (this._resolve) {
            this._resolve(null);
            this._resolve = null;
        }
    }
});

/**
 * Shows the overlay over a frozen screen.
 *
 * @param {object} frozen - result of freeze()
 * @param {string} mode - Mode.WINDOW or Mode.REGION to start with
 * @returns {{promise: Promise<object|null>, cancel: Function}} resolves with
 *   {mode: 'window', window: entry} or {mode: 'region', region: rect}, or null
 */
export function select(frozen, mode) {
    let overlay;
    const promise = new Promise(resolve => {
        overlay = new CaptureOverlay(frozen, mode, resolve);
        overlay.open();
    });
    return {promise, cancel: () => overlay.close()};
}
