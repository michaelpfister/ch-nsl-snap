#include "app/UpdateDialog.h"

#include "app/Language.h"

#include "ui/Placement.h"
#include "update/UpdateInstaller.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>
#include <QVBoxLayout>

UpdateDialog::UpdateDialog(const Releases::Release &release, QWidget *parent)
    : QDialog(parent)
    , m_release(release)
{
    const Version installed = Version::current();
    const int order = Version::compare(release.version, installed);
    const Releases::Installation installation = Releases::installation();
    const bool canInstall = UpdateInstaller::isSupported();

    setWindowTitle(order > 0 ? tr("Update") : tr("Andere Version"));
    setMinimumSize(520, 440);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(8);

    auto *title = new QLabel(m_release.title, this);
    title->setProperty("role", QStringLiteral("panelTitle"));
    layout->addWidget(title);

    QString summary;
    if (order > 0)
        summary = tr("Eine neue Version ist verfügbar. Installiert ist %1.").arg(installed.toString());
    else if (order < 0)
        summary = tr("Diese Version ist älter als die installierte (%1). Die Einstellungen bleiben erhalten; "
                     "was diese Version noch nicht kennt, gilt nach einem späteren Update wieder.")
                      .arg(installed.toString());
    else
        summary = tr("Diese Version ist bereits installiert. Erneut installieren ersetzt die Programmdateien.");
    if (m_release.prerelease)
        summary += QLatin1Char(' ') + tr("Es ist eine Vorabversion zum Testen.");
    if (m_release.published.isValid())
        summary += QLatin1Char(' ') + tr("Veröffentlicht am %1.")
                                          .arg(Language::locale().toString(m_release.published.toLocalTime().date(), tr("d. MMMM yyyy")));
    auto *summaryLabel = new QLabel(summary, this);
    summaryLabel->setWordWrap(true);
    summaryLabel->setProperty("role", QStringLiteral("hint"));
    layout->addWidget(summaryLabel);

    // Release notes, followed by the steps where the app cannot install.
    auto *notes = new QTextBrowser(this);
    notes->setOpenExternalLinks(true);
    QString markdown = m_release.notes.trimmed().isEmpty() ? tr("*Keine Beschreibung.*") : m_release.notes;
    if (!canInstall)
        markdown = instructions(installation, m_release) + QStringLiteral("\n\n---\n\n") + markdown;
    notes->setMarkdown(markdown);
    layout->addWidget(notes, 1);

    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(6);
    layout->addWidget(m_progress);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setProperty("role", QStringLiteral("hint"));
    m_status->setVisible(false);
    layout->addWidget(m_status);

    auto *buttons = new QHBoxLayout;
    auto *page = new QPushButton(tr("Auf GitHub ansehen"), this);
    page->setProperty("role", QStringLiteral("secondary"));
    connect(page, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(m_release.page); });
    buttons->addWidget(page);
    buttons->addStretch(1);
    m_closeButton = new QPushButton(canInstall ? tr("Später") : tr("Schliessen"), this);
    m_closeButton->setProperty("role", QStringLiteral("secondary"));
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(m_closeButton);
    if (canInstall) {
        m_installButton = new QPushButton(order >= 0 ? tr("Installieren und neu starten")
                                                     : tr("Version %1 installieren und neu starten")
                                                           .arg(m_release.version.toString()), this);
        m_installButton->setProperty("role", QStringLiteral("primary"));
        m_installButton->setDefault(true);
        connect(m_installButton, &QPushButton::clicked, this, &UpdateDialog::install);
        buttons->addWidget(m_installButton);
    }
    layout->addLayout(buttons);
    resize(minimumSize());
    fitIntoScreen(this); // the notes scroll
}

UpdateDialog::~UpdateDialog() = default;

void UpdateDialog::install()
{
    if (!m_installer) {
        m_installer = new UpdateInstaller(this);
        connect(m_installer, &UpdateInstaller::progress, this, [this](qint64 received, qint64 total) {
            if (total > 0) {
                m_progress->setRange(0, 1000);
                m_progress->setValue(int(received * 1000 / total));
                m_status->setText(tr("Wird heruntergeladen: %1 von %2")
                                      .arg(Language::locale().formattedDataSize(received), Language::locale().formattedDataSize(total)));
            } else {
                m_progress->setRange(0, 0); // busy indicator
            }
        });
        connect(m_installer, &UpdateInstaller::ready, this, [this] {
            m_ready = true;
            accept();
        });
        connect(m_installer, &UpdateInstaller::failed, this, [this](const QString &message) {
            m_progress->setVisible(false);
            m_status->setText(message);
            m_status->setProperty("role", QStringLiteral("warning"));
            m_status->style()->unpolish(m_status); // the role changed: restyle
            m_status->style()->polish(m_status);
            m_installButton->setEnabled(true);
            m_installButton->setText(tr("Nochmals versuchen"));
        });
    }
    m_installButton->setEnabled(false);
    m_progress->setRange(0, 0);
    m_progress->setVisible(true);
    m_status->setProperty("role", QStringLiteral("hint"));
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
    m_status->setText(tr("Wird heruntergeladen …"));
    m_status->setVisible(true);
    m_installer->prepare(m_release);
}

QString UpdateDialog::instructions(Releases::Installation installation, const Releases::Release &release)
{
    const QString tag = release.tag;
    const QString flake = QStringLiteral("github:%1/%2").arg(Releases::repository(), tag);
    switch (installation) {
    case Releases::Installation::Nix:
        return tr("**Wechsel auf %1:** Snapslash wird von Nix verwaltet und kann sich nicht selbst ersetzen.\n\n"
                  "In einer NixOS-Konfiguration mit Flake den Eingang auf diese Version setzen, dann neu bauen:\n\n"
                  "```\ninputs.snapslash.url = \"%2\";\n```\n\n"
                  "```\nnix flake update snapslash\nsudo nixos-rebuild switch\n```\n\n"
                  "Mit `nix profile`:\n\n"
                  "```\nnix profile remove snapslash\nnix profile install %2\n```")
            .arg(release.version.toString(), flake);
    case Releases::Installation::AppImage:
        // Until the app replaces the file itself (D-041, step 3).
        return tr("**Wechsel auf %1:** Auf der Release-Seite die Datei `%2` herunterladen, "
                  "die bisherige AppImage-Datei damit ersetzen und sie starten. Menüeintrag, Befehl und "
                  "GNOME-Erweiterung richtet die App danach selbst neu ein.")
            .arg(release.version.toString(), Releases::linuxAssetName(release.version));
    case Releases::Installation::MacApp:
        // The permission belongs to the build while the app is signed ad hoc
        // (D-047, Q-36).
        return tr("**Wechsel auf %1:** Auf der Release-Seite die Datei `%2` herunterladen und öffnen, "
                  "dann Snapslash in den Ordner «Programme» ziehen und die bisherige Version ersetzen.\n\n"
                  "Danach in den Systemeinstellungen unter «Datenschutz & Sicherheit › Bildschirm- & "
                  "Systemaudioaufnahme» Snapslash aus- und wieder einschalten: Die neue Version braucht "
                  "die Berechtigung noch einmal.")
            .arg(release.version.toString(), Releases::macosAssetName(release.version));
    case Releases::Installation::PortableWindows:
    case Releases::Installation::Other:
        break;
    }
#ifdef Q_OS_WIN
    return tr("**Wechsel auf %1:** Diese Kopie stammt nicht aus einem Release-Paket und kann sich "
              "deshalb nicht selbst ersetzen. Auf der Release-Seite das ZIP für Windows herunterladen, "
              "entpacken und `snapslash.exe` darin starten; ab dann geht es mit einem Klick.")
        .arg(release.version.toString());
#else
    // The pre-release part goes to CMake, so the build reports the same
    // version as the release (D-037).
    const QString suffix = release.version.prerelease.join(QLatin1Char('.'));
    return tr("**Wechsel auf %1:** Diese Kopie wurde aus dem Quellcode gebaut. Im Repository:\n\n"
              "```\ngit fetch --tags\ngit checkout %2\n"
              "cmake -S . -B build -DNSL_VERSION_SUFFIX=%3\ncmake --build build\n"
              "cmake --install build --prefix ~/.local\n```\n\n"
              "Unter GNOME danach die Erweiterung neu installieren (`scripts/dev-install.sh`) und sich "
              "ab- und wieder anmelden.")
        .arg(release.version.toString(), tag, suffix);
#endif
}
