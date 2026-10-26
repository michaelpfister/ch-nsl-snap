#include "app/AppService.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

namespace {
const QString kName = QStringLiteral("ch.netslash.Snap");
const QString kPath = QStringLiteral("/ch/netslash/Snap");
const QString kInterface = QStringLiteral("ch.netslash.Snap.App1");
}

AppService::AppService(QObject *parent)
    : QObject(parent)
{
}

bool AppService::registerOrActivateOther()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return true; // no session bus: run without the service
    // The object first: D-Bus activation delivers the waiting call as soon
    // as the name is owned.
    bus.registerObject(kPath, this, QDBusConnection::ExportScriptableSlots);
    if (bus.registerService(kName))
        return true;
    bus.unregisterObject(kPath);
    const auto message = QDBusMessage::createMethodCall(kName, kPath, kInterface, QStringLiteral("Activate"));
    bus.call(message, QDBus::Block, 3000);
    return false;
}

void AppService::CaptureReady()
{
    emit captureReady();
}

void AppService::Activate()
{
    emit activateRequested();
}

bool AppService::HideForCapture()
{
    return hideForCapture ? hideForCapture() : false;
}

void AppService::CaptureCancelled()
{
    emit captureCancelled();
}
