// NSL Snap capture backend for GNOME Shell (docs/01-architecture.md).
//
// Exports ch.netslash.Snap1 on the session bus. Every capture requires the
// user to pick the window interactively, so no D-Bus caller can grab window
// contents silently (docs/02-decisions.md, D-003).

import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

import {pickWindow} from './picker.js';

Gio._promisify(Shell.Screenshot, 'composite_to_stream');

const BUS_NAME = 'ch.netslash.Snap.Shell';
const OBJECT_PATH = '/ch/netslash/Snap';
const ERROR_PREFIX = 'ch.netslash.Snap1.Error';
const API_VERSION = 1;

const IFACE_XML = `
<node>
  <interface name="ch.netslash.Snap1">
    <!--
      options:
        crop (s): "frame" (default) crops to the visible window,
                  "buffer" keeps client-side shadow margins.
      png:  the capture as PNG with alpha channel.
      info: title, app-id, wm-class, frame-rect, buffer-rect (logical px),
            scale, width, height (image px).
    -->
    <method name="PickWindow">
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="ay" name="png" direction="out"/>
      <arg type="a{sv}" name="info" direction="out"/>
    </method>
    <property name="Version" type="u" access="read"/>
  </interface>
</node>`;

function rectVariant(r) {
    return new GLib.Variant('(iiii)', [r.x, r.y, r.width, r.height]);
}

/**
 * Renders a window offscreen, like GNOME's own screenshot UI does, and
 * encodes it as PNG. Overlapping windows do not affect the result.
 *
 * @param {Meta.Window} window - the window to capture
 * @param {string} crop - "frame" or "buffer" (D-005)
 * @returns {Promise<[GLib.Bytes, object]>} PNG bytes and info dictionary
 */
async function captureWindow(window, crop) {
    const actor = window.get_compositor_private();
    if (!actor)
        throw new Error('Window has no actor anymore');

    const content = actor.paint_to_content(null);
    const texture = content.get_texture();
    const texWidth = texture.get_width();
    const texHeight = texture.get_height();
    const scale = actor.get_resource_scale();
    const frame = window.get_frame_rect();
    const buffer = window.get_buffer_rect();

    let [x, y, width, height] = [0, 0, texWidth, texHeight];
    if (crop === 'frame') {
        // The texture starts at the buffer origin and is in buffer pixels.
        x = Math.max(0, Math.round((frame.x - buffer.x) * scale));
        y = Math.max(0, Math.round((frame.y - buffer.y) * scale));
        width = Math.min(Math.round(frame.width * scale), texWidth - x);
        height = Math.min(Math.round(frame.height * scale), texHeight - y);
    }

    const stream = Gio.MemoryOutputStream.new_resizable();
    await Shell.Screenshot.composite_to_stream(
        texture, x, y, width, height, scale,
        null, 0, 0, 1,
        stream);
    stream.close(null);

    const info = {
        'title': GLib.Variant.new_string(window.get_title() ?? ''),
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
    return [stream.steal_as_bytes(), info];
}

class SnapService {
    constructor() {
        this._dbus = Gio.DBusExportedObject.wrapJSObject(IFACE_XML, this);
        this._dbus.export(Gio.DBus.session, OBJECT_PATH);
        this._nameId = Gio.bus_own_name_on_connection(
            Gio.DBus.session, BUS_NAME, Gio.BusNameOwnerFlags.NONE, null, null);
        this._pick = null;
    }

    get Version() {
        return API_VERSION;
    }

    async PickWindowAsync(_params, invocation) {
        if (this._pick) {
            invocation.return_dbus_error(`${ERROR_PREFIX}.Busy`, 'A pick is already in progress');
            return;
        }

        const [options] = invocation.get_parameters().recursiveUnpack();
        const crop = options.crop ?? 'frame';
        if (crop !== 'frame' && crop !== 'buffer') {
            invocation.return_dbus_error(`${ERROR_PREFIX}.InvalidArgs`, `Unknown crop "${crop}"`);
            return;
        }

        let window;
        try {
            this._pick = pickWindow();
            window = await this._pick.promise;
        } finally {
            this._pick = null;
        }

        if (!window) {
            invocation.return_dbus_error(`${ERROR_PREFIX}.Cancelled`, 'Cancelled by the user');
            return;
        }

        try {
            const [png, info] = await captureWindow(window, crop);
            invocation.return_value(GLib.Variant.new_tuple([
                GLib.Variant.new_from_bytes(new GLib.VariantType('ay'), png, true),
                new GLib.Variant('a{sv}', info),
            ]));
        } catch (e) {
            logError(e, 'NSL Snap: capture failed');
            invocation.return_dbus_error(`${ERROR_PREFIX}.Failed`, e.message);
        }
    }

    destroy() {
        this._pick?.cancel();
        this._pick = null;
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
