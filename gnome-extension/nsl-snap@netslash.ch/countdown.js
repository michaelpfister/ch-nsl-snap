// Countdown of a delayed capture (F-04, D-025): a small round display in
// the middle of the primary monitor. Only the display itself takes clicks
// (a click cancels), so the desktop stays usable meanwhile: menus can be
// opened, things hovered. Escape cancels through a keybinding that exists
// only while the countdown runs, because the shell does not see keys that
// go to an application window.

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

const CountdownDisplay = GObject.registerClass(
class CountdownDisplay extends St.BoxLayout {
    _init(onClick) {
        super._init({style_class: 'nsl-snap-countdown', vertical: true, reactive: true});
        this._onClick = onClick;
        this._number = new St.Label({style_class: 'nsl-snap-countdown-number', x_align: Clutter.ActorAlign.CENTER});
        this.add_child(this._number);
        this.add_child(new St.Label({
            style_class: 'nsl-snap-countdown-hint', x_align: Clutter.ActorAlign.CENTER,
            text: 'Esc oder Klick: Abbrechen',
        }));
    }

    show(seconds) {
        this._number.text = `${seconds}`;
        const monitor = Main.layoutManager.primaryMonitor;
        const [, width] = this.get_preferred_width(-1);
        const [, height] = this.get_preferred_height(width);
        this.set_position(Math.round(monitor.x + (monitor.width - width) / 2),
            Math.round(monitor.y + (monitor.height - height) / 2));
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
 * @returns {{promise: Promise<boolean>, cancel: Function}}
 */
export function countdown(seconds, settings) {
    let finish;
    const promise = new Promise(resolve => {
        let left = seconds;
        let timer = 0;
        let done = false;
        const display = new CountdownDisplay(() => finish(false));
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
