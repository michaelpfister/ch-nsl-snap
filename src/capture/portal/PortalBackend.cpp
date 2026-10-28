#include "capture/portal/PortalBackend.h"

#include "capture/Countdown.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QRandomGenerator>
#include <QScreen>
#include <QUrl>

#include <cmath>

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kScreenshot = QStringLiteral("org.freedesktop.portal.Screenshot");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");

// Pictures the portal leaves in a temporary folder are the app's to
// remove; one it saved elsewhere (some desktops use ~/Pictures) stays.
bool isTemporary(const QString &path)
{
    const QString dir = QFileInfo(path).absolutePath();
    QStringList temporary{QDir::tempPath(), QStringLiteral("/tmp")};
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty())
        temporary << runtime;
    for (const QString &root : std::as_const(temporary)) {
        if (dir == root || dir.startsWith(root + QLatin1Char('/')))
            return true;
    }
    return false;
}

} // namespace

PortalBackend::PortalBackend(QObject *parent)
    : CaptureBackend(parent)
{
}

PortalBackend::~PortalBackend()
{
    stopListening();
}

bool PortalBackend::isAvailable() const
{
    const QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus)
        return false;
    // The portal starts on demand; it need not be running yet.
    return bus->isServiceRegistered(kService) || bus->activatableServiceNames().value().contains(kService);
}

QString PortalBackend::unavailableReason() const
{
    return tr("Auf diesem Desktop gibt es keine Möglichkeit zur Bildschirmaufnahme: weder wlr-screencopy noch "
              "das Screenshot-Portal (xdg-desktop-portal) sind vorhanden.");
}

void PortalBackend::captureWindow()
{
    emit failed(tr("Fenster lassen sich auf diesem Desktop nicht einzeln aufnehmen; das geht nur unter GNOME "
                   "und Hyprland. Bereich und Bildschirm funktionieren."));
}

void PortalBackend::captureRegion()
{
    start(CaptureOverlay::Mode::Region);
}

void PortalBackend::captureScreen()
{
    start(CaptureOverlay::Mode::Screen);
}

QString PortalBackend::requestPath(const QString &uniqueName, const QString &token)
{
    QString sender = uniqueName;
    if (sender.startsWith(QLatin1Char(':')))
        sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return kPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token;
}

QList<QRect> PortalBackend::splitDesktop(const QSize &picture, const QList<QRect> &logical)
{
    QRect desktop;
    for (const QRect &rect : logical)
        desktop = desktop.united(rect);
    QList<QRect> parts;
    if (desktop.isEmpty())
        return parts;
    const double sx = double(picture.width()) / desktop.width();
    const double sy = double(picture.height()) / desktop.height();
    for (const QRect &rect : logical) {
        // Edges, not sizes, are rounded, so neighbouring monitors still touch.
        const int left = int(std::lround((rect.x() - desktop.x()) * sx));
        const int top = int(std::lround((rect.y() - desktop.y()) * sy));
        const int right = int(std::lround((rect.x() + rect.width() - desktop.x()) * sx));
        const int bottom = int(std::lround((rect.y() + rect.height() - desktop.y()) * sy));
        parts.append(QRect(left, top, right - left, bottom - top));
    }
    return parts;
}

void PortalBackend::start(CaptureOverlay::Mode mode)
{
    if (m_waiting || m_overlay || m_countdown)
        return;
    if (!isAvailable()) {
        emit failed(unavailableReason());
        return;
    }
    m_mode = mode;
    if (delay() <= 0) {
        requestScreenshot();
        return;
    }
    // The countdown first (F-04); it is off the screen before the freeze.
    m_countdown = new Countdown;
    connect(m_countdown, &Countdown::finished, this, [this] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        requestScreenshot();
    });
    connect(m_countdown, &Countdown::cancelled, this, [this] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        emit cancelled();
    });
    m_countdown->start(delay());
}

void PortalBackend::listenTo(const QString &path)
{
    stopListening();
    m_requestPath = path;
    QDBusConnection::sessionBus().connect(kService, path, kRequest, QStringLiteral("Response"), this,
                                          SLOT(onResponse(uint, QVariantMap)));
}

void PortalBackend::stopListening()
{
    if (m_requestPath.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(kService, m_requestPath, kRequest, QStringLiteral("Response"), this,
                                             SLOT(onResponse(uint, QVariantMap)));
    m_requestPath.clear();
}

void PortalBackend::requestScreenshot()
{
    // Portal calls answer twice: at once with the path of a request object,
    // later with a Response signal on it. The path follows from our token,
    // so we listen before calling and miss no answer.
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString token = QStringLiteral("nsl_snap_%1").arg(QRandomGenerator::global()->generate());
    listenTo(requestPath(bus.baseService(), token));
    m_waiting = true;

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("interactive"), false); // no dialog of the portal's own: our overlay selects
    options.insert(QStringLiteral("modal"), false);
    auto message = QDBusMessage::createMethodCall(kService, kPath, kScreenshot, QStringLiteral("Screenshot"));
    message << QString() << options; // no parent window
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            stopListening();
            m_waiting = false;
            emit failed(tr("Das Screenshot-Portal hat die Aufnahme abgelehnt: %1").arg(reply.error().message()));
            return;
        }
        // Portals before version 0.9 choose their own path.
        if (m_waiting && reply.value().path() != m_requestPath)
            listenTo(reply.value().path());
    });
}

void PortalBackend::onResponse(uint response, const QVariantMap &results)
{
    if (!m_waiting)
        return;
    m_waiting = false;
    stopListening();
    if (response == 1) {
        emit cancelled();
        return;
    }
    const QString path = QUrl(results.value(QStringLiteral("uri")).toString()).toLocalFile();
    if (response != 0 || path.isEmpty()) {
        emit failed(tr("Das Screenshot-Portal konnte den Bildschirm nicht aufnehmen."));
        return;
    }
    QImageReader reader(path);
    const QImage picture = reader.read().convertToFormat(QImage::Format_RGB32);
    if (isTemporary(path))
        QFile::remove(path);
    if (picture.isNull()) {
        emit failed(tr("Die Aufnahme des Screenshot-Portals konnte nicht gelesen werden: %1").arg(reader.errorString()));
        return;
    }

    // One picture of the whole desktop: cut it into the monitors.
    const QList<QScreen *> screens = QGuiApplication::screens();
    QList<QRect> logical;
    for (QScreen *screen : screens)
        logical << screen->geometry();
    const QList<QRect> parts = splitDesktop(picture.size(), logical);
    m_screens.clear();
    for (qsizetype i = 0; i < screens.size(); ++i)
        m_screens.append({screens[i], parts[i], picture.copy(parts[i])});

    m_overlay = new CaptureOverlay(m_screens, {}, m_mode,
                                   regionByDragging() ? RegionSelector::Mode::Drag : RegionSelector::Mode::Clicks, this);
    connect(m_overlay, &CaptureOverlay::regionSelected, this,
            [this](const QRect &rect) { deliverPart(rect, CaptureResult::Kind::Region); });
    connect(m_overlay, &CaptureOverlay::screenSelected, this,
            [this](const QRect &rect) { deliverPart(rect, CaptureResult::Kind::Screen); });
    connect(m_overlay, &CaptureOverlay::cancelled, this, [this] {
        finish();
        emit cancelled();
    });
    m_overlay->show();
}

void PortalBackend::deliverPart(const QRect &native, CaptureResult::Kind kind)
{
    double scale = 1.0;
    const QImage image = CaptureOverlay::compose(m_screens, native, &scale);
    finish();

    CaptureResult result;
    result.kind = kind;
    result.image = image;
    result.scale = scale;
    result.regionRect = native;
    emit captured(result);
}

void PortalBackend::finish()
{
    m_screens.clear();
    if (m_overlay) {
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }
}
