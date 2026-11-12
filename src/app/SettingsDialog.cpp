#include "app/SettingsDialog.h"

#include "app/CountdownPositionPicker.h"
#include "app/FileNames.h"
#include "app/GlobalHotkeys.h"
#include "app/GnomeExtensionSetup.h"
#include "app/Language.h"
#include "app/Settings.h"
#include "update/UpdateChecker.h"
#include "update/UpdateInstaller.h"
#include "ui/Placement.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QScrollArea>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QApplication>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTabWidget>

#include <algorithm>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(GlobalHotkeys *hotkeys, bool countdownHasEscape, QWidget *parent)
    : QDialog(parent)
    , m_hotkeys(hotkeys)
{
    setWindowTitle(tr("Einstellungen"));
    setMinimumWidth(540);

    // Three tabs keep the dialog small enough for laptop screens.
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 12);
    auto *tabs = new QTabWidget(this);
    outer->addWidget(tabs);
    // Each page scrolls, so the dialog fits small or highly scaled screens
    // (fitIntoScreen below); usually the scroll bar never shows.
    const auto page = [this, tabs](const QString &title) {
        auto *widget = new QWidget(this);
        auto *pageLayout = new QVBoxLayout(widget);
        pageLayout->setContentsMargins(16, 16, 16, 16);
        pageLayout->setSpacing(6);
        auto *scroll = new QScrollArea(this);
        scroll->setWidget(widget);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // The page's own background shows through, not the scroll area's.
        scroll->viewport()->setAutoFillBackground(false);
        widget->setAutoFillBackground(false);
        tabs->addTab(scroll, title);
        return pageLayout;
    };

    // --- Allgemein ---
    QVBoxLayout *layout = page(tr("Allgemein"));

    // Language (D-038): applies at the next start.
    auto *languageTitle = new QLabel(tr("Sprache"), this);
    languageTitle->setProperty("role", QStringLiteral("section"));
    layout->addWidget(languageTitle);
    auto *language = new QComboBox(this);
    language->setAccessibleName(tr("Sprache"));
    // The system's choice names the language it resolves to right now.
    const QString systemLanguage = Language::resolve(QString(), QLocale::system().uiLanguages());
    language->addItem(tr("Wie das System (%1)").arg(Language::nativeName(systemLanguage)), QString());
    for (const QString &code : Language::available())
        language->addItem(Language::nativeName(code), code);
    language->setCurrentIndex(std::max(0, language->findData(Settings::language())));
    auto *languageRow = new QHBoxLayout;
    languageRow->addWidget(language);
    languageRow->addStretch(1);
    layout->addLayout(languageRow);
    auto *languageHint = new QLabel(tr("Gilt nach einem Neustart von Snapslash."), this);
    languageHint->setProperty("role", QStringLiteral("hint"));
    languageHint->setWordWrap(true);
    layout->addWidget(languageHint);
    connect(language, &QComboBox::currentIndexChanged, this,
            [language] { Settings::setLanguage(language->currentData().toString()); });
    layout->addSpacing(16);


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

    createCountdownSection(layout, countdownHasEscape);
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

    // The extension inside the AppImage (D-041): its state, and setting it
    // up again, e.g. after "Später" or when it was removed.
    if (GnomeExtensionSetup::isAvailable()) {
        layout->addSpacing(16);
        auto *extensionTitle = new QLabel(tr("GNOME-Erweiterung"), this);
        extensionTitle->setProperty("role", QStringLiteral("section"));
        layout->addWidget(extensionTitle);
        auto *extensionState = new QLabel(GnomeExtensionSetup::stateText(), this);
        extensionState->setWordWrap(true);
        layout->addWidget(extensionState);
        auto *extensionHint = new QLabel(tr("Über sie wählt Snapslash unter GNOME Fenster und Bereiche aus. "
                                            "Sie kommt mit der App und wird in den eigenen Benutzerordner installiert."), this);
        extensionHint->setProperty("role", QStringLiteral("hint"));
        extensionHint->setWordWrap(true);
        layout->addWidget(extensionHint);
        layout->addSpacing(4);
        auto *extensionRow = new QHBoxLayout;
        auto *setUp = new QPushButton(tr("GNOME-Erweiterung einrichten …"), this);
        setUp->setProperty("role", QStringLiteral("secondary"));
        extensionRow->addWidget(setUp);
        extensionRow->addStretch(1);
        layout->addLayout(extensionRow);
        connect(setUp, &QPushButton::clicked, this, [this, extensionState] {
            GnomeExtensionSetup::offer(this);
            extensionState->setText(GnomeExtensionSetup::stateText());
        });
    }

    layout->addStretch(1);

    // --- Tastenkürzel (F-42) ---
    if (m_hotkeys)
        createHotkeyPage(page(tr("Tastenkürzel")));

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
    layout->addSpacing(16);

    // Series of captures (F-52).
    auto *seriesTitle = new QLabel(tr("Serienaufnahme"), this);
    seriesTitle->setProperty("role", QStringLiteral("section"));
    layout->addWidget(seriesTitle);
    auto *series = new QCheckBox(tr("Jede Aufnahme sofort in diesem Ordner speichern"), this);
    series->setChecked(Settings::autoSaveCaptures());
    layout->addWidget(series);
    auto *seriesHint = new QLabel(tr("So lassen sich mehrere Aufnahmen hintereinander machen: Sie öffnen sich nicht "
                                     "im Editor, sondern erscheinen in der Liste «Letzte Aufnahmen» unten im Fenster. "
                                     "Ein Klick darauf öffnet eine Aufnahme im Editor."), this);
    seriesHint->setProperty("role", QStringLiteral("hint"));
    seriesHint->setWordWrap(true);
    seriesHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(seriesHint);
    connect(series, &QCheckBox::toggled, this, [this](bool enabled) {
        Settings::setAutoSaveCaptures(enabled);
        emit autoSaveChanged(enabled);
    });

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

    // --- Updates (F-49, D-037) ---
    createUpdatePage(page(tr("Updates")));
    // The list of versions loads when the tab is opened, if automatic
    // checks are allowed; otherwise only with "Jetzt suchen".
    const int updateTab = tabs->count() - 1;
    connect(tabs, &QTabWidget::currentChanged, this, [this, updateTab](int index) {
        if (index == updateTab && m_releases.isEmpty() && Settings::checkForUpdates())
            checkForUpdates();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Schliessen"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_advanced, &QCheckBox::toggled, this, [this](bool enabled) {
        Settings::setAdvancedMode(enabled);
        emit advancedModeChanged(enabled);
    });

    // A scroll area asks only for a small size, so the pages' own sizes
    // decide how large the dialog wants to be; then it is limited to the
    // screen, so its title bar stays reachable.
    int pageHeight = 0;
    int pageWidth = 0;
    for (int i = 0; i < tabs->count(); ++i) {
        const QSize hint = static_cast<QScrollArea *>(tabs->widget(i))->widget()->sizeHint();
        pageHeight = std::max(pageHeight, hint.height());
        pageWidth = std::max(pageWidth, hint.width());
    }
    const QSize chrome = sizeHint() - tabs->currentWidget()->sizeHint();
    resize(QSize(std::max(minimumWidth(), pageWidth + chrome.width()), pageHeight + chrome.height()).expandedTo(minimumSize()));
    fitIntoScreen(this);
}

SettingsDialog::~SettingsDialog()
{
    if (m_hotkeys)
        m_hotkeys->setSuspended(false);
}

void SettingsDialog::createCountdownSection(QVBoxLayout *layout, bool hasEscape)
{
    auto *title = new QLabel(tr("Countdown des Timers"), this);
    title->setProperty("role", QStringLiteral("section"));
    layout->addWidget(title);

    // Where it appears: the drawn monitor, with the chosen place beside it.
    const CountdownOptions options = Settings::countdownOptions();
    auto *picker = new CountdownPositionPicker(this);
    picker->setPosition(options.position);
    auto *positionName = new QLabel(CountdownPositionPicker::positionName(options.position), this);
    auto *positionHint = new QLabel(tr("Ort der Anzeige auf dem Hauptbildschirm. Punkt anklicken, um ihn zu ändern."), this);
    positionHint->setProperty("role", QStringLiteral("hint"));
    positionHint->setWordWrap(true);
    auto *text = new QVBoxLayout;
    text->setSpacing(4);
    text->addStretch(1);
    text->addWidget(positionName);
    text->addWidget(positionHint);
    text->addStretch(1);
    auto *row = new QHBoxLayout;
    row->setSpacing(16);
    row->addWidget(picker);
    row->addLayout(text, 1);
    layout->addLayout(row);
    connect(picker, &CountdownPositionPicker::positionChanged, this, [positionName](CountdownPosition position) {
        Settings::setCountdownPosition(position);
        positionName->setText(CountdownPositionPicker::positionName(position));
    });
    layout->addSpacing(6);

    // The click: off, clicks go through it and only Escape cancels.
    auto *click = new QCheckBox(tr("Klick auf den Countdown bricht die Aufnahme ab"), this);
    click->setChecked(hasEscape ? options.clickCancels : true);
    click->setEnabled(hasEscape);
    layout->addWidget(click);
    auto *clickHint = new QLabel(hasEscape ? tr("Ausgeschaltet gehen Klicks durch den Countdown hindurch, "
                                                "zum Beispiel auf einen Knopf darunter; abbrechen dann nur mit Esc.")
                                           : tr("Auf diesem Desktop erreicht Esc den Countdown nicht, daher bricht "
                                                "ein Klick immer ab."), this);
    clickHint->setProperty("role", QStringLiteral("hint"));
    clickHint->setWordWrap(true);
    clickHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(clickHint);
    connect(click, &QCheckBox::toggled, this, [](bool enabled) { Settings::setCountdownClickCancels(enabled); });
}

void SettingsDialog::createHotkeyPage(QVBoxLayout *layout)
{
    auto *intro = new QLabel(tr("Diese Tasten nehmen auf, auch wenn Snapslash im Infobereich läuft oder ein anderes "
                                "Programm aktiv ist. Ein eingestellter Timer gilt auch hier."), this);
    intro->setProperty("role", QStringLiteral("hint"));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    layout->addSpacing(10);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

    QList<QKeySequenceEdit *> edits;
    QList<QLabel *> states;
    int row = 0;
    for (GlobalHotkeys::Action action : GlobalHotkeys::actions()) {
        auto *name = new QLabel(GlobalHotkeys::actionName(action), this);
        auto *edit = new QKeySequenceEdit(m_hotkeys->key(action), this);
        edit->setMaximumSequenceLength(1); // one key with modifiers
        edit->setClearButtonEnabled(true); // no key: switched off
        edit->setAccessibleName(GlobalHotkeys::actionName(action));
        auto *standard = new QPushButton(tr("Standard"), this);
        standard->setProperty("role", QStringLiteral("secondary"));
        standard->setToolTip(GlobalHotkeys::defaultKey(action).toString(QKeySequence::NativeText));
        auto *state = new QLabel(this);
        state->setProperty("role", QStringLiteral("warning"));
        state->setWordWrap(true);
        grid->addWidget(name, row, 0);
        grid->addWidget(edit, row, 1);
        grid->addWidget(standard, row, 2);
        grid->addWidget(state, row + 1, 1, 1, 2);
        row += 2;
        edits << edit;
        states << state;

        const auto assign = [this, action](const QKeySequence &key) {
            Settings::setGlobalHotkey(action, key);
            m_hotkeys->setKey(action, key);
        };
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, assign);
        connect(standard, &QPushButton::clicked, this, [edit, action] {
            edit->setKeySequence(GlobalHotkeys::defaultKey(action)); // calls assign
        });
    }

    // Windows' own use of Print (D-035): a note with the way out.
    layout->addSpacing(12);
    auto *printNote = new QLabel(tr("Windows öffnet mit der Druck-Taste das Snipping Tool, deshalb erreicht die "
                                    "Taste Snapslash nicht. Abhilfe: in den Windows-Einstellungen unter "
                                    "«Barrierefreiheit › Tastatur» den Schalter zur Druck-Taste ausschalten."), this);
    printNote->setProperty("role", QStringLiteral("warning"));
    printNote->setWordWrap(true);
    layout->addWidget(printNote);
    auto *openSetting = new QPushButton(tr("Windows-Einstellung öffnen"), this);
    openSetting->setProperty("role", QStringLiteral("secondary"));
    auto *settingRow = new QHBoxLayout;
    settingRow->addWidget(openSetting);
    settingRow->addStretch(1);
    layout->addLayout(settingRow);
    connect(openSetting, &QPushButton::clicked, this, [] { GlobalHotkeys::openPrintKeySetting(); });
    layout->addStretch(1);

    const auto showStates = [this, states, printNote, openSetting] {
        bool usesPrint = false;
        const QList<GlobalHotkeys::Action> actions = GlobalHotkeys::actions();
        for (int i = 0; i < actions.size(); ++i) {
            const QKeySequence key = m_hotkeys->key(actions[i]);
            usesPrint = usesPrint || key == QKeySequence(Qt::Key_Print);
            unsigned modifiers = 0;
            unsigned virtualKey = 0;
            QString text;
            if (!key.isEmpty() && !GlobalHotkeys::toNative(key, modifiers, virtualKey))
                text = tr("Diese Taste kann nicht global verwendet werden.");
            else if (m_hotkeys->isTaken(actions[i]))
                text = tr("Belegt durch ein anderes Programm oder durch Windows.");
            states[i]->setText(text);
            states[i]->setVisible(!text.isEmpty());
        }
        const bool blocked = usesPrint && GlobalHotkeys::printKeyOpensSnippingTool();
        printNote->setVisible(blocked);
        openSetting->setVisible(blocked);
    };
    showStates();
    connect(m_hotkeys, &GlobalHotkeys::changed, this, showStates);
    // The Windows setting may change while the dialog is open.
    connect(qApp, &QApplication::applicationStateChanged, this, showStates);

    // A registered key never reaches the app's own windows, so the hotkeys
    // pause while a key field has the focus; leaving it registers again
    // and shows whether the new key is free.
    connect(qApp, &QApplication::focusChanged, this, [this, edits](QWidget *, QWidget *now) {
        bool editing = false;
        for (QKeySequenceEdit *edit : edits)
            editing = editing || (now && (now == edit || edit->isAncestorOf(now)));
        m_hotkeys->setSuspended(editing);
    });
}

void SettingsDialog::createUpdatePage(QVBoxLayout *layout)
{
    m_updates = new UpdateChecker(this);

    auto *versionTitle = new QLabel(tr("Version"), this);
    versionTitle->setProperty("role", QStringLiteral("section"));
    layout->addWidget(versionTitle);
    QString kind;
    switch (Releases::installation()) {
    case Releases::Installation::PortableWindows: kind = tr("portable Windows-Version"); break;
    case Releases::Installation::AppImage: kind = tr("AppImage"); break;
    case Releases::Installation::Nix: kind = tr("über Nix installiert"); break;
    case Releases::Installation::Other: kind = tr("selbst gebaut"); break;
    }
    layout->addWidget(new QLabel(tr("Snapslash %1 · %2").arg(Version::current().toString(), kind), this));
    layout->addSpacing(16);

    auto *checkTitle = new QLabel(tr("Nach Updates suchen"), this);
    checkTitle->setProperty("role", QStringLiteral("section"));
    layout->addWidget(checkTitle);
    auto *automatic = new QCheckBox(tr("Beim Start automatisch suchen, höchstens einmal pro Tag"), this);
    automatic->setChecked(Settings::checkForUpdates());
    layout->addWidget(automatic);
    auto *prereleases = new QCheckBox(tr("Auch Vorabversionen anbieten"), this);
    prereleases->setChecked(Settings::offerPrereleases());
    layout->addWidget(prereleases);
    auto *privacy = new QLabel(tr("Die App fragt dazu GitHub nach den veröffentlichten Versionen von Snapslash. "
                                  "Übertragen wird nur diese Anfrage, keine Aufnahmen und keine Einstellungen."), this);
    privacy->setProperty("role", QStringLiteral("hint"));
    privacy->setWordWrap(true);
    layout->addWidget(privacy);
    layout->addSpacing(4);
    auto *checkRow = new QHBoxLayout;
    m_checkButton = new QPushButton(tr("Jetzt suchen"), this);
    m_checkButton->setProperty("role", QStringLiteral("secondary"));
    m_updateStatus = new QLabel(this);
    m_updateStatus->setProperty("role", QStringLiteral("hint"));
    m_updateStatus->setWordWrap(true);
    checkRow->addWidget(m_checkButton);
    checkRow->addWidget(m_updateStatus, 1);
    layout->addLayout(checkRow);
    layout->addSpacing(16);

    // Downgrade: any published version, in case a new one misbehaves.
    auto *otherTitle = new QLabel(tr("Andere Version installieren"), this);
    otherTitle->setProperty("role", QStringLiteral("section"));
    layout->addWidget(otherTitle);
    auto *otherHint = new QLabel(tr("Falls eine neue Version Probleme macht, lässt sich hier eine frühere wieder "
                                    "installieren. Die Einstellungen bleiben erhalten."), this);
    otherHint->setProperty("role", QStringLiteral("hint"));
    otherHint->setWordWrap(true);
    layout->addWidget(otherHint);
    auto *versionRow = new QHBoxLayout;
    m_versions = new QComboBox(this);
    m_versions->setAccessibleName(tr("Version"));
    m_versions->setEnabled(false);
    m_versions->addItem(tr("Noch nicht geladen"));
    m_installVersion = new QPushButton(UpdateInstaller::isSupported() ? tr("Installieren …") : tr("Anleitung …"), this);
    m_installVersion->setProperty("role", QStringLiteral("secondary"));
    m_installVersion->setEnabled(false);
    versionRow->addWidget(m_versions, 1);
    versionRow->addWidget(m_installVersion);
    layout->addLayout(versionRow);
    layout->addStretch(1);

    connect(automatic, &QCheckBox::toggled, this, [](bool enabled) { Settings::setCheckForUpdates(enabled); });
    connect(prereleases, &QCheckBox::toggled, this, [this](bool enabled) {
        Settings::setOfferPrereleases(enabled);
        if (!m_releases.isEmpty()) {
            fillVersions();
            emit releasesLoaded(m_releases); // the update button follows
        }
    });
    connect(m_checkButton, &QPushButton::clicked, this, &SettingsDialog::checkForUpdates);
    connect(m_installVersion, &QPushButton::clicked, this, [this] {
        const int index = m_versions->currentData().toInt();
        const QList<Releases::Release> offered =
            Releases::offered(m_releases, Settings::offerPrereleases(), Version::current());
        if (index >= 0 && index < offered.size())
            emit installRequested(offered[index]);
    });
    connect(m_updates, &UpdateChecker::finished, this, [this](const QList<Releases::Release> &releases) {
        m_releases = releases;
        m_checkButton->setEnabled(true);
        Settings::setLastUpdateCheck(QDateTime::currentDateTimeUtc());
        fillVersions();
        emit releasesLoaded(releases);
    });
    connect(m_updates, &UpdateChecker::failed, this, [this](const QString &message) {
        m_checkButton->setEnabled(true);
        m_updateStatus->setText(message);
    });
}

void SettingsDialog::checkForUpdates()
{
    m_checkButton->setEnabled(false);
    m_updateStatus->setText(tr("Wird gesucht …"));
    m_updates->check();
}

void SettingsDialog::fillVersions()
{
    const Version installed = Version::current();
    const bool prereleases = Settings::offerPrereleases();
    const std::optional<Releases::Release> newer = Releases::newerThan(m_releases, installed, prereleases);
    if (m_releases.isEmpty())
        m_updateStatus->setText(tr("Es ist noch keine Version veröffentlicht."));
    else if (newer)
        m_updateStatus->setText(tr("Version %1 ist verfügbar.").arg(newer->version.toString()));
    else
        m_updateStatus->setText(tr("Snapslash ist auf dem neusten Stand."));

    // Item data: the index in the offered list, newest first.
    m_versions->clear();
    const QList<Releases::Release> offered = Releases::offered(m_releases, prereleases, installed);
    int current = -1;
    for (int i = 0; i < offered.size(); ++i) {
        const Releases::Release &release = offered[i];
        QString text = release.version.toString();
        if (release.published.isValid())
            text += QStringLiteral(" · ") + release.published.toLocalTime().date().toString(QStringLiteral("d.M.yyyy"));
        if (release.version == installed) {
            text += tr(" (installiert)");
            current = i;
        } else if (release.prerelease) {
            text += tr(" (Vorabversion)");
        }
        m_versions->addItem(text, i);
    }
    // Preselect the version just below the installed one: the usual reason
    // to come here is to go back one step.
    if (current >= 0 && current + 1 < offered.size())
        m_versions->setCurrentIndex(current + 1);
    if (offered.isEmpty())
        m_versions->addItem(tr("Keine Versionen veröffentlicht"));
    m_versions->setEnabled(!offered.isEmpty());
    m_installVersion->setEnabled(!offered.isEmpty());
}
