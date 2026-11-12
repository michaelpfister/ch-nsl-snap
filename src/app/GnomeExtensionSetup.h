#pragma once

#include <QCoreApplication>
#include <QString>

class QWidget;

// The dialog "GNOME-Erweiterung einrichten" of the AppImage (D-041): asks,
// then installs the bundled extension for this user and turns it on.
class GnomeExtensionSetup
{
    Q_DECLARE_TR_FUNCTIONS(GnomeExtensionSetup)

public:
    // An AppImage on GNOME that carries the extension.
    static bool isAvailable();

    // At start: when the installed extension is missing or of another
    // version, and the user has not said "Später" to this app version yet.
    static bool shouldOffer();

    // Asks (with `reason` above the explanation, if given), installs and
    // enables. "Später" is remembered for this app version. Returns true
    // when the extension was installed.
    static bool offer(QWidget *parent, const QString &reason = {});

    // The state for the settings: installed and matching, missing, other.
    static QString stateText();
};
