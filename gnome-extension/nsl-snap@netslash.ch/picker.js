// Interactive window picker: highlights the window under the pointer and
// resolves with its MetaWindow on click, or null on Escape / right click.

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

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

function windowAt(x, y) {
    // get_window_actors() is in stacking order, bottom first.
    const actors = global.get_window_actors().slice().reverse();
    for (const actor of actors) {
        const window = actor.meta_window;
        if (!isPickable(window))
            continue;
        const r = window.get_frame_rect();
        if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height)
            return window;
    }
    return null;
}

const PickerOverlay = GObject.registerClass(
class PickerOverlay extends St.Widget {
    _init(resolve) {
        super._init({reactive: true, can_focus: true});
        this._resolve = resolve;
        this._window = null;

        this.add_constraint(new Clutter.BindConstraint({
            source: global.stage,
            coordinate: Clutter.BindCoordinate.ALL,
        }));

        this._highlight = new St.Widget({style_class: 'nsl-snap-highlight', visible: false});
        this.add_child(this._highlight);
    }

    open() {
        Main.uiGroup.add_child(this);
        this._grab = Main.pushModal(this);
        global.display.set_cursor(Meta.Cursor.CROSSHAIR);
        const [x, y] = global.get_pointer();
        this._update(x, y);
    }

    vfunc_motion_event(event) {
        const [x, y] = event.get_coords();
        this._update(x, y);
        return Clutter.EVENT_STOP;
    }

    vfunc_button_press_event(event) {
        if (event.get_button() === Clutter.BUTTON_PRIMARY) {
            const [x, y] = event.get_coords();
            this._update(x, y);
            this._finish(this._window);
        } else {
            this._finish(null);
        }
        return Clutter.EVENT_STOP;
    }

    vfunc_key_press_event(event) {
        const key = event.get_key_symbol();
        if (key === Clutter.KEY_Escape)
            this._finish(null);
        else if (key === Clutter.KEY_Return || key === Clutter.KEY_KP_Enter || key === Clutter.KEY_space)
            this._finish(this._window);
        return Clutter.EVENT_STOP;
    }

    _update(x, y) {
        this._window = windowAt(x, y);
        if (!this._window) {
            this._highlight.hide();
            return;
        }
        const r = this._window.get_frame_rect();
        this._highlight.set_position(r.x, r.y);
        this._highlight.set_size(r.width, r.height);
        this._highlight.show();
    }

    _finish(window) {
        if (!this._resolve)
            return;
        const resolve = this._resolve;
        this._resolve = null;
        this.close();
        resolve(window);
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
 * Lets the user pick a window on the active workspace.
 *
 * @returns {{promise: Promise<Meta.Window|null>, cancel: Function}}
 */
export function pickWindow() {
    let overlay;
    const promise = new Promise(resolve => {
        overlay = new PickerOverlay(resolve);
        overlay.open();
    });
    return {promise, cancel: () => overlay.close()};
}
