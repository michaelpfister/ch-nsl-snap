#include "app/GnomeExtensionSetup.h"

#include "app/LinuxIntegration.h"
#include "app/Settings.h"

#include <QMessageBox>
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

    QString text = update ? tr("Die installierte GNOME-Erweiterung «Snapslash» passt nicht zu dieser Version der App.")
                          : tr("Unter GNOME nimmt Snapslash über die eigene GNOME-Erweiterung auf: Nur sie darf "
                               "Fenster und Bereiche auswählen lassen und das Bild an die App geben.");
    if (!reason.isEmpty())
        text = reason + QStringLiteral("\n\n") + text;
    QMessageBox box(QMessageBox::Question, tr("GNOME-Erweiterung"), text, QMessageBox::NoButton, parent);
    box.setInformativeText(
        (update ? tr("Die App ersetzt sie durch die mitgelieferte Version.")
                : tr("Die App kopiert die mitgelieferte Erweiterung in den eigenen Benutzerordner und schaltet sie ein."))
        + QLatin1Char(' ')
        + tr("GNOME lädt Erweiterungen nur bei der Anmeldung: Danach einmal ab- und wieder anmelden."));
    QPushButton *setUp = box.addButton(update ? tr("Aktualisieren") : tr("Einrichten"), QMessageBox::AcceptRole);
    box.addButton(tr("Später"), QMessageBox::RejectRole);
    box.setDefaultButton(setUp);
    box.exec();
    if (box.clickedButton() != setUp) {
        Settings::setGnomeExtensionDeclinedFor(QStringLiteral(NSL_VERSION));
        return false;
    }

    QString error = LinuxIntegration::installExtension(paths);
    if (error.isEmpty())
        error = LinuxIntegration::enableExtension();
    if (!error.isEmpty()) {
        QMessageBox::warning(parent, tr("GNOME-Erweiterung"),
                             tr("Die Erweiterung liess sich nicht einrichten.") + QStringLiteral("\n\n") + error);
        return false;
    }
    QMessageBox::information(parent, tr("GNOME-Erweiterung"),
                             tr("Die Erweiterung ist eingerichtet. Bitte jetzt ab- und wieder anmelden, "
                                "damit GNOME sie lädt."));
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
