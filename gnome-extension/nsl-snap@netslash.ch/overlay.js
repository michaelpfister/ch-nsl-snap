// Capture overlay (docs/02-decisions.md, D-011): the screen is frozen first,
// then the user selects on the frozen picture. Three modes, switchable with
// W, R and S (F-47): window (hover and click), region (D-023: two clicks or
// a drag, with crosshair lines and a magnifier; arrow keys move by one
// physical pixel) and screen (F-01: click a monitor, Enter for all). Resolves with the selection, or null when the user
// cancels. The same behaviour as the app's own overlay
// (src/capture/CaptureOverlay.cpp, src/capture/RegionSelector.cpp).

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

export const Mode = Object.freeze({WINDOW: 'window', REGION: 'region', SCREEN: 'screen'});
// How a region is selected (setting of the app): two clicks or a drag.
export const RegionSelect = Object.freeze({CLICKS: 'clicks', DRAG: 'drag'});

// A press moved farther than this (physical px) is a drag, not a click.
const DRAG_THRESHOLD = 4;
// A second click closer than this to the first one is ignored (physical px).
const MIN_REGION = 2;
// The magnifier: an odd number of pixels, so the selection point is the
// middle one, each shown as a square of ZOOM logical px.
const MAGNIFIER_PIXELS = 15;
const ZOOM = 11;
const MAGNIFIER_SIZE = MAGNIFIER_PIXELS * ZOOM;
const MAGNIFIER_GAP = 24;

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

// The region selection without UI, as RegionSelector in the app: points
// are physical pixels of the frozen picture, both corner pixels included.
class RegionSelector {
    constructor(how) {
        this.how = how;
        this.state = 'idle'; // idle, pressed, started, dragging, done
        this.start = [0, 0];
        this.current = [0, 0];
        this._pressedAt = [0, 0];
    }

    get hasStart() {
        return this.state !== 'idle';
    }

    region() {
        if (this.state === 'idle')
            return null;
        const [sx, sy] = this.start;
        const [cx, cy] = this.current;
        const x = Math.min(sx, cx);
        const y = Math.min(sy, cy);
        return {x, y, width: Math.abs(cx - sx) + 1, height: Math.abs(cy - sy) + 1};
    }

    _finish(point) {
        this.current = point;
        const r = this.region();
        if (r.width < MIN_REGION || r.height < MIN_REGION)
            return false;
        this.state = 'done';
        return true;
    }

    press(point) {
        if (this.state === 'idle') {
            this.state = 'pressed';
            this.start = this.current = this._pressedAt = point;
        } else if (this.state === 'started') {
            this._finish(point); // the second click closes the region
        }
    }

    move(point) {
        if (this.state === 'done')
            return;
        this.current = point;
        if (this.state === 'pressed' &&
            (Math.abs(point[0] - this._pressedAt[0]) > DRAG_THRESHOLD ||
             Math.abs(point[1] - this._pressedAt[1]) > DRAG_THRESHOLD))
            this.state = 'dragging';
    }

    release(point) {
        if (this.state === 'pressed') {
            // A click: in drag mode it starts nothing.
            this.state = this.how === RegionSelect.DRAG ? 'idle' : 'started';
            this.current = point;
        } else if (this.state === 'dragging' && !this._finish(point)) {
            this.state = this.how === RegionSelect.CLICKS ? 'started' : 'idle';
        }
    }

    // Enter or Space: a click from the keyboard, in both modes.
    click(point) {
        if (this.state === 'idle') {
            this.state = 'started';
            this.start = this.current = point;
        } else if (this.state === 'started') {
            this._finish(point);
        }
    }

    discard() {
        if (this.state !== 'done')
            this.state = 'idle';
    }
}

const CaptureOverlay = GObject.registerClass(
class CaptureOverlay extends St.Widget {
    _init(frozen, mode, how, resolve) {
        super._init({reactive: true, can_focus: true});
        this._frozen = frozen;
        this._mode = mode;
        this._how = how;
        this._resolve = resolve;
        this._hovered = null;     // window entry under the pointer (window mode)
        this._selector = new RegionSelector(how);
        // The selection point in physical pixels: follows the pointer, moved
        // by the arrow keys.
        this._cursor = [0, 0];
        this._ignoreMotionAt = null;
        const texture = frozen.content.get_texture();
        this._textureSize = [texture.get_width(), texture.get_height()];

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

        // Crosshair lines across the screen: a dark and a light line.
        this._lines = ['dark', 'dark', 'light', 'light'].map(tone => {
            const line = new St.Widget({style_class: `nsl-snap-line-${tone}`, visible: false});
            this.add_child(line);
            return line;
        });

        // The magnifier: the frozen picture, enlarged with hard pixels and
        // clipped to a square, plus guides and the coordinates.
        this._magnifier = new St.Widget({
            clip_to_allocation: true, width: MAGNIFIER_SIZE, height: MAGNIFIER_SIZE,
            style_class: 'nsl-snap-magnifier', visible: false,
        });
        this._zoomed = new Clutter.Actor({content: frozen.content});
        this._zoomed.set_content_scaling_filters(Clutter.ScalingFilter.NEAREST, Clutter.ScalingFilter.NEAREST);
        this._zoomed.set_size(this._textureSize[0] * ZOOM, this._textureSize[1] * ZOOM);
        this._magnifier.add_child(this._zoomed);
        const half = Math.floor(MAGNIFIER_PIXELS / 2);
        for (const [x, y, w, h, style] of [
            [0, half * ZOOM, MAGNIFIER_SIZE, ZOOM, 'nsl-snap-magnifier-guide'],
            [half * ZOOM, 0, ZOOM, MAGNIFIER_SIZE, 'nsl-snap-magnifier-guide'],
            [half * ZOOM - 1, half * ZOOM - 1, ZOOM + 2, ZOOM + 2, 'nsl-snap-magnifier-pixel-outline'],
            [half * ZOOM, half * ZOOM, ZOOM, ZOOM, 'nsl-snap-magnifier-pixel'],
            [0, 0, MAGNIFIER_SIZE, MAGNIFIER_SIZE, 'nsl-snap-magnifier-frame'],
        ]) {
            const guide = new St.Widget({style_class: style, x, y, width: w, height: h});
            this._magnifier.add_child(guide);
        }
        this.add_child(this._magnifier);
        this._coordinates = new St.Label({style_class: 'nsl-snap-magnifier-label', visible: false});
        this.add_child(this._coordinates);

        // Window mode: the picked window's frame.
        this._highlight = new St.Widget({style_class: 'nsl-snap-highlight', visible: false});
        this.add_child(this._highlight);

        // Screen mode: the monitor under the pointer.
        this._monitorHighlight = new St.Widget({style_class: 'nsl-snap-monitor', visible: false});
        this.add_child(this._monitorHighlight);

        // Hint bar at the top of the primary monitor (F1 hides it, F-47).
        this._hint = new St.Label({style_class: 'nsl-snap-hint'});
        this.add_child(this._hint);
    }

    open() {
        Main.uiGroup.add_child(this);
        this._grab = Main.pushModal(this);
        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        const [x, y] = global.get_pointer();
        this._cursor = this._toPhysical(x, y);
        this._setMode(this._mode);
    }

    get _scale() {
        return this._frozen.scale;
    }

    // A stage position (logical) as the physical pixel under it.
    _toPhysical(x, y) {
        const [w, h] = this._textureSize;
        return [Math.min(Math.max(Math.floor(x * this._scale), 0), w - 1),
            Math.min(Math.max(Math.floor(y * this._scale), 0), h - 1)];
    }

    _setMode(mode) {
        this._mode = mode;
        this._selector = new RegionSelector(this._how);
        const [x, y] = this._cursor.map(v => (v + 0.5) / this._scale);
        this._hovered = mode === Mode.WINDOW ? this._windowAt(x, y) : null;
        this._sync();
    }

    _windowAt(x, y) {
        return this._frozen.windows.find(entry => contains(entry.frameRect, x, y)) ?? null;
    }

    _sync() {
        const [stageWidth, stageHeight] = global.stage.get_size();
        const scale = this._scale;

        // Window mode.
        const entry = this._mode === Mode.WINDOW ? this._hovered : null;
        if (entry) {
            const r = entry.frameRect;
            this._highlight.set_position(r.x, r.y);
            this._highlight.set_size(r.width, r.height);
        }
        this._highlight.visible = entry !== null;

        // Screen mode.
        const hoveredMonitor = this._mode === Mode.SCREEN ? this._monitorAtCursor() : null;
        if (hoveredMonitor) {
            this._monitorHighlight.set_position(hoveredMonitor.x, hoveredMonitor.y);
            this._monitorHighlight.set_size(hoveredMonitor.width, hoveredMonitor.height);
        }
        this._monitorHighlight.visible = hoveredMonitor !== null;

        // Region mode, in logical px from here on.
        const inRegionMode = this._mode === Mode.REGION;
        const physical = inRegionMode ? this._selector.region() : null;
        const region = physical && {
            x: physical.x / scale, y: physical.y / scale,
            width: physical.width / scale, height: physical.height / scale,
        };
        if (!inRegionMode) {
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
        this._syncMagnifier(inRegionMode, stageWidth, stageHeight);

        let text;
        if (this._mode === Mode.WINDOW)
            text = 'Fenster anklicken · R: Bereich · S: Bildschirm · Esc: Abbrechen';
        else if (this._mode === Mode.SCREEN)
            text = Main.layoutManager.monitors.length > 1
                ? 'Bildschirm anklicken · Enter: alle Bildschirme · W: Fenster · R: Bereich · Esc: Abbrechen'
                : 'Klicken oder Enter: Bildschirm aufnehmen · W: Fenster · R: Bereich · Esc: Abbrechen';
        else if (this._selector.hasStart)
            text = 'Gegenüberliegende Ecke anklicken · Rücktaste: neu beginnen · Esc: Abbrechen';
        else if (this._how === RegionSelect.DRAG)
            text = 'Bereich aufziehen · Pfeiltasten und Enter: pixelgenau · W: Fenster · Esc: Abbrechen';
        else
            text = 'Erste Ecke anklicken · Pfeiltasten: 1 Pixel · W: Fenster · Esc: Abbrechen';
        this._hint.text = text;
        const monitor = Main.layoutManager.primaryMonitor;
        if (monitor) {
            const [, natWidth] = this._hint.get_preferred_width(-1);
            this._hint.set_position(
                Math.round(monitor.x + (monitor.width - natWidth) / 2), monitor.y + 24);
        }
    }

    _syncMagnifier(visible, stageWidth, stageHeight) {
        for (const actor of [...this._lines, this._magnifier, this._coordinates])
            actor.visible = visible;
        if (!visible)
            return;
        const scale = this._scale;
        const [px, py] = this._cursor;
        // The middle of the selection pixel, in logical px.
        const cx = (px + 0.5) / scale;
        const cy = (py + 0.5) / scale;
        const [darkV, darkH, lightV, lightH] = this._lines;
        darkV.set_position(Math.round(cx) + 1, 0);
        darkV.set_size(1, stageHeight);
        darkH.set_position(0, Math.round(cy) + 1);
        darkH.set_size(stageWidth, 1);
        lightV.set_position(Math.round(cx), 0);
        lightV.set_size(1, stageHeight);
        lightH.set_position(0, Math.round(cy));
        lightH.set_size(stageWidth, 1);

        // The enlarged picture, shifted so the selection pixel is the middle one.
        const half = Math.floor(MAGNIFIER_PIXELS / 2);
        this._zoomed.set_position(-(px - half) * ZOOM, -(py - half) * ZOOM);

        const r = this._selector.hasStart ? this._selector.region() : null;
        this._coordinates.text = r ? `${px}, ${py} · ${r.width} × ${r.height}` : `${px}, ${py}`;
        const [, labelHeight] = this._coordinates.get_preferred_height(-1);

        // Below right of the pointer; flipped where it would leave the monitor.
        const monitor = Main.layoutManager.monitors.find(m => contains(m, cx, cy)) ??
            {x: 0, y: 0, width: stageWidth, height: stageHeight};
        let mx = cx + MAGNIFIER_GAP;
        let my = cy + MAGNIFIER_GAP;
        if (mx + MAGNIFIER_SIZE > monitor.x + monitor.width)
            mx = cx - MAGNIFIER_GAP - MAGNIFIER_SIZE;
        if (my + MAGNIFIER_SIZE + labelHeight > monitor.y + monitor.height)
            my = cy - MAGNIFIER_GAP - MAGNIFIER_SIZE - labelHeight;
        this._magnifier.set_position(Math.round(mx), Math.round(my));
        this._coordinates.set_position(Math.round(mx), Math.round(my) + MAGNIFIER_SIZE + 2);
    }

    vfunc_motion_event(event) {
        const [x, y] = event.get_coords();
        // The move that warping the pointer for an arrow key causes is
        // rounded to logical pixels; it must not undo the step.
        if (this._ignoreMotionAt && Math.round(x) === this._ignoreMotionAt[0] &&
            Math.round(y) === this._ignoreMotionAt[1])
            return Clutter.EVENT_STOP;
        this._ignoreMotionAt = null;
        this._cursor = this._toPhysical(x, y);
        if (this._mode === Mode.WINDOW)
            this._hovered = this._windowAt(x, y);
        else
            this._selector.move(this._cursor);
        this._sync();
        return Clutter.EVENT_STOP;
    }

    vfunc_button_press_event(event) {
        if (event.get_button() !== Clutter.BUTTON_PRIMARY) {
            this._finish(null);
            return Clutter.EVENT_STOP;
        }
        const [x, y] = event.get_coords();
        this._cursor = this._toPhysical(x, y);
        if (this._mode === Mode.WINDOW) {
            this._hovered = this._windowAt(x, y);
            if (this._hovered)
                this._finish({mode: Mode.WINDOW, window: this._hovered});
            return Clutter.EVENT_STOP;
        }
        if (this._mode === Mode.SCREEN) {
            this._pickScreen(false);
            return Clutter.EVENT_STOP;
        }
        this._selector.press(this._cursor);
        this._checkRegionDone();
        return Clutter.EVENT_STOP;
    }

    vfunc_button_release_event(event) {
        if (event.get_button() !== Clutter.BUTTON_PRIMARY || this._mode !== Mode.REGION)
            return Clutter.EVENT_STOP;
        const [x, y] = event.get_coords();
        this._cursor = this._toPhysical(x, y);
        this._selector.release(this._cursor);
        this._checkRegionDone();
        return Clutter.EVENT_STOP;
    }

    vfunc_key_press_event(event) {
        const key = event.get_key_symbol();
        const step = event.get_state() & Clutter.ModifierType.CONTROL_MASK ? 10 : 1;
        switch (key) {
        case Clutter.KEY_Escape:
            this._finish(null);
            break;
        case Clutter.KEY_Return:
        case Clutter.KEY_KP_Enter:
        case Clutter.KEY_space:
            if (this._mode === Mode.WINDOW) {
                if (this._hovered)
                    this._finish({mode: Mode.WINDOW, window: this._hovered});
            } else if (this._mode === Mode.SCREEN) {
                this._pickScreen(true); // Enter: all monitors together
            } else {
                this._selector.click(this._cursor);
                this._checkRegionDone();
            }
            break;
        case Clutter.KEY_BackSpace:
        case Clutter.KEY_Delete:
            this._selector.discard();
            this._sync();
            break;
        case Clutter.KEY_w:
        case Clutter.KEY_W:
            if (this._mode !== Mode.WINDOW)
                this._setMode(Mode.WINDOW);
            break;
        case Clutter.KEY_r:
        case Clutter.KEY_R:
            if (this._mode !== Mode.REGION)
                this._setMode(Mode.REGION);
            break;
        case Clutter.KEY_s:
        case Clutter.KEY_S:
            if (this._mode !== Mode.SCREEN)
                this._setMode(Mode.SCREEN);
            break;
        case Clutter.KEY_F1:
            this._hint.visible = !this._hint.visible;
            break;
        case Clutter.KEY_Left:
            this._moveCursorBy(-step, 0);
            break;
        case Clutter.KEY_Right:
            this._moveCursorBy(step, 0);
            break;
        case Clutter.KEY_Up:
            this._moveCursorBy(0, -step);
            break;
        case Clutter.KEY_Down:
            this._moveCursorBy(0, step);
            break;
        }
        return Clutter.EVENT_STOP;
    }

    // Arrow keys: one physical pixel per press (F-47), the real pointer follows.
    _moveCursorBy(dx, dy) {
        const [w, h] = this._textureSize;
        const px = Math.min(Math.max(this._cursor[0] + dx, 0), w - 1);
        const py = Math.min(Math.max(this._cursor[1] + dy, 0), h - 1);
        this._cursor = [px, py];
        const x = Math.floor((px + 0.5) / this._scale);
        const y = Math.floor((py + 0.5) / this._scale);
        if (this._mode === Mode.WINDOW)
            this._hovered = this._windowAt(x, y);
        else
            this._selector.move(this._cursor);
        this._ignoreMotionAt = [x, y];
        Clutter.get_default_backend().get_default_seat().warp_pointer(x, y);
        this._sync();
    }

    // The monitor (logical rectangle) under the selection point.
    _monitorAtCursor() {
        const [x, y] = this._cursor.map(v => (v + 0.5) / this._scale);
        return Main.layoutManager.monitors.find(m => contains(m, x, y)) ?? null;
    }

    // One monitor, or all of them (the whole stage), in physical pixels.
    _pickScreen(all) {
        let rect;
        if (all) {
            rect = {x: 0, y: 0, width: this._textureSize[0], height: this._textureSize[1]};
        } else {
            const m = this._monitorAtCursor();
            if (!m)
                return;
            const scale = this._scale;
            rect = {
                x: Math.round(m.x * scale), y: Math.round(m.y * scale),
                width: Math.round(m.width * scale), height: Math.round(m.height * scale),
            };
        }
        this._finish({mode: Mode.SCREEN, region: rect});
    }

    _checkRegionDone() {
        if (this._selector.state === 'done')
            this._finish({mode: Mode.REGION, region: this._selector.region()});
        else
            this._sync();
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
 * @param {string} mode - Mode.WINDOW, Mode.REGION or Mode.SCREEN to start with
 * @param {string} how - RegionSelect.CLICKS or RegionSelect.DRAG
 * @returns {{promise: Promise<object|null>, cancel: Function}} resolves with
 *   {mode: 'window', window: entry} or {mode: 'region' or 'screen', region:
 *   rect in physical px}, or null
 */
export function select(frozen, mode, how = RegionSelect.CLICKS) {
    let overlay;
    const promise = new Promise(resolve => {
        overlay = new CaptureOverlay(frozen, mode, how, resolve);
        overlay.open();
    });
    return {promise, cancel: () => overlay.close()};
}
