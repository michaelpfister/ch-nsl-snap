#include "app/GnomeExtensionSetup.h"

#include "app/LinuxIntegration.h"
#include "app/Settings.h"
#include "ui/AppDialog.h"

#include <QPushButton>

using LinuxIntegration::ExtensionState;

bool GnomeExtensionSetup::isAvailable()
{
    return LinuxIntegration::isAppImage() && LinuxIntegration::isGnome()
        && LinuxIntegration::hasBundledExtension(LinuxIntegration::currentPaths());
}

bool GnomeExtensionSetup::shouldOffer()
{
    return isAvailable()
        && LinuxIntegration::extensionState(LinuxIntegration::currentPaths()) != ExtensionState::Current
        && Settings::gnomeExtensionDeclinedFor() != QLatin1String(NSL_VERSION);
}

bool GnomeExtensionSetup::offer(QWidget *parent, const QString &reason)
{
    const LinuxIntegration::Paths paths = LinuxIntegration::currentPaths();
    const bool update = LinuxIntegration::extensionState(paths) == ExtensionState::Different;

    QString text = update ? tr("Die installierte Erweiterung passt nicht zu dieser Version der App. Die App ersetzt sie "
                               "durch die mitgelieferte Version.")
                          : tr("Unter GNOME wählt Snapslash Fenster und Bereiche über die eigene Erweiterung aus. Die "
                               "App kopiert sie in den eigenen Benutzerordner und schaltet sie ein.");
    text += QLatin1Char(' ') + tr("GNOME lädt Erweiterungen nur bei der Anmeldung: Danach einmal ab- und wieder anmelden.");
    if (!reason.isEmpty())
        text = reason + QStringLiteral("\n\n") + text;
    AppDialog dialog(parent, AppDialog::Kind::Question,
                     update ? tr("GNOME-Erweiterung aktualisieren?") : tr("GNOME-Erweiterung einrichten?"), text);
    dialog.addButton(tr("Später"), AppDialog::Role::Cancel);
    QPushButton *setUp = dialog.addButton(update ? tr("Aktualisieren") : tr("Einrichten"), AppDialog::Role::Primary);
    if (dialog.exec() != setUp) {
        Settings::setGnomeExtensionDeclinedFor(QStringLiteral(NSL_VERSION));
        return false;
    }

    QString error = LinuxIntegration::installExtension(paths);
    if (error.isEmpty())
        error = LinuxIntegration::enableExtension();
    if (!error.isEmpty()) {
        AppDialog::error(parent, tr("Die Erweiterung liess sich nicht einrichten."), error);
        return false;
    }
    AppDialog::information(parent, tr("Die Erweiterung ist eingerichtet."),
                           tr("Bitte jetzt ab- und wieder anmelden, damit GNOME sie lädt."));
    return true;
}

QString GnomeExtensionSetup::stateText()
{
    switch (LinuxIntegration::extensionState(LinuxIntegration::currentPaths())) {
    case ExtensionState::Missing: return tr("Nicht installiert.");
    case ExtensionState::Current: return tr("Installiert, passend zu dieser Version der App.");
    case ExtensionState::Different: return tr("Installiert, aber in einer anderen Version als die App.");
    }
    return {};
}
