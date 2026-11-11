#include "recording/gnome/GnomeRecordingBackend.h"

#include "capture/CaptureHints.h"
#include "capture/gnome/GnomeShellBackend.h"
#include "recording/pipewire/PipeWireStream.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

#include <limits>

namespace {

const QString kShellService = QStringLiteral("ch.netslash.Snap.Shell");
const QString kShellPath = QStringLiteral("/ch/netslash/Snap");
const QString kShellInterface = QStringLiteral("ch.netslash.Snap1");
// Version 8 knows the purpose "video" (7 brought the countdown options).
constexpr uint kVideoVersion = 8;

const QString kMutter = QStringLiteral("org.gnome.Mutter.ScreenCast");
const QString kMutterPath = QStringLiteral("/org/gnome/Mutter/ScreenCast");
const QString kSessionInterface = QStringLiteral("org.gnome.Mutter.ScreenCast.Session");
const QString kStreamInterface = QStringLiteral("org.gnome.Mutter.ScreenCast.Stream");
// cursor-mode 1: the pointer is drawn into the frames (F-50).
constexpr uint kCursorEmbedded = 1;
constexpr int kMutterTimeoutMs = 5000;

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

// A blocking call to Mutter, which answers within milliseconds; the error
// text in `error` if it fails.
QDBusMessage callMutter(const QString &path, const QString &interface, const QString &method,
                        const QVariantList &arguments, QString *error)
{
    auto message = QDBusMessage::createMethodCall(kMutter, path, interface, method);
    message.setArguments(arguments);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, kMutterTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage && error)
        *error = reply.errorMessage();
    return reply;
}

// The overlay's texts for a video, under the keys the extension knows.
QVariantMap videoHints()
{
    return {
        {QStringLiteral("window"), CaptureHints::videoWindow()},
        {QStringLiteral("screens"), CaptureHints::videoScreens()},
        {QStringLiteral("screen"), CaptureHints::videoScreen()},
        {QStringLiteral("countdown"), CaptureHints::countdown()},
    };
}

} // namespace

GnomeRecordingBackend::GnomeRecordingBackend(QObject *parent)
    : RecordingBackend(parent)
    , m_shell(std::make_unique<GnomeShellBackend>())
    , m_pipewire(std::make_unique<PipeWireStream>())
{
}

GnomeRecordingBackend::~GnomeRecordingBackend()
{
    stopStream();
}

bool GnomeRecordingBackend::isAvailable() const
{
    return m_shell->isAvailable();
}

QString GnomeRecordingBackend::unavailableReason() const
{
    return m_shell->unavailableReason();
}

GnomeRecordingBackend::Pick GnomeRecordingBackend::pickFromReply(const QVariantMap &info)
{
    Pick pick;
    pick.window = info.value(QStringLiteral("mode")).toString() == QLatin1String("window");
    pick.windowId = info.value(QStringLiteral("window-id")).toULongLong();
    pick.area = rectFromVariant(info.value(QStringLiteral("rect")));
    pick.size = QSize(info.value(QStringLiteral("width")).toInt(), info.value(QStringLiteral("height")).toInt());
    pick.title = info.value(QStringLiteral("title")).toString();
    return pick;
}

void GnomeRecordingBackend::pick(Source source)
{
    if (m_busy)
        return;
    if (!isAvailable()) {
        emit failed(unavailableReason());
        return;
    }
    // The "Version" property; an older extension runs until the next login.
    auto versionCall = QDBusMessage::createMethodCall(kShellService, kShellPath,
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    versionCall << kShellInterface << QStringLiteral("Version");
    const QDBusMessage versionReply = QDBusConnection::sessionBus().call(versionCall, QDBus::Block, 2000);
    const uint version = versionReply.arguments().isEmpty()
        ? 0
        : versionReply.arguments().first().value<QDBusVariant>().variant().toUInt();
    if (version < kVideoVersion) {
        emit failed(tr("Die GNOME-Erweiterung «Snapslash» ist veraltet und kennt die Videoaufnahme noch nicht. "
                       "Bitte ab- und wieder anmelden, damit die neue Version geladen wird."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("purpose"), QStringLiteral("video"));
    options.insert(QStringLiteral("mode"), source == Source::Window ? QStringLiteral("window") : QStringLiteral("screen"));
    options.insert(QStringLiteral("hints"), videoHints());
    auto message = QDBusMessage::createMethodCall(kShellService, kShellPath, kShellInterface, QStringLiteral("Capture"));
    message << options;
    m_busy = true;
    // No timeout: the user may take as long as they like to pick.
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, std::numeric_limits<int>::max()), this);
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
        m_pick = pickFromReply(reply.argumentAt<1>());
        if (m_pick.window && m_pick.windowId == 0) {
            emit failed(tr("Die GNOME-Erweiterung hat kein Fenster genannt."));
            return;
        }
        emit picked(m_pick.window ? Source::Window : Source::Screen,
                    m_pick.window ? m_pick.title : tr("Bildschirm"), m_pick.size);
    });
}

void GnomeRecordingBackend::startStream()
{
    stopStream();
    m_ended = false;
    QString error;
    const QDBusMessage created =
        callMutter(kMutterPath, kMutter, QStringLiteral("CreateSession"), {QVariantMap()}, &error);
    if (created.type() != QDBusMessage::ReplyMessage || created.arguments().isEmpty()) {
        end(tr("GNOME verweigert die Bildschirmaufnahme: %1").arg(error));
        return;
    }
    m_session = created.arguments().first().value<QDBusObjectPath>().path();

    QVariantMap properties;
    properties.insert(QStringLiteral("cursor-mode"), QVariant::fromValue(kCursorEmbedded));
    QDBusMessage recorded;
    if (m_pick.window) {
        properties.insert(QStringLiteral("window-id"), QVariant::fromValue(quint64(m_pick.windowId))); // D-Bus t
        recorded = callMutter(m_session, kSessionInterface, QStringLiteral("RecordWindow"), {properties}, &error);
    } else {
        recorded = callMutter(m_session, kSessionInterface, QStringLiteral("RecordArea"),
                              {m_pick.area.x(), m_pick.area.y(), m_pick.area.width(), m_pick.area.height(), properties},
                              &error);
    }
    if (recorded.type() != QDBusMessage::ReplyMessage || recorded.arguments().isEmpty()) {
        end(m_pick.window ? tr("Das Fenster lässt sich nicht aufnehmen: %1").arg(error)
                          : tr("Der Bildschirm lässt sich nicht aufnehmen: %1").arg(error));
        return;
    }
    m_stream = recorded.arguments().first().value<QDBusObjectPath>().path();

    // The PipeWire node comes as a signal once the session runs; a closed
    // window ends the session.
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kMutter, m_stream, kStreamInterface, QStringLiteral("PipeWireStreamAdded"), this,
                SLOT(onPipeWireStreamAdded(uint)));
    bus.connect(kMutter, m_session, kSessionInterface, QStringLiteral("Closed"), this, SLOT(onSessionClosed()));
    const QDBusMessage started = callMutter(m_session, kSessionInterface, QStringLiteral("Start"), {}, &error);
    if (started.type() == QDBusMessage::ErrorMessage)
        end(tr("GNOME verweigert die Bildschirmaufnahme: %1").arg(error));
}

void GnomeRecordingBackend::onPipeWireStreamAdded(uint nodeId)
{
    if (m_ended || m_session.isEmpty())
        return;
    if (!m_pipewire->start(nodeId))
        end(tr("PipeWire ist nicht erreichbar; ohne PipeWire gibt es unter GNOME keine Videos."));
}

void GnomeRecordingBackend::onSessionClosed()
{
    end(m_pick.window ? tr("Das Fenster ist nicht mehr da; die Aufnahme wurde beendet.")
                      : tr("GNOME hat die Bildschirmaufnahme beendet."));
}

void GnomeRecordingBackend::requestFrame()
{
    if (m_ended)
        return;
    if (m_pipewire->hasEnded()) {
        end(tr("Der Videostrom von GNOME ist abgebrochen: %1").arg(m_pipewire->lastError()));
        return;
    }
    // Mutter sends a frame when something changes; in between the
    // controller repeats the last one.
    QImage frame;
    if (m_pipewire->takeLatest(frame))
        emit frameReady(frame);
}

void GnomeRecordingBackend::stopStream()
{
    m_pipewire->stop();
    closeSession();
}

void GnomeRecordingBackend::closeSession()
{
    if (m_session.isEmpty())
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(kMutter, m_stream, kStreamInterface, QStringLiteral("PipeWireStreamAdded"), this,
                   SLOT(onPipeWireStreamAdded(uint)));
    bus.disconnect(kMutter, m_session, kSessionInterface, QStringLiteral("Closed"), this, SLOT(onSessionClosed()));
    // Without waiting: the session may already be gone.
    bus.asyncCall(QDBusMessage::createMethodCall(kMutter, m_session, kSessionInterface, QStringLiteral("Stop")));
    m_session.clear();
    m_stream.clear();
}

void GnomeRecordingBackend::end(const QString &reason)
{
    if (m_ended)
        return;
    m_ended = true;
    stopStream();
    emit streamEnded(reason);
}
