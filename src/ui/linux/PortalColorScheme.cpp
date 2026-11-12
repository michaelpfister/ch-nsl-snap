#include "ui/linux/PortalColorScheme.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kSettings = QStringLiteral("org.freedesktop.portal.Settings");
const QString kAppearance = QStringLiteral("org.freedesktop.appearance");
const QString kColorScheme = QStringLiteral("color-scheme");

} // namespace

PortalColorScheme::PortalColorScheme(QObject *parent)
    : QObject(parent)
{
    // The portal says when the user switches light and dark.
    QDBusConnection::sessionBus().connect(kService, kPath, kSettings, QStringLiteral("SettingChanged"), this,
                                          SLOT(onSettingChanged(QString, QString, QDBusVariant)));
}

std::optional<bool> PortalColorScheme::darkFromValue(uint value)
{
    switch (value) {
    case 1: return true;  // prefer dark
    case 2: return false; // prefer light
    case 0: return false; // no preference: the desktops' default is light
    default: return std::nullopt;
    }
}

std::optional<bool> PortalColorScheme::read() const
{
    // ReadOne (portal version 2) answers the value itself; the older Read
    // wraps it in one more variant.
    for (const QString &method : {QStringLiteral("ReadOne"), QStringLiteral("Read")}) {
        QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kSettings, method);
        call << kAppearance << kColorScheme;
        const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 500);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            continue;
        QVariant value = reply.arguments().first().value<QDBusVariant>().variant();
        if (value.canConvert<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        if (value.canConvert<uint>()) {
            if (const std::optional<bool> dark = darkFromValue(value.toUInt()))
                return dark;
        }
    }
    return std::nullopt;
}

void PortalColorScheme::onSettingChanged(const QString &group, const QString &key, const QDBusVariant &value)
{
    if (group != kAppearance || key != kColorScheme)
        return;
    if (const std::optional<bool> dark = darkFromValue(value.variant().toUInt()))
        emit darkChanged(*dark);
}
