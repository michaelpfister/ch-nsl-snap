// Global capture hotkeys (F-42, D-031): registered with the window manager,
// so they work while any application has the focus. GNOME's own screenshot
// bindings for the same keys are taken out while the extension is enabled
// and put back on disable (Q-14).

import Gio from 'gi://Gio';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';

// Our keybindings (keys in the extension's schema) and what they do.
export const Hotkey = Object.freeze({
    LAST: 'capture-last',
    SCREEN: 'capture-screen',
    WINDOW: 'capture-window',
    REGION: 'capture-region',
});

const SYSTEM_SCHEMA = 'org.gnome.shell.keybindings';
const SYSTEM_KEYS = ['show-screenshot-ui', 'screenshot', 'screenshot-window'];

// "<Ctrl>" and "<Control>" mean the same; compare without case and with
// one spelling.
function normalize(accelerator) {
    return accelerator.toLowerCase().replaceAll('<ctrl>', '<control>').replaceAll('<primary>', '<control>');
}

export class Hotkeys {
    /**
     * @param {Gio.Settings} settings - the extension's settings
     * @param {Function} onPressed - called with a Hotkey value
     */
    constructor(settings, onPressed) {
        this._settings = settings;
        this._onPressed = onPressed;
        this._added = false;
        this._system = new Gio.Settings({schema_id: SYSTEM_SCHEMA});
        this._changedId = settings.connect('changed', (_s, key) => {
            if (key === 'hotkeys-enabled' || key === 'take-over-system-keys' ||
                Object.values(Hotkey).includes(key))
                this._sync();
        });
        this._sync();
    }

    _sync() {
        const enabled = this._settings.get_boolean('hotkeys-enabled');
        if (enabled && !this._added) {
            for (const name of Object.values(Hotkey)) {
                Main.wm.addKeybinding(name, this._settings, Meta.KeyBindingFlags.IGNORE_AUTOREPEAT,
                    Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW, () => this._onPressed(name));
            }
            this._added = true;
        } else if (!enabled && this._added) {
            this._removeBindings();
        }
        // Mutter follows changes of the accelerators by itself; GNOME's own
        // bindings have to be adjusted to the current ones.
        this._restoreSystemKeys();
        if (enabled && this._settings.get_boolean('take-over-system-keys'))
            this._takeOverSystemKeys();
    }

    _ours() {
        const ours = new Set();
        for (const name of Object.values(Hotkey)) {
            for (const accelerator of this._settings.get_strv(name))
                ours.add(normalize(accelerator));
        }
        return ours;
    }

    _takeOverSystemKeys() {
        const ours = this._ours();
        const saved = {};
        for (const key of SYSTEM_KEYS) {
            const current = this._system.get_strv(key);
            const kept = current.filter(a => !ours.has(normalize(a)));
            if (kept.length !== current.length) {
                saved[key] = current;
                this._system.set_strv(key, kept);
            }
        }
        if (Object.keys(saved).length > 0)
            this._settings.set_string('saved-system-keys', JSON.stringify(saved));
    }

    _restoreSystemKeys() {
        const text = this._settings.get_string('saved-system-keys');
        if (!text)
            return;
        try {
            const saved = JSON.parse(text);
            for (const key of SYSTEM_KEYS) {
                if (Array.isArray(saved[key]))
                    this._system.set_strv(key, saved[key]);
            }
        } catch (e) {
            logError(e, 'NSL Snap: could not restore GNOME screenshot keys');
        }
        this._settings.set_string('saved-system-keys', '');
    }

    _removeBindings() {
        for (const name of Object.values(Hotkey))
            Main.wm.removeKeybinding(name);
        this._added = false;
    }

    destroy() {
        this._settings.disconnect(this._changedId);
        if (this._added)
            this._removeBindings();
        this._restoreSystemKeys();
    }
}
