// Countdown of a delayed capture (F-04, D-025): a small round display in
// the middle or a corner of the primary monitor, as the app asks. Only the
// display itself takes clicks (a click cancels), so the desktop stays
// usable meanwhile: menus can be opened, things hovered. With clicks off
// it takes none at all; they go to the window below it. Escape cancels
// through a keybinding that exists only while the countdown runs, because
// the shell does not see keys that go to an application window.

import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

const KEYBINDING = 'cancel-countdown';
// After the display is hidden, wait this long (ms) before the freeze, so the
// compositor has drawn a frame without it.
const SETTLE_MS = 150;
// Distance of a corner position from the edges of the work area (px),
// as in the app (CountdownOptions::kMargin).
const MARGIN = 24;

export const Position = Object.freeze({
    CENTER: 'center',
    TOP_LEFT: 'top-left',
    TOP_RIGHT: 'top-right',
    BOTTOM_LEFT: 'bottom-left',
    BOTTOM_RIGHT: 'bottom-right',
});

const CountdownDisplay = GObject.registerClass(
class CountdownDisplay extends St.BoxLayout {
    _init(hint, position, clickCancels, onClick) {
        // Not reactive: the shell's pick skips it, so clicks reach the window below.
        super._init({style_class: 'nsl-snap-countdown', vertical: true, reactive: clickCancels});
        this._position = position;
        this._onClick = onClick;
        this._number = new St.Label({style_class: 'nsl-snap-countdown-number', x_align: Clutter.ActorAlign.CENTER});
        this.add_child(this._number);
        this.add_child(new St.Label({
            style_class: 'nsl-snap-countdown-hint', x_align: Clutter.ActorAlign.CENTER,
            text: hint,
        }));
    }

    show(seconds) {
        this._number.text = `${seconds}`;
        const monitor = Main.layoutManager.primaryMonitor;
        const [, width] = this.get_preferred_width(-1);
        const [, height] = this.get_preferred_height(width);
        // Corners within the work area (without the top bar and docks).
        const area = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        const left = area.x + MARGIN;
        const right = area.x + area.width - MARGIN - width;
        const top = area.y + MARGIN;
        const bottom = area.y + area.height - MARGIN - height;
        let x = monitor.x + (monitor.width - width) / 2;
        let y = monitor.y + (monitor.height - height) / 2;
        switch (this._position) {
        case Position.TOP_LEFT: [x, y] = [left, top]; break;
        case Position.TOP_RIGHT: [x, y] = [right, top]; break;
        case Position.BOTTOM_LEFT: [x, y] = [left, bottom]; break;
        case Position.BOTTOM_RIGHT: [x, y] = [right, bottom]; break;
        }
        this.set_position(Math.round(x), Math.round(y));
        super.show();
    }

    vfunc_button_press_event() {
        this._onClick();
        return Clutter.EVENT_STOP;
    }
});

/**
 * Counts down, then hides itself and resolves with true; resolves with
 * false when the user cancels (Escape or a click on the display).
 *
 * @param {number} seconds - the delay
 * @param {Gio.Settings} settings - the extension's settings (keybinding)
 * @param {string} hint - the text under the number, from the app (D-038)
 * @param {string} position - one of Position
 * @param {boolean} clickCancels - false: clicks go through, only Escape cancels
 * @returns {{promise: Promise<boolean>, cancel: Function}}
 */
export function countdown(seconds, settings, hint, position = Position.CENTER, clickCancels = true) {
    let finish;
    const promise = new Promise(resolve => {
        let left = seconds;
        let timer = 0;
        let done = false;
        const display = new CountdownDisplay(hint, position, clickCancels, () => finish(false));
        Main.layoutManager.uiGroup.add_child(display);
        display.show(left);
        Main.wm.addKeybinding(KEYBINDING, settings, Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
            Shell.ActionMode.ALL, () => finish(false));

        finish = ok => {
            if (done)
                return;
            done = true;
            if (timer)
                GLib.source_remove(timer);
            Main.wm.removeKeybinding(KEYBINDING);
            display.destroy();
            if (!ok) {
                resolve(false);
                return;
            }
            GLib.timeout_add(GLib.PRIORITY_DEFAULT, SETTLE_MS, () => {
                resolve(true);
                return GLib.SOURCE_REMOVE;
            });
        };

        timer = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 1000, () => {
            left -= 1;
            if (left <= 0) {
                timer = 0;
                finish(true);
                return GLib.SOURCE_REMOVE;
            }
            display.show(left);
            return GLib.SOURCE_CONTINUE;
        });
    });
    return {promise, cancel: () => finish(false)};
}
