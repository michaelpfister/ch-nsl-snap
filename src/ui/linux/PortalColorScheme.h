#pragma once

#include <QObject>

#include <optional>

class QDBusVariant;

// Light or dark from the desktop portal (org.freedesktop.portal.Settings,
// org.freedesktop.appearance color-scheme), Linux only: the D-Bus part of
// ColorScheme (D-045), apart so Windows builds need no Qt D-Bus.
class PortalColorScheme : public QObject
{
    Q_OBJECT

public:
    explicit PortalColorScheme(QObject *parent = nullptr);

    // The portal's answer now: dark, light, or nothing (no portal).
    std::optional<bool> read() const;

    // The portal's value: 1 prefers dark, 2 light, 0 no preference.
    static std::optional<bool> darkFromValue(uint value);

signals:
    void darkChanged(bool dark);

private slots:
    void onSettingChanged(const QString &group, const QString &key, const QDBusVariant &value);
};
