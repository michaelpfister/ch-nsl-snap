// NSL Snap capture backend for GNOME Shell (docs/01-architecture.md).
//
// Exports ch.netslash.Snap1 on the session bus. Every capture freezes the
// screen and then requires the user to select on the frozen picture, so no
// D-Bus caller can grab screen contents silently (docs/02-decisions.md,
// D-003, D-011). The global hotkeys (F-42, D-031) are a key press of the
// user, so they may capture the active window or a monitor directly; the
// result is kept for the app, which is woken over D-Bus and takes it, and
// only the installed app gets it.

import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

import {countdown} from './countdown.js';
import {Hotkey, Hotkeys} from './hotkeys.js';
import {Mode, RegionSelect, freeze, select} from './overlay.js';

Gio._promisify(Shell.Screenshot, 'composite_to_stream');
Gio._promisify(Shell.Screenshot.prototype, 'screenshot_stage_to_content');
Gio._promisify(Gio.DBusConnection.prototype, 'call');

const BUS_NAME = 'ch.netslash.Snap.Shell';
const OBJECT_PATH = '/ch/netslash/Snap';
const ERROR_PREFIX = 'ch.netslash.Snap1.Error';
// 2: Capture with window and region mode, both on a frozen screen.
// 3: region by two clicks or a drag (region-select), with a magnifier.
// 4: screen mode (F-01).
// 5: delay with a countdown (F-04).
// 6: global hotkeys (F-42): Configure, TakeCapture.
const API_VERSION = 6;

// The app (src/app/AppService.cpp): woken when a hotkey capture is ready.
const APP_NAME = 'ch.netslash.Snap';
const APP_PATH = '/ch/netslash/Snap';
const APP_IFACE = 'ch.netslash.Snap.App1';
// A hotkey capture the app has not taken is forgotten after this time.
const PENDING_SECONDS = 60;

const IFACE_XML = `
<node>
  <interface name="ch.netslash.Snap1">
    <!--
      Freezes the screen, shows the capture overlay and returns what the
      user selected. W and R switch between window and region in it.
      options:
        mode (s): "window" (default), "region" or "screen": the mode the
                  overlay starts in (W, R, S switch in it).
        crop (s): window captures only. "frame" (default) crops to the
                  visible window, "buffer" keeps client-side shadow margins.
        region-select (s): "clicks" (default): two clicks set the corners;
                  "drag": press, drag, release (D-023).
        delay (u): seconds of countdown before the freeze (F-04); 0 (default)
                  freezes at once. Escape or a click on the countdown
                  cancels.
      png:  the capture as PNG with alpha channel.
      info: mode ("window", "region" or "screen"), scale, width, height
            (image px); screen: one monitor or all (rect, pixels as region);
            window: title, app-id, wm-class, frame-rect, buffer-rect, crop;
            region: rect (logical px), pixels (physical px).
    -->
    <method name="Capture">
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="ay" name="png" direction="out"/>
      <arg type="a{sv}" name="info" direction="out"/>
    </method>
    <!--
      Preferences of the app for hotkey captures (F-42), kept in the
      extension's settings so they also apply while the app is not running.
      options (all optional): delay (u), region-select (s), last-mode (s),
        hotkeys-enabled (b), take-over-system-keys (b), capture-last,
        capture-screen, capture-window, capture-region (as: accelerators
        such as "<Shift>Print"; empty = off).
    -->
    <method name="Configure">
      <arg type="a{sv}" name="options" direction="in"/>
    </method>
    <!--
      The capture of the last hotkey, once. Only for the app whose
      executable is set in the settings (app-executable); others get
      AccessDenied. NoCapture if there is none (or it is too old).
    -->
    <method name="TakeCapture">
      <arg type="ay" name="png" direction="out"/>
      <arg type="a{sv}" name="info" direction="out"/>
    </method>
    <!-- Version 1 API: Capture in window mode (PoC client). -->
    <method name="PickWindow">
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="ay" name="png" direction="out"/>
      <arg type="a{sv}" name="info" direction="out"/>
    </method>
    <property name="Version" type="u" access="read"/>
  </interface>
</node>`;

class CaptureError extends Error {
    constructor(name, message) {
        super(message);
        this.dbusName = `${ERROR_PREFIX}.${name}`;
    }
}

function rectVariant(r) {
    return new GLib.Variant('(iiii)', [r.x, r.y, r.width, r.height]);
}

async function encode(texture, x, y, width, height, scale) {
    const stream = Gio.MemoryOutputStream.new_resizable();
    // No cursor texture: the pointer is never part of a capture (F-05).
    await Shell.Screenshot.composite_to_stream(
        texture, x, y, width, height, scale,
        null, 0, 0, 1,
        stream);
    stream.close(null);
    return stream.steal_as_bytes();
}

/**
 * Encodes a window as it was at the freeze (rendered offscreen, so
 * overlapping windows do not matter).
 *
 * @param {object} entry - a window from freeze()
 * @param {string} crop - "frame" or "buffer" (D-005)
 * @returns {Promise<[GLib.Bytes, object]>} PNG bytes and info dictionary
 */
async function encodeWindow(entry, crop) {
    const texture = entry.content.get_texture();
    const texWidth = texture.get_width();
    const texHeight = texture.get_height();
    const {scale, frameRect: frame, bufferRect: buffer} = entry;

    let [x, y, width, height] = [0, 0, texWidth, texHeight];
    if (crop === 'frame') {
        // The texture starts at the buffer origin and is in buffer pixels.
        x = Math.max(0, Math.round((frame.x - buffer.x) * scale));
        y = Math.max(0, Math.round((frame.y - buffer.y) * scale));
        width = Math.min(Math.round(frame.width * scale), texWidth - x);
        height = Math.min(Math.round(frame.height * scale), texHeight - y);
    }

    const png = await encode(texture, x, y, width, height, scale);
    const window = entry.window;
    const info = {
        'mode': GLib.Variant.new_string(Mode.WINDOW),
        'title': GLib.Variant.new_string(entry.title),
        'app-id': GLib.Variant.new_string(
            Shell.WindowTracker.get_default().get_window_app(window)?.get_id() ?? ''),
        'wm-class': GLib.Variant.new_string(window.get_wm_class() ?? ''),
        'frame-rect': rectVariant(frame),
        'buffer-rect': rectVariant(buffer),
        'scale': GLib.Variant.new_double(scale),
        'width': GLib.Variant.new_int32(width),
        'height': GLib.Variant.new_int32(height),
        'crop': GLib.Variant.new_string(crop),
    };
    return [png, info];
}

/**
 * Encodes a region of the frozen screen (F-03).
 *
 * @param {object} frozen - result of freeze()
 * @param {object} region - {x, y, width, height} in physical px (texture px)
 * @returns {Promise<[GLib.Bytes, object]>} PNG bytes and info dictionary
 */
async function encodeRegion(frozen, region, mode = Mode.REGION) {
    const texture = frozen.content.get_texture();
    const scale = frozen.scale;
    // Inside the texture.
    const x = Math.max(0, region.x);
    const y = Math.max(0, region.y);
    const width = Math.min(region.width, texture.get_width() - x);
    const height = Math.min(region.height, texture.get_height() - y);

    const png = await encode(texture, x, y, width, height, scale);
    const logical = {
        x: Math.round(x / scale), y: Math.round(y / scale),
        width: Math.round(width / scale), height: Math.round(height / scale),
    };
    const info = {
        'mode': GLib.Variant.new_string(mode),
        'rect': rectVariant(logical),
        'pixels': rectVariant({x, y, width, height}),
        'scale': GLib.Variant.new_double(scale),
        'width': GLib.Variant.new_int32(width),
        'height': GLib.Variant.new_int32(height),
    };
    return [png, info];
}

/**
 * A selection without the overlay, for the hotkeys: the window that had
 * the focus at the freeze, or the monitor under the pointer. Null if there
 * is none (e.g. the desktop had the focus): then the overlay is shown.
 *
 * @param {object} frozen - result of freeze()
 * @param {?string} direct - "focus-window", "pointer-monitor" or null
 * @param {?Meta.Window} focus - the focused window at the freeze
 * @returns {?object} a selection like the overlay's
 */
function directSelection(frozen, direct, focus) {
    if (direct === 'focus-window') {
        const entry = frozen.windows.find(e => e.window === focus);
        return entry ? {mode: Mode.WINDOW, window: entry} : null;
    }
    if (direct === 'pointer-monitor') {
        const [px, py] = global.get_pointer();
        const m = Main.layoutManager.monitors.find(
            r => px >= r.x && px < r.x + r.width && py >= r.y && py < r.y + r.height);
        if (!m)
            return null;
        const scale = frozen.scale;
        return {
            mode: Mode.SCREEN,
            region: {
                x: Math.round(m.x * scale), y: Math.round(m.y * scale),
                width: Math.round(m.width * scale), height: Math.round(m.height * scale),
            },
        };
    }
    return null;
}

class SnapService {
    constructor(settings) {
        this._settings = settings;
        this._countdown = null;
        this._dbus = Gio.DBusExportedObject.wrapJSObject(IFACE_XML, this);
        this._dbus.export(Gio.DBus.session, OBJECT_PATH);
        this._nameId = Gio.bus_own_name_on_connection(
            Gio.DBus.session, BUS_NAME, Gio.BusNameOwnerFlags.NONE, null, null);
        this._selection = null;
        this._busy = false;
        this._pending = null;
        this._hotkeys = new Hotkeys(settings, name => this._onHotkey(name));
    }

    get Version() {
        return API_VERSION;
    }

    CaptureAsync(_params, invocation) {
        const [options] = invocation.get_parameters().recursiveUnpack();
        this._reply(invocation, () => this._capture(options));
    }

    PickWindowAsync(_params, invocation) {
        const [options] = invocation.get_parameters().recursiveUnpack();
        this._reply(invocation, () => this._capture({...options, mode: Mode.WINDOW}));
    }

    ConfigureAsync(_params, invocation) {
        const [options] = invocation.get_parameters().recursiveUnpack();
        try {
            this._configure(options);
            invocation.return_value(null);
        } catch (e) {
            invocation.return_dbus_error(e.dbusName ?? `${ERROR_PREFIX}.Failed`, e.message);
        }
    }

    _configure(options) {
        // Checked first, so an invalid call changes nothing.
        const strings = {'region-select': Object.values(RegionSelect), 'last-mode': Object.values(Mode)};
        for (const [key, value] of Object.entries(options)) {
            if (key === 'delay') {
                if (!Number.isInteger(value) || value < 0 || value > 60)
                    throw new CaptureError('InvalidArgs', `Invalid delay ${value}`);
            } else if (key in strings) {
                if (!strings[key].includes(value))
                    throw new CaptureError('InvalidArgs', `Invalid ${key} "${value}"`);
            } else if (key === 'hotkeys-enabled' || key === 'take-over-system-keys') {
                if (typeof value !== 'boolean')
                    throw new CaptureError('InvalidArgs', `${key} must be a boolean`);
            } else if (Object.values(Hotkey).includes(key)) {
                if (!Array.isArray(value) || !value.every(a => typeof a === 'string'))
                    throw new CaptureError('InvalidArgs', `${key} must be a list of accelerators`);
            } else {
                throw new CaptureError('InvalidArgs', `Unknown option "${key}"`);
            }
        }
        for (const [key, value] of Object.entries(options)) {
            if (key === 'delay')
                this._settings.set_uint(key, value);
            else if (key in strings)
                this._settings.set_string(key, value);
            else if (typeof value === 'boolean')
                this._settings.set_boolean(key, value);
            else
                this._settings.set_strv(key, value.filter(a => a.length > 0));
        }
    }

    TakeCaptureAsync(_params, invocation) {
        this._takeCapture(invocation);
    }

    async _takeCapture(invocation) {
        try {
            const pid = await this._callerPid(invocation.get_sender());
            if (!this._isApp(pid))
                throw new CaptureError('AccessDenied', 'Only the NSL Snap app can take hotkey captures');
            const pending = this._pending;
            this._pending = null;
            if (!pending || GLib.get_monotonic_time() - pending.time > PENDING_SECONDS * 1e6)
                throw new CaptureError('NoCapture', 'There is no hotkey capture');
            invocation.return_value(GLib.Variant.new_tuple([
                GLib.Variant.new_from_bytes(new GLib.VariantType('ay'), pending.png, true),
                new GLib.Variant('a{sv}', pending.info),
            ]));
            // Started in the background, the app may not raise its window
            // itself (focus stealing prevention); the shell may.
            this._activateWindowOf(pid);
        } catch (e) {
            if (!(e instanceof CaptureError))
                logError(e, 'NSL Snap: TakeCapture failed');
            invocation.return_dbus_error(e.dbusName ?? `${ERROR_PREFIX}.Failed`, e.message);
        }
    }

    async _callerPid(sender) {
        const reply = await Gio.DBus.session.call(
            'org.freedesktop.DBus', '/org/freedesktop/DBus', 'org.freedesktop.DBus',
            'GetConnectionUnixProcessID', new GLib.Variant('(s)', [sender]),
            new GLib.VariantType('(u)'), Gio.DBusCallFlags.NONE, -1, null);
        return reply.deepUnpack()[0];
    }

    // True if the process runs the executable from the settings.
    _isApp(pid) {
        const expected = this._settings.get_string('app-executable');
        if (!expected)
            return false;
        try {
            // A rebuilt binary shows as "… (deleted)" while the old one runs.
            const exe = GLib.file_read_link(`/proc/${pid}/exe`).replace(/ \(deleted\)$/, '');
            let path = expected;
            try {
                path = GLib.file_read_link(expected); // one level of symlink
                if (!GLib.path_is_absolute(path))
                    path = GLib.build_filenamev([GLib.path_get_dirname(expected), path]);
            } catch {
                // not a symlink
            }
            return exe === GLib.canonicalize_filename(path, null);
        } catch (e) {
            logError(e, 'NSL Snap: could not check the caller');
            return false;
        }
    }

    _activateWindowOf(pid) {
        for (const ms of [300, 1000]) {
            GLib.timeout_add(GLib.PRIORITY_DEFAULT, ms, () => {
                const actor = global.get_window_actors().find(a => a.meta_window.get_pid() === pid);
                if (actor && !actor.meta_window.has_focus())
                    Main.activateWindow(actor.meta_window);
                return GLib.SOURCE_REMOVE;
            });
        }
    }

    // A global hotkey (F-42): capture with the stored preferences, keep the
    // result, wake the app.
    async _onHotkey(name) {
        if (this._busy)
            return;
        let mode = this._settings.get_string('last-mode');
        let direct = null;
        if (name === Hotkey.SCREEN) {
            mode = Mode.SCREEN;
            direct = 'pointer-monitor';
        } else if (name === Hotkey.WINDOW) {
            mode = Mode.WINDOW;
            direct = 'focus-window';
        } else if (name === Hotkey.REGION) {
            mode = Mode.REGION;
        }
        // The app hides its window first, so it is not in the frozen
        // picture (D-011); not started for it.
        const hidden = await this._askApp('HideForCapture', new GLib.VariantType('(b)'));
        if (hidden?.deepUnpack()[0])
            await new Promise(resolve => GLib.timeout_add(GLib.PRIORITY_DEFAULT, 250, () => {
                resolve();
                return GLib.SOURCE_REMOVE;
            }));
        try {
            const [png, info] = await this._capture({
                mode,
                crop: 'buffer', // the app crops windows itself (D-010)
                'region-select': this._settings.get_string('region-select'),
                delay: this._settings.get_uint('delay'),
            }, direct);
            this._pending = {png, info, time: GLib.get_monotonic_time()};
            this._wakeApp();
        } catch (e) {
            // The app shows its window again.
            this._askApp('CaptureCancelled', null);
            if (e instanceof CaptureError && e.dbusName.endsWith('.Cancelled'))
                return;
            logError(e, 'NSL Snap: hotkey capture failed');
            Main.notify('NSL Snap', `Die Aufnahme ist fehlgeschlagen: ${e.message}`);
        }
    }

    // Calls the app if it is running (no D-Bus activation); null otherwise.
    async _askApp(method, replyType) {
        try {
            return await Gio.DBus.session.call(APP_NAME, APP_PATH, APP_IFACE, method, null, replyType,
                Gio.DBusCallFlags.NO_AUTO_START, 1000, null);
        } catch {
            return null; // not running, or an older app
        }
    }

    _wakeApp() {
        // D-Bus activation starts the app if it is not running.
        Gio.DBus.session.call(APP_NAME, APP_PATH, APP_IFACE, 'CaptureReady', null, null,
            Gio.DBusCallFlags.NONE, 30000, null, (connection, result) => {
                try {
                    connection.call_finish(result);
                } catch (e) {
                    this._pending = null;
                    logError(e, 'NSL Snap: could not reach the app');
                    Main.notify('NSL Snap', 'Die App NSL Snap konnte nicht gestartet werden. ' +
                        'Ist sie installiert (scripts/dev-install.sh)?');
                }
            });
    }

    async _reply(invocation, capture) {
        try {
            const [png, info] = await capture();
            invocation.return_value(GLib.Variant.new_tuple([
                GLib.Variant.new_from_bytes(new GLib.VariantType('ay'), png, true),
                new GLib.Variant('a{sv}', info),
            ]));
        } catch (e) {
            if (e instanceof CaptureError) {
                invocation.return_dbus_error(e.dbusName, e.message);
            } else {
                logError(e, 'NSL Snap: capture failed');
                invocation.return_dbus_error(`${ERROR_PREFIX}.Failed`, e.message);
            }
        }
    }

    /**
     * Countdown, freeze and selection.
     *
     * @param {object} options - as for Capture
     * @param {?string} direct - for hotkeys: "focus-window" or
     *   "pointer-monitor" capture without the overlay; null shows it
     * @returns {Promise<[GLib.Bytes, object]>} PNG bytes and info dictionary
     */
    async _capture(options, direct = null) {
        if (this._busy)
            throw new CaptureError('Busy', 'A capture is already in progress');
        const mode = options.mode ?? Mode.WINDOW;
        const crop = options.crop ?? 'frame';
        const how = options['region-select'] ?? RegionSelect.CLICKS;
        const delay = options.delay ?? 0;
        if (!Object.values(Mode).includes(mode))
            throw new CaptureError('InvalidArgs', `Unknown mode "${mode}"`);
        if (crop !== 'frame' && crop !== 'buffer')
            throw new CaptureError('InvalidArgs', `Unknown crop "${crop}"`);
        if (how !== RegionSelect.CLICKS && how !== RegionSelect.DRAG)
            throw new CaptureError('InvalidArgs', `Unknown region-select "${how}"`);
        if (!Number.isInteger(delay) || delay < 0 || delay > 60)
            throw new CaptureError('InvalidArgs', `Invalid delay ${delay}`);

        this._busy = true;
        try {
            // After the countdown, if any: the countdown is gone before the
            // freeze (F-04).
            if (delay > 0) {
                this._countdown = countdown(delay, this._settings);
                const go = await this._countdown.promise;
                this._countdown = null;
                if (!go)
                    throw new CaptureError('Cancelled', 'Cancelled by the user');
            }
            // Freeze first, then select on the frozen picture (D-011).
            const focus = global.display.focus_window;
            const frozen = await freeze();
            let result = directSelection(frozen, direct, focus);
            if (!result) {
                this._selection = select(frozen, mode, how);
                result = await this._selection.promise;
                this._selection = null;
            }
            if (!result)
                throw new CaptureError('Cancelled', 'Cancelled by the user');
            // Print (F-42) starts in the mode used last.
            this._settings.set_string('last-mode', result.mode);
            if (result.mode === Mode.WINDOW)
                return await encodeWindow(result.window, crop);
            return await encodeRegion(frozen, result.region, result.mode);
        } finally {
            this._countdown = null;
            this._selection = null;
            this._busy = false;
        }
    }

    destroy() {
        this._hotkeys.destroy();
        this._hotkeys = null;
        this._pending = null;
        this._countdown?.cancel();
        this._countdown = null;
        this._selection?.cancel();
        this._selection = null;
        Gio.bus_unown_name(this._nameId);
        this._dbus.unexport();
    }
}

export default class NslSnapExtension extends Extension {
    enable() {
        this._service = new SnapService(this.getSettings());
    }

    disable() {
        this._service?.destroy();
        this._service = null;
    }
}
