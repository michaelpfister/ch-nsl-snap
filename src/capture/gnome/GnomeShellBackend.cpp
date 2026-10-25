#include "capture/gnome/GnomeShellBackend.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

#include <limits>

namespace {

const QString kService = QStringLiteral("ch.netslash.Snap.Shell");
const QString kPath = QStringLiteral("/ch/netslash/Snap");
const QString kInterface = QStringLiteral("ch.netslash.Snap1");

// "frame-rect" and "buffer-rect" are D-Bus structs (iiii): x, y, width, height.
QRect rectFromVariant(const QVariant &value)
{
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

void GnomeShellBackend::captureWindow()
{
    if (m_busy)
        return;
    if (!isAvailable()) {
        emit failed(unavailableReason());
        return;
    }
    m_busy = true;

    // The app crops itself (D-010), so it always asks for the whole buffer.
    QVariantMap options;
    options.insert(QStringLiteral("crop"), QStringLiteral("buffer"));

    auto message = QDBusMessage::createMethodCall(kService, kPath, kInterface,
                                                  QStringLiteral("PickWindow"));
    message << options;

    // No timeout: the user may take as long as they like to pick a window.
    const auto call = QDBusConnection::sessionBus().asyncCall(message, std::numeric_limits<int>::max());
    auto *watcher = new QDBusPendingCallWatcher(call, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        m_busy = false;

        const QDBusPendingReply<QByteArray, QVariantMap> reply = *w;
        if (reply.isError()) {
            if (reply.error().name().endsWith(QLatin1String(".Cancelled")))
                emit cancelled();
            else
                emit failed(reply.error().message());
            return;
        }

        const QVariantMap info = reply.argumentAt<1>();
        CaptureResult result;
        result.image = QImage::fromData(reply.argumentAt<0>(), "PNG");
        result.title = info.value(QStringLiteral("title")).toString();
        result.appId = info.value(QStringLiteral("app-id")).toString();
        result.frameRect = rectFromVariant(info.value(QStringLiteral("frame-rect")));
        result.bufferRect = rectFromVariant(info.value(QStringLiteral("buffer-rect")));
        result.scale = info.value(QStringLiteral("scale"), 1.0).toDouble();

        if (result.image.isNull()) {
            emit failed(tr("Die Aufnahme konnte nicht gelesen werden."));
            return;
        }
        emit captured(result);
    });
}
