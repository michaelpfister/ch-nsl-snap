#include "app/SettingsDialog.h"

#include "app/FileNames.h"
#include "app/Settings.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QRadioButton>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    setMinimumWidth(540);

    // Three tabs keep the dialog small enough for laptop screens.
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 12);
    auto *tabs = new QTabWidget(this);
    outer->addWidget(tabs);
    const auto page = [this, tabs](const QString &title) {
        auto *widget = new QWidget(this);
        auto *pageLayout = new QVBoxLayout(widget);
        pageLayout->setContentsMargins(16, 16, 16, 16);
        pageLayout->setSpacing(6);
        tabs->addTab(widget, title);
        return pageLayout;
    };

    // --- Allgemein ---
    QVBoxLayout *layout = page(tr("Allgemein"));

    m_advanced = new QCheckBox(tr("Erweiterter Modus"), this);
    m_advanced->setChecked(Settings::advancedMode());
    layout->addWidget(m_advanced);

    auto *hint = new QLabel(tr("Zeigt zusätzliche Regler im Eigenschaften-Panel, "
                               "zum Beispiel für die Stärke des Abdunkelns und der Unschärfe."), this);
    hint->setProperty("role", QStringLiteral("hint"));
    hint->setWordWrap(true);
    hint->setContentsMargins(26, 0, 0, 0); // aligned with the checkbox text
    layout->addWidget(hint);
    layout->addSpacing(16);

    // Region capture (F-03, D-023).
    auto *region = new QLabel(tr("Bereich auswählen"), this);
    region->setProperty("role", QStringLiteral("section"));
    layout->addWidget(region);
    auto *clicks = new QRadioButton(tr("Mit zwei Klicks: erst eine Ecke, dann die gegenüberliegende"), this);
    auto *drag = new QRadioButton(tr("Durch Ziehen mit gedrückter Maustaste"), this);
    auto *regionGroup = new QButtonGroup(this);
    regionGroup->addButton(clicks);
    regionGroup->addButton(drag);
    (Settings::regionByDragging() ? drag : clicks)->setChecked(true);
    layout->addWidget(clicks);
    layout->addWidget(drag);
    auto *regionHint = new QLabel(tr("Die Lupe und die Pfeiltasten (Ctrl: 10 Pixel) helfen, pixelgenau zu treffen."), this);
    regionHint->setProperty("role", QStringLiteral("hint"));
    regionHint->setWordWrap(true);
    regionHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(regionHint);
    connect(drag, &QRadioButton::toggled, this, [](bool enabled) { Settings::setRegionByDragging(enabled); });
    layout->addSpacing(16);

    // Tray icon (F-45).
    auto *trayTitle = new QLabel(tr("Infobereich"), this);
    trayTitle->setProperty("role", QStringLiteral("section"));
    layout->addWidget(trayTitle);
    auto *tray = new QCheckBox(tr("Beim Schliessen im Infobereich weiterlaufen"), this);
    tray->setChecked(Settings::closeToTray());
    layout->addWidget(tray);
    const bool hasTray = QSystemTrayIcon::isSystemTrayAvailable();
    auto *trayHint = new QLabel(hasTray ? tr("Aufnahmen lassen sich dann über das Symbol im Infobereich starten; "
                                             "«Beenden» im Menü des Symbols schliesst die App.")
                                        : tr("Diese Arbeitsfläche hat keinen Infobereich. Unter GNOME braucht es "
                                             "dafür die Erweiterung «AppIndicator»."), this);
    trayHint->setProperty("role", QStringLiteral("hint"));
    trayHint->setWordWrap(true);
    trayHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(trayHint);
    tray->setEnabled(hasTray);
    connect(tray, &QCheckBox::toggled, this, [](bool enabled) { Settings::setCloseToTray(enabled); });

    layout->addStretch(1);

    // --- Speichern (F-41): folder and file name pattern ---
    layout = page(tr("Speichern"));
    auto *folderLabel = new QLabel(tr("Ordner"), this);
    layout->addWidget(folderLabel);
    auto *folderRow = new QHBoxLayout;
    auto *folder = new QLineEdit(QDir::toNativeSeparators(Settings::saveFolder()), this);
    folder->setReadOnly(true);
    folder->setAccessibleName(tr("Speicherordner"));
    auto *chooseFolder = new QPushButton(tr("Ändern …"), this);
    chooseFolder->setProperty("role", QStringLiteral("secondary"));
    folderRow->addWidget(folder, 1);
    folderRow->addWidget(chooseFolder);
    layout->addLayout(folderRow);
    connect(chooseFolder, &QPushButton::clicked, this, [this, folder] {
        const QString chosen = QFileDialog::getExistingDirectory(this, tr("Speicherordner wählen"), Settings::saveFolder());
        if (chosen.isEmpty())
            return;
        Settings::setSaveFolder(chosen);
        folder->setText(QDir::toNativeSeparators(chosen));
    });
    layout->addSpacing(6);

    layout->addWidget(new QLabel(tr("Dateiname"), this));
    auto *pattern = new QLineEdit(Settings::fileNamePattern(), this);
    pattern->setAccessibleName(tr("Dateiname"));
    pattern->setPlaceholderText(FileNames::defaultPattern());
    layout->addWidget(pattern);
    auto *preview = new QLabel(this);
    preview->setProperty("role", QStringLiteral("hint"));
    preview->setWordWrap(true);
    layout->addWidget(preview);
    const auto showPreview = [preview](const QString &text) {
        preview->setText(tr("{Datum}, {Zeit} und {Titel} werden ersetzt. Beispiel: %1")
                             .arg(FileNames::fromPattern(text, tr("Einstellungen"), QDateTime::currentDateTime())));
    };
    showPreview(pattern->text());
    connect(pattern, &QLineEdit::textChanged, this, [showPreview](const QString &text) {
        Settings::setFileNamePattern(text);
        showPreview(text);
    });
    layout->addSpacing(10);
    auto *saveHint = new QLabel(tr("Ctrl+S speichert direkt in diesem Ordner, der Knopf «Speichern» fragt "
                                   "nach Ordner und Name."), this);
    saveHint->setProperty("role", QStringLiteral("hint"));
    saveHint->setWordWrap(true);
    layout->addWidget(saveHint);

    layout->addStretch(1);

    // --- Farben und Stile (F-22, D-029) ---
    layout = page(tr("Farben und Stile"));
    auto *stylesHint = new QLabel(tr("Die App merkt sich Farben, Linienstärken und Schriftgrössen für neue Objekte "
                                     "und die zuletzt verwendeten eigenen Farben. Bereits gezeichnete Objekte "
                                     "bleiben beim Zurücksetzen, wie sie sind."), this);
    stylesHint->setProperty("role", QStringLiteral("hint"));
    stylesHint->setWordWrap(true);
    layout->addWidget(stylesHint);
    auto *reset = new QPushButton(tr("Auf Standard zurücksetzen"), this);
    reset->setProperty("role", QStringLiteral("secondary"));
    auto *resetRow = new QHBoxLayout;
    resetRow->addWidget(reset);
    resetRow->addStretch(1);
    layout->addSpacing(4);
    layout->addLayout(resetRow);
    connect(reset, &QPushButton::clicked, this, [this, reset] {
        const auto answer = QMessageBox::question(
            this, tr("Farben und Stile zurücksetzen"),
            tr("Farben, Linienstärken und Schriftgrössen für neue Objekte auf den Standard setzen "
               "und die zuletzt verwendeten eigenen Farben vergessen?"));
        if (answer != QMessageBox::Yes)
            return;
        Settings::resetStyles();
        emit stylesReset();
        reset->setText(tr("Zurückgesetzt"));
        reset->setEnabled(false);
    });
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Schliessen"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_advanced, &QCheckBox::toggled, this, [this](bool enabled) {
        Settings::setAdvancedMode(enabled);
        emit advancedModeChanged(enabled);
    });
}
