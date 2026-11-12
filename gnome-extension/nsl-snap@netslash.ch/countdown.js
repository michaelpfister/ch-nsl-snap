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

import {DEFAULT_HINTS} from './overlay.js';

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

// The countdown's ring and the app's mark, the three strokes of the logo,
// which turns like an egg timer (D-043): each second a share of a full
// turn counterclockwise while the ring empties in step, blue turning red
// over the last half; in the last second the frame corners close like a
// shutter. Drawn as the app draws it (src/ui/LogoMark.cpp, Countdown.cpp).
const RING = 56;
const MARK = 28;
const TICK_MS = 450;
const WIND_UP_MS = 650;
const BLUE = [76, 141, 255];
const RED = [240, 82, 82];

// Ease out with a little spring past the end (as QEasingCurve::OutBack).
function easeOutBack(t, overshoot = 1.6) {
    const u = t - 1;
    return 1 + u * u * ((overshoot + 1) * u + overshoot);
}

function ringColor(rest) {
    const t = Math.min(1, Math.max(0, (0.5 - rest) / 0.5));
    return BLUE.map((v, i) => (v + (RED[i] - v) * t) / 255);
}

// The strokes of logo.svg, turned by `turn` degrees around the centre
// (negative: counterclockwise); the corners moved by `shut` towards it.
function drawMark(cr, size, turn, shut) {
    cr.save();
    cr.scale(size / 44, size / 44);
    cr.translate(-10, -10);
    cr.translate(32, 32);
    cr.rotate(turn * Math.PI / 180);
    cr.translate(-32, -32);
    cr.setSourceRGB(0xE9 / 255, 0xEC / 255, 0xEF / 255);
    cr.setLineWidth(5);
    cr.setLineCap(1); // round
    cr.setLineJoin(1); // round

    cr.save();
    cr.translate(shut, shut);
    cr.moveTo(15, 31);
    cr.lineTo(15, 18);
    cr.arc(18, 18, 3, Math.PI, 1.5 * Math.PI);
    cr.lineTo(31, 15);
    cr.stroke();
    cr.restore();

    cr.save();
    cr.translate(-shut, -shut);
    cr.moveTo(49, 33);
    cr.lineTo(49, 46);
    cr.arc(46, 46, 3, 0, 0.5 * Math.PI);
    cr.lineTo(33, 49);
    cr.stroke();
    cr.restore();

    cr.moveTo(23, 41);
    cr.lineTo(41, 23);
    cr.stroke();
    cr.restore();
}

const CountdownDisplay = GObject.registerClass(
class CountdownDisplay extends St.BoxLayout {
    _init(hints, position, clickCancels, onClick) {
        // Not reactive: the shell's pick skips it, so clicks reach the window below.
        super._init({style_class: 'nsl-snap-countdown', reactive: clickCancels});
        this._position = position;
        this._onClick = onClick;
        // Without the seconds (D-043); an older app's "%1 s" text is skipped.
        this._titleText = hints['countdown-title'].includes('%1')
            ? DEFAULT_HINTS['countdown-title'] : hints['countdown-title'];
        this._total = 1;
        this._rest = 1;     // shown now, 1 → 0
        this._shut = 0;     // corners moved in, in logo units
        this._timeline = null;

        this._ring = new St.DrawingArea({width: RING, height: RING, y_align: Clutter.ActorAlign.CENTER});
        this._ring.connect('repaint', area => this._paint(area));
        this.add_child(this._ring);
        const texts = new St.BoxLayout({vertical: true, y_align: Clutter.ActorAlign.CENTER});
        this._title = new St.Label({style_class: 'nsl-snap-countdown-title'});
        texts.add_child(this._title);
        texts.add_child(new St.Label({style_class: 'nsl-snap-countdown-hint', text: hints.countdown}));
        this.add_child(texts);
        this.connect('destroy', () => this._stopTimeline());
    }

    _paint(area) {
        const cr = area.get_context();
        const [width, height] = area.get_surface_size();
        const c = width / 2;
        const r = (Math.min(width, height) - 4) / 2;
        cr.setLineWidth(4);
        cr.setSourceRGB(0x3A / 255, 0x3F / 255, 0x46 / 255);
        cr.arc(c, height / 2, r, 0, 2 * Math.PI);
        cr.stroke();
        if (this._rest > 0) {
            // From the top, clockwise; it empties counterclockwise.
            cr.setSourceRGB(...ringColor(this._rest));
            cr.setLineCap(1);
            cr.arc(c, height / 2, r, -Math.PI / 2, -Math.PI / 2 + this._rest * 2 * Math.PI);
            cr.stroke();
        }
        cr.translate((width - MARK) / 2, (height - MARK) / 2);
        drawMark(cr, MARK, -360 * (1 - this._rest), this._shut);
        cr.$dispose();
    }

    _stopTimeline() {
        if (this._timeline) {
            this._timeline.stop();
            this._timeline = null;
        }
    }

    // Animates this._rest and this._shut with `curve` from their current
    // values; without animations (accessibility setting) it jumps.
    _animate(rest, shut, ms, curve, done) {
        this._stopTimeline();
        if (!St.Settings.get().enable_animations) {
            [this._rest, this._shut] = [rest, shut];
            this._ring.queue_repaint();
            done?.();
            return;
        }
        const [fromRest, fromShut] = [this._rest, this._shut];
        this._timeline = new Clutter.Timeline({actor: this, duration: ms});
        this._timeline.connect('new-frame', timeline => {
            const t = curve(timeline.get_progress());
            this._rest = Math.max(0, Math.min(1, fromRest + (rest - fromRest) * t));
            this._shut = fromShut + (shut - fromShut) * t;
            this._ring.queue_repaint();
        });
        this._timeline.connect('completed', () => {
            [this._rest, this._shut] = [rest, shut];
            this._ring.queue_repaint();
            this._timeline = null;
            done?.();
        });
        this._timeline.start();
    }

    // Shows the display empty and winds it up: the ring fills while the
    // mark turns back, with a spring; `done` when it is wound up.
    start(seconds, done) {
        this._total = Math.max(1, seconds);
        this._rest = 0;
        this._title.text = this._titleText;
        this._place();
        super.show();
        this._animate(1, 0, WIND_UP_MS, t => easeOutBack(t, 1.2), done);
    }

    // One second less: turn and ring together; `done` at the end of it.
    tick(left, done) {
        this._animate(left / this._total, 0, TICK_MS, t => easeOutBack(t), done);
    }

    // The shutter at the end: corners in and out again, then `done`.
    snap(done) {
        const t = t2 => t2 * (2 - t2);
        this._animate(this._rest, 6, 120, t, () => this._animate(this._rest, 0, 160, t, done));
    }

    _place() {
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
 * @param {object} hints - the texts from the app (D-038): countdown, countdown-title
 * @param {string} position - one of Position
 * @param {boolean} clickCancels - false: clicks go through, only Escape cancels
 * @returns {{promise: Promise<boolean>, cancel: Function}}
 */
export function countdown(seconds, settings, hints, position = Position.CENTER, clickCancels = true) {
    let finish;
    const promise = new Promise(resolve => {
        let left = seconds;
        let timer = 0;
        let done = false;
        const display = new CountdownDisplay(hints, position, clickCancels, () => finish(false));
        Main.layoutManager.uiGroup.add_child(display);
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

        // The animation leads, the seconds only orient (D-043): wound up
        // first, then a turn each second; the last turn runs to its end,
        // then the snap, then the capture.
        display.start(seconds, () => {
            if (done)
                return;
            timer = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 1000, () => {
                left -= 1;
                if (left > 0) {
                    display.tick(left);
                    return GLib.SOURCE_CONTINUE;
                }
                timer = 0;
                display.tick(0, () => {
                    if (!done)
                        display.snap(() => finish(true));
                });
                return GLib.SOURCE_REMOVE;
            });
        });
    });
    return {promise, cancel: () => finish(false)};
}
