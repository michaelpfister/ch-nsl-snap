#include "capture/gnome/GnomeShellBackend.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

#include <algorithm>
#include <limits>

namespace {

const QString kService = QStringLiteral("ch.netslash.Snap.Shell");
const QString kPath = QStringLiteral("/ch/netslash/Snap");
const QString kInterface = QStringLiteral("ch.netslash.Snap1");
// Version 2 added Capture with region mode, both on a frozen screen (D-011);
// version 4 the screen mode (F-01).
constexpr uint kCaptureVersion = 2;
constexpr uint kScreenVersion = 4;
// Version 5 the delay with a countdown (F-04).
constexpr uint kDelayVersion = 5;
// Version 6 the global hotkeys (F-42): Configure and TakeCapture.
constexpr uint kHotkeyVersion = 6;

QString modeName(CaptureResult::Kind kind)
{
    switch (kind) {
    case CaptureResult::Kind::Region: return QStringLiteral("region");
    case CaptureResult::Kind::Screen: return QStringLiteral("screen");
    case CaptureResult::Kind::Window: break;
    }
    return QStringLiteral("window");
}

// "frame-rect", "buffer-rect" and "rect" are D-Bus structs (iiii): x, y,
// width, height.
QRect rectFromVariant(const QVariant &value)
{
    if (!value.canConvert<QDBusArgument>())
        return {};
    const auto arg = value.value<QDBusArgument>();
    int x = 0, y = 0, width = 0, height = 0;
    arg.beginStructure();
    arg >> x >> y >> width >> height;
    arg.endStructure();
    return QRect(x, y, width, height);
}

} // namespace

GnomeShellBackend::GnomeShellBackend(QObject *parent)
    : CaptureBackend(parent)
{
}

bool GnomeShellBackend::isAvailable() const
{
    const auto *bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(kService);
}

QString GnomeShellBackend::unavailableReason() const
{
    return tr("Die GNOME-Erweiterung «NSL Snap» läuft nicht. "
              "Bitte installieren und aktivieren (docs/06-poc-gnome-window-capture.md) "
              "und danach ab- und wieder anmelden.");
}

uint GnomeShellBackend::extensionVersion() const
{
    // The "Version" property, read through the standard Properties interface.
    auto message = QDBusMessage::createMethodCall(kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                  QStringLiteral("Get"));
    message << kInterface << QStringLiteral("Version");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return 0;
    return reply.arguments().first().value<QDBusVariant>().variant().toUInt();
}

void GnomeShellBackend::captureWindow()
{
    capture(QStringLiteral("window"));
}

void GnomeShellBackend::captureRegion()
{
    capture(QStringLiteral("region"));
}

void GnomeShellBackend::captureScreen()
{
    capture(QStringLiteral("screen"));
}

void GnomeShellBackend::capture(const QString &mode)
{
    if (m_busy)
        return;
    if (!isAvailable()) {
        emit failed(unavailableReason());
        return;
    }
    // An extension from before the update still runs until the next login:
    // window capture works with its old method, region capture does not.
    const uint version = extensionVersion();
    const bool hasCapture = version >= kCaptureVersion;
    if (mode == QLatin1String("screen") && version < kScreenVersion) {
        emit failed(tr("Die GNOME-Erweiterung «NSL Snap» ist veraltet und kennt die Bildschirmaufnahme noch nicht. "
                       "Bitte ab- und wieder anmelden, damit die neue Version geladen wird."));
        return;
    }
    if (delay() > 0 && version < kDelayVersion) {
        emit failed(tr("Die GNOME-Erweiterung «NSL Snap» ist veraltet und kennt die verzögerte Aufnahme noch nicht. "
                       "Bitte ab- und wieder anmelden, damit die neue Version geladen wird."));
        return;
    }
    if (!hasCapture && mode != QLatin1String("window")) {
        emit failed(tr("Die GNOME-Erweiterung «NSL Snap» ist veraltet und kennt die Bereichsaufnahme noch nicht. "
                       "Bitte ab- und wieder anmelden, damit die neue Version geladen wird."));
        return;
    }
    m_busy = true;

    // The app crops windows itself (D-010), so it always asks for the whole buffer.
    QVariantMap options;
    options.insert(QStringLiteral("crop"), QStringLiteral("buffer"));
    if (hasCapture) {
        options.insert(QStringLiteral("mode"), mode);
        // How a region is selected (D-023); older extensions ignore it.
        options.insert(QStringLiteral("region-select"),
                       regionByDragging() ? QStringLiteral("drag") : QStringLiteral("clicks"));
        if (delay() > 0)
            options.insert(QStringLiteral("delay"), QVariant::fromValue(uint(delay()))); // D-Bus type u
    }

    auto message = QDBusMessage::createMethodCall(kService, kPath, kInterface,
                                                  hasCapture ? QStringLiteral("Capture") : QStringLiteral("PickWindow"));
    message << options;

    // No timeout: the user may take as long as they like to select.
    const auto call = QDBusConnection::sessionBus().asyncCall(message, std::numeric_limits<int>::max());
    auto *watcher = new QDBusPendingCallWatcher(call, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        m_busy = false;
        handleCaptureReply(w->reply(), false);
    });
}

void GnomeShellBackend::handleCaptureReply(const QDBusMessage &message, bool fromHotkey)
{
    if (message.type() == QDBusMessage::ErrorMessage) {
        const QString name = message.errorName();
        if (name.endsWith(QLatin1String(".Cancelled")) || (fromHotkey && name.endsWith(QLatin1String(".NoCapture")))) {
            if (!fromHotkey)
                emit cancelled();
        } else if (name.endsWith(QLatin1String(".AccessDenied"))) {
            emit failed(tr("Die GNOME-Erweiterung gibt die Aufnahme nur der installierten App. "
                           "Bitte scripts/dev-install.sh ausführen, damit sie diese App kennt."));
        } else {
            emit failed(message.errorMessage());
        }
        return;
    }
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2) {
        emit failed(tr("Die Aufnahme konnte nicht gelesen werden."));
        return;
    }
    // The info dictionary arrives as a map or, depending on its contents,
    // as a QDBusArgument still to be read.
    QVariantMap info;
    if (arguments.at(1).canConvert<QDBusArgument>())
        arguments.at(1).value<QDBusArgument>() >> info;
    else
        info = arguments.at(1).toMap();
    const CaptureResult result = resultFromReply(arguments.at(0).toByteArray(), info);
    if (result.image.isNull()) {
        emit failed(tr("Die Aufnahme konnte nicht gelesen werden."));
        return;
    }
    emit captured(result);
}

QVariantMap GnomeShellBackend::configureOptions(const HotkeyOptions &options)
{
    const GlobalHotkeys &keys = options.hotkeys;
    const auto accelerators = [](const QKeySequence &key) {
        const QString accelerator = HotkeyKeys::gnomeAccelerator(key);
        return accelerator.isEmpty() ? QStringList() : QStringList{accelerator};
    };
    QVariantMap map;
    map.insert(QStringLiteral("delay"), QVariant::fromValue(uint(std::max(0, options.delay)))); // D-Bus type u
    map.insert(QStringLiteral("region-select"),
               options.regionByDragging ? QStringLiteral("drag") : QStringLiteral("clicks"));
    map.insert(QStringLiteral("last-mode"), modeName(options.lastMode));
    map.insert(QStringLiteral("hotkeys-enabled"), keys.enabled);
    map.insert(QStringLiteral("take-over-system-keys"), keys.takeOverSystemKeys);
    map.insert(QStringLiteral("capture-last"), accelerators(keys.last));
    map.insert(QStringLiteral("capture-screen"), accelerators(keys.screen));
    map.insert(QStringLiteral("capture-window"), accelerators(keys.window));
    map.insert(QStringLiteral("capture-region"), accelerators(keys.region));
    return map;
}

void GnomeShellBackend::setHotkeyOptions(const HotkeyOptions &options)
{
    // The extension keeps them in its settings, for hotkey captures while
    // the app is not running.
    if (!isAvailable() || extensionVersion() < kHotkeyVersion)
        return;
    auto message = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("Configure"));
    message << configureOptions(options);
    QDBusConnection::sessionBus().asyncCall(message);
}

QString GnomeShellBackend::hotkeyProblem() const
{
    if (!isAvailable())
        return unavailableReason();
    if (extensionVersion() < kHotkeyVersion)
        return tr("Die GNOME-Erweiterung «NSL Snap» ist veraltet und kennt die globalen Tastenkürzel noch nicht. "
                  "Bitte ab- und wieder anmelden.");
    return {};
}

void GnomeShellBackend::takeHotkeyCapture()
{
    if (!isAvailable())
        return;
    const auto message = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("TakeCapture"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        handleCaptureReply(w->reply(), true);
    });
}

CaptureResult GnomeShellBackend::resultFromReply(const QByteArray &png, const QVariantMap &info)
{
    CaptureResult result;
    // Version 1 of the extension sends no "mode": always a window.
    const QString mode = info.value(QStringLiteral("mode")).toString();
    result.kind = mode == QLatin1String("region")   ? CaptureResult::Kind::Region
                  : mode == QLatin1String("screen") ? CaptureResult::Kind::Screen
                                                    : CaptureResult::Kind::Window;
    result.image = QImage::fromData(png, "PNG");
    result.title = info.value(QStringLiteral("title")).toString();
    result.appId = info.value(QStringLiteral("app-id")).toString();
    result.frameRect = rectFromVariant(info.value(QStringLiteral("frame-rect")));
    result.bufferRect = rectFromVariant(info.value(QStringLiteral("buffer-rect")));
    result.regionRect = rectFromVariant(info.value(QStringLiteral("rect")));
    result.scale = info.value(QStringLiteral("scale"), 1.0).toDouble();
    return result;
}
