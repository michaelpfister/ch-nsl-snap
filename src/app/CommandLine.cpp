#include "app/CommandLine.h"

#include <QCommandLineParser>
#include <QCoreApplication>

namespace CommandLine {

namespace {

constexpr int kMaxDelay = 60;

// The options, shared by parse() and helpText(). Command-line help is in
// English like the options themselves; the app's windows stay German.
void addOptions(QCommandLineParser &parser)
{
    parser.setApplicationDescription(QStringLiteral(
        "Snapslash: screenshots of single windows, regions and screens, with an editor.\n"
        "With a --capture option the capture starts at once; without --open it is saved\n"
        "to the save folder from the settings and its path printed. If Snapslash is\n"
        "already running, the running app does the capture.\n"
        "A --record option records a video (MP4, at most 10 minutes); the same command\n"
        "again, or the indicator on the screen, stops it and saves the whole video."));
    parser.addOptions({
        {QStringLiteral("capture-window"), QStringLiteral("Pick a window and capture it.")},
        {QStringLiteral("capture-region"), QStringLiteral("Select a region and capture it.")},
        {QStringLiteral("capture-screen"), QStringLiteral("Pick a monitor (Enter: all) and capture it.")},
        {QStringLiteral("record-window"), QStringLiteral("Pick a window and record a video of it (Hyprland, Windows).")},
        {QStringLiteral("record-screen"), QStringLiteral("Pick a monitor (Enter: all) and record a video of it.")},
        {QStringLiteral("open"), QStringLiteral("Show the capture in the editor (a video: in the trim window) instead of saving it.")},
        {QStringLiteral("delay"), QStringLiteral("Count down <seconds> before the capture (0-60)."),
         QStringLiteral("seconds"), QStringLiteral("0")},
    });
    parser.addHelpOption();
    parser.addVersionOption();
}

} // namespace

Parsed parse(const QStringList &arguments)
{
    Parsed parsed;
    QCommandLineParser parser;
    addOptions(parser);
    if (!parser.parse(arguments)) {
        parsed.error = parser.errorText();
        return parsed;
    }
    parsed.showHelp = parser.isSet(QStringLiteral("help")) || parser.isSet(QStringLiteral("help-all"));
    parsed.showVersion = parser.isSet(QStringLiteral("version"));
    if (!parser.positionalArguments().isEmpty()) {
        parsed.error = QStringLiteral("Unknown argument: %1").arg(parser.positionalArguments().first());
        return parsed;
    }

    int modes = 0;
    const std::pair<const char *, CaptureResult::Kind> options[] = {
        {"capture-window", CaptureResult::Kind::Window},
        {"capture-region", CaptureResult::Kind::Region},
        {"capture-screen", CaptureResult::Kind::Screen},
    };
    for (const auto &[name, kind] : options) {
        if (parser.isSet(QLatin1String(name))) {
            parsed.request.capture = kind;
            ++modes;
        }
    }
    const std::pair<const char *, RecordingBackend::Source> recordings[] = {
        {"record-window", RecordingBackend::Source::Window},
        {"record-screen", RecordingBackend::Source::Screen},
    };
    for (const auto &[name, source] : recordings) {
        if (parser.isSet(QLatin1String(name))) {
            parsed.request.record = source;
            ++modes;
        }
    }
    if (modes > 1) {
        parsed.error = QStringLiteral(
            "Only one of --capture-window, --capture-region, --capture-screen, --record-window and --record-screen.");
        return parsed;
    }
    parsed.request.open = parser.isSet(QStringLiteral("open"));

    bool ok = false;
    const int delay = parser.value(QStringLiteral("delay")).toInt(&ok);
    if (!ok || delay < 0 || delay > kMaxDelay) {
        parsed.error = QStringLiteral("--delay takes whole seconds from 0 to %1.").arg(kMaxDelay);
        return parsed;
    }
    parsed.request.delay = delay;
    return parsed;
}

int exitCode(Outcome outcome)
{
    switch (outcome) {
    case Outcome::Saved:
    case Outcome::Opened: return 0;
    case Outcome::Cancelled: return 1;
    case Outcome::Failed: break;
    }
    return 2;
}

QString helpText()
{
    QCommandLineParser parser;
    addOptions(parser);
    return parser.helpText();
}

} // namespace CommandLine
