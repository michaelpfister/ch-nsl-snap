#pragma once

#include <QObject>

#include <optional>

class QDBusVariant;

// Light or dark (D-014, D-045): the user's choice in the settings, or the
// system's. On Linux the app asks the desktop portal itself
// (org.freedesktop.appearance color-scheme) and follows its changes, so it
// does not depend on which platform theme Qt found (an AppImage carries no
// GTK theme plugin). Elsewhere, or without a portal, Qt's own answer counts.
class ColorScheme : public QObject
{
    Q_OBJECT

public:
    enum class Choice { System, Light, Dark };

    // The one instance, created on first use.
    static ColorScheme *instance();

    bool isDark() const;
    Choice choice() const { return m_choice; }
    void setChoice(Choice choice);

    // "system", "light", "dark" (stored in the settings).
    static Choice fromString(const QString &text);
    static QString toString(Choice choice);

    // The portal's value: 1 prefers dark, 2 light, 0 no preference.
    static std::optional<bool> darkFromPortalValue(uint value);

signals:
    // Light or dark changed (the choice, or the system's setting).
    void changed();

private slots:
    void onPortalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value);

private:
    explicit ColorScheme(QObject *parent);
    void readPortal();
    bool systemIsDark() const;

    Choice m_choice = Choice::System;
    std::optional<bool> m_portalDark; // nothing: no portal, ask Qt
};
