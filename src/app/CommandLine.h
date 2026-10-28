#pragma once

#include "capture/CaptureResult.h"

#include <QString>
#include <QStringList>

#include <optional>

// The app's command line (F-48, D-034), e.g. for a key binding in
// hyprland.conf:
//   nsl-snap --capture-region            capture, save to the save folder
//   nsl-snap --capture-window --open     capture, show it in the editor
// Without a capture option the app starts as usual.
namespace CommandLine {

struct Request
{
    std::optional<CaptureResult::Kind> capture; // none: just start (or show) the app
    bool open = false;  // show the capture in the editor instead of saving it
    int delay = 0;      // seconds of countdown before the freeze (F-04)
};

// What a request from the command line ended in.
enum class Outcome { Saved, Opened, Cancelled, Failed };

// The exit code of the `nsl-snap` call: 0 saved or opened, 1 cancelled by
// the user, 2 failed (also for wrong arguments).
int exitCode(Outcome outcome);

struct Parsed
{
    Request request;
    QString error;        // not empty: the arguments were wrong
    bool showHelp = false;
    bool showVersion = false;
};

// `arguments` as QCoreApplication::arguments() has them, the program first.
Parsed parse(const QStringList &arguments);

// The text for --help.
QString helpText();

} // namespace CommandLine
