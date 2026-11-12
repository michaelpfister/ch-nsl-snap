#include "ui/ColorScheme.h"

#include <QGuiApplication>
#include <QStyleHints>

#ifdef NSL_HAVE_GNOME_BACKEND
#include "ui/linux/PortalColorScheme.h"
#endif

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
#ifdef NSL_HAVE_GNOME_BACKEND
    // Linux: the desktop portal, whatever platform theme Qt found.
    auto *portal = new PortalColorScheme(this);
    m_portalDark = portal->read();
    connect(portal, &PortalColorScheme::darkChanged, this, [this](bool dark) {
        if (dark == m_portalDark)
            return;
        m_portalDark = dark;
        if (m_choice == Choice::System)
            emit changed();
    });
#endif
}

std::optional<bool> ColorScheme::darkFromPortalValue(uint value)
{
#ifdef NSL_HAVE_GNOME_BACKEND
    return PortalColorScheme::darkFromValue(value);
#else
    return value == 1 ? std::optional<bool>(true)
         : (value == 0 || value == 2) ? std::optional<bool>(false)
                                      : std::nullopt;
#endif
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
