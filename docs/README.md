# Project documentation

Concept, decisions and rules for **NSL Snap** (working title): a screenshot tool for **Windows** and **Linux (GNOME on Wayland)**, inspired by [WinSnap](https://www.ntwind.com/software/winsnap.html). Its key feature is capturing a single window cleanly: only that window, with clean edges and transparent rounded corners. No shadow is added; the instruction pages do that via CSS.

These files record **why** the tool is built the way it is. The code says how it works; this says which alternatives were rejected and on what grounds. Anyone wanting to change a decision a year from now should be able to read here what argued against it at the time.

## Conventions

- Documentation, code, identifiers and commit messages are in **English**.
- The user interface is in **German (Switzerland)**: no "ß" (assumption, see Q-02).

## Reading order

| File | Contents |
|---|---|
| [01-architecture.md](01-architecture.md) | The whole picture: components, platform backends, data flow |
| [02-decisions.md](02-decisions.md) | Every decision taken, with rationale and consequences |
| [03-platform-capabilities.md](03-platform-capabilities.md) | What each platform allows (Windows, GNOME/Wayland, KDE) and how we get at it |
| [04-features.md](04-features.md) | Feature scope: WinSnap reference and our own additions (still to be defined) |
| [05-open-questions.md](05-open-questions.md) | What is still undecided, and what it blocks |
| [06-poc-gnome-window-capture.md](06-poc-gnome-window-capture.md) | Proof of concept: window capture on GNOME via a Shell extension, how to run it, results |
