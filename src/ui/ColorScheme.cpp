#include "ui/ColorScheme.h"

#include <QGuiApplication>
#include <QStyleHints>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#endif

namespace {

const QString kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kPortalSettings = QStringLiteral("org.freedesktop.portal.Settings");
const QString kAppearance = QStringLiteral("org.freedesktop.appearance");
const QString kColorScheme = QStringLiteral("color-scheme");

} // namespace

ColorScheme *ColorScheme::instance()
{
    static ColorScheme *scheme = new ColorScheme(qApp);
    return scheme;
}

ColorScheme::ColorScheme(QObject *parent)
    : QObject(parent)
{
    // Qt's answer (Windows, macOS, and Linux without a portal).
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_choice == Choice::System && !m_portalDark)
            emit changed();
    });
#ifdef Q_OS_LINUX
    readPortal();
    // The portal says when the user switches light and dark.
    QDBusConnection::sessionBus().connect(kPortalService, kPortalPath, kPortalSettings, QStringLiteral("SettingChanged"),
                                          this, SLOT(onPortalSettingChanged(QString, QString, QDBusVariant)));
#endif
}

std::optional<bool> ColorScheme::darkFromPortalValue(uint value)
{
    switch (value) {
    case 1: return true;  // prefer dark
    case 2: return false; // prefer light
    case 0: return false; // no preference: the desktops' default is light
    default: return std::nullopt;
    }
}

void ColorScheme::readPortal()
{
#ifdef Q_OS_LINUX
    // ReadOne (portal version 2) answers the value itself; the older Read
    // wraps it in one more variant.
    for (const QString &method : {QStringLiteral("ReadOne"), QStringLiteral("Read")}) {
        QDBusMessage call = QDBusMessage::createMethodCall(kPortalService, kPortalPath, kPortalSettings, method);
        call << kAppearance << kColorScheme;
        const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 500);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            continue;
        QVariant value = reply.arguments().first().value<QDBusVariant>().variant();
        if (value.canConvert<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        if (value.canConvert<uint>()) {
            m_portalDark = darkFromPortalValue(value.toUInt());
            if (m_portalDark)
                return;
        }
    }
#endif
}

void ColorScheme::onPortalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value)
{
    if (group != kAppearance || key != kColorScheme)
        return;
    const std::optional<bool> dark = darkFromPortalValue(value.variant().toUInt());
    if (!dark || dark == m_portalDark)
        return;
    m_portalDark = dark;
    if (m_choice == Choice::System)
        emit changed();
}

bool ColorScheme::systemIsDark() const
{
    if (m_portalDark)
        return *m_portalDark;
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

bool ColorScheme::isDark() const
{
    switch (m_choice) {
    case Choice::Light: return false;
    case Choice::Dark: return true;
    case Choice::System: break;
    }
    return systemIsDark();
}

void ColorScheme::setChoice(Choice choice)
{
    if (choice == m_choice)
        return;
    const bool wasDark = isDark();
    m_choice = choice;
    if (isDark() != wasDark)
        emit changed();
}

ColorScheme::Choice ColorScheme::fromString(const QString &text)
{
    if (text == QLatin1String("light"))
        return Choice::Light;
    if (text == QLatin1String("dark"))
        return Choice::Dark;
    return Choice::System;
}

QString ColorScheme::toString(Choice choice)
{
    switch (choice) {
    case Choice::Light: return QStringLiteral("light");
    case Choice::Dark: return QStringLiteral("dark");
    case Choice::System: break;
    }
    return QStringLiteral("system");
}
