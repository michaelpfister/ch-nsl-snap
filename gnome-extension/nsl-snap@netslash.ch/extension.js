// NSL Snap capture backend for GNOME Shell (docs/01-architecture.md).
//
// Exports ch.netslash.Snap1 on the session bus. Every capture freezes the
// screen and then requires the user to select on the frozen picture, so no
// D-Bus caller can grab screen contents silently (docs/02-decisions.md,
// D-003, D-011).

import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

import {Mode, RegionSelect, freeze, select} from './overlay.js';

Gio._promisify(Shell.Screenshot, 'composite_to_stream');
Gio._promisify(Shell.Screenshot.prototype, 'screenshot_stage_to_content');

const BUS_NAME = 'ch.netslash.Snap.Shell';
const OBJECT_PATH = '/ch/netslash/Snap';
const ERROR_PREFIX = 'ch.netslash.Snap1.Error';
// 2: Capture with window and region mode, both on a frozen screen.
// 3: region by two clicks or a drag (region-select), with a magnifier.
const API_VERSION = 3;

const IFACE_XML = `
<node>
  <interface name="ch.netslash.Snap1">
    <!--
      Freezes the screen, shows the capture overlay and returns what the
      user selected. W and R switch between window and region in it.
      options:
        mode (s): "window" (default) or "region": the mode the overlay
                  starts in.
        crop (s): window captures only. "frame" (default) crops to the
                  visible window, "buffer" keeps client-side shadow margins.
        region-select (s): "clicks" (default): two clicks set the corners;
                  "drag": press, drag, release (D-023).
      png:  the capture as PNG with alpha channel.
      info: mode ("window" or "region"), scale, width, height (image px);
            window: title, app-id, wm-class, frame-rect, buffer-rect, crop;
            region: rect (logical px), pixels (physical px).
    -->
    <method name="Capture">
      <arg type="a{sv}" name="options" direction="in"/>
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
async function encodeRegion(frozen, region) {
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
        'mode': GLib.Variant.new_string(Mode.REGION),
        'rect': rectVariant(logical),
        'pixels': rectVariant({x, y, width, height}),
        'scale': GLib.Variant.new_double(scale),
        'width': GLib.Variant.new_int32(width),
        'height': GLib.Variant.new_int32(height),
    };
    return [png, info];
}

class SnapService {
    constructor() {
        this._dbus = Gio.DBusExportedObject.wrapJSObject(IFACE_XML, this);
        this._dbus.export(Gio.DBus.session, OBJECT_PATH);
        this._nameId = Gio.bus_own_name_on_connection(
            Gio.DBus.session, BUS_NAME, Gio.BusNameOwnerFlags.NONE, null, null);
        this._selection = null;
        this._busy = false;
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

    async _capture(options) {
        if (this._busy)
            throw new CaptureError('Busy', 'A capture is already in progress');
        const mode = options.mode ?? Mode.WINDOW;
        const crop = options.crop ?? 'frame';
        const how = options['region-select'] ?? RegionSelect.CLICKS;
        if (mode !== Mode.WINDOW && mode !== Mode.REGION)
            throw new CaptureError('InvalidArgs', `Unknown mode "${mode}"`);
        if (crop !== 'frame' && crop !== 'buffer')
            throw new CaptureError('InvalidArgs', `Unknown crop "${crop}"`);
        if (how !== RegionSelect.CLICKS && how !== RegionSelect.DRAG)
            throw new CaptureError('InvalidArgs', `Unknown region-select "${how}"`);

        this._busy = true;
        try {
            // Freeze first, then select on the frozen picture (D-011).
            const frozen = await freeze();
            this._selection = select(frozen, mode, how);
            const result = await this._selection.promise;
            this._selection = null;
            if (!result)
                throw new CaptureError('Cancelled', 'Cancelled by the user');
            if (result.mode === Mode.WINDOW)
                return await encodeWindow(result.window, crop);
            return await encodeRegion(frozen, result.region);
        } finally {
            this._selection = null;
            this._busy = false;
        }
    }

    destroy() {
        this._selection?.cancel();
        this._selection = null;
        Gio.bus_unown_name(this._nameId);
        this._dbus.unexport();
    }
}

export default class NslSnapExtension extends Extension {
    enable() {
        this._service = new SnapService();
    }

    disable() {
        this._service?.destroy();
        this._service = null;
    }
}
