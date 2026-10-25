# Working on NSL Snap

Read [docs/README.md](docs/README.md) before changing behaviour. Decisions live in `docs/02-decisions.md`; when a change contradicts one, update the decision (or ask) instead of silently diverging. New open points go into `docs/05-open-questions.md`. Platform limits are in `docs/03-platform-capabilities.md`; check there before promising a feature on GNOME.

## Conventions

- Code, identifiers, comments, docs and commit messages: English.
- UI text: German (Switzerland), no "ß".
- App: C++20, Qt 6.8 Widgets, CMake (D-013). Write for C++ newcomers: modern C++, Qt idioms, no template tricks, comments where Qt or platform APIs are not obvious.
- The GNOME Shell extension stays thin: pick, capture, return image bytes. Logic belongs in the app (docs/01-architecture.md).
- Every capture method of the extension must require an interactive pick by the user. Never add a D-Bus method that lists windows or captures without user interaction (docs/02-decisions.md, D-003).
- No shadows, backgrounds or margins in captured images, neither from the system nor from the app; the instruction pages add shadows via CSS (D-004).
- Every capture freezes the screen first (after the countdown if delayed), then the user selects on the frozen picture (D-011).

## Development

- Setup and build commands: [docs/07-development-setup.md](docs/07-development-setup.md). Qt 6.8.3 lives in `~/Qt/6.8.3/gcc_64` (pass it as `CMAKE_PREFIX_PATH`); never use the distribution's Qt 6.4.
- GNOME Shell 46 on Wayland. The shell cannot be restarted in place: after installing or changing the extension, log out and in, or test in `scripts/nested-shell.sh`.
- `ctest --test-dir build` must pass and the build must have no warnings (`-Wall -Wextra`) before committing.
- Extension code is ES modules (GNOME 45+ format). Syntax check: copy to `.mjs` and run `node --check`.
- GNOME Shell's own sources are a good reference for APIs: `gresource extract /usr/lib/gnome-shell/libshell-14.so /org/gnome/shell/ui/screenshot.js`.
