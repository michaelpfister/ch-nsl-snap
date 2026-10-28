#include "app/SettingsDialog.h"

#include "app/FileNames.h"
#include "app/GlobalHotkeys.h"
#include "app/Settings.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QRadioButton>
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
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(GlobalHotkeys *hotkeys, QWidget *parent)
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

SettingsDialog::~SettingsDialog()
{
    if (m_hotkeys)
        m_hotkeys->setSuspended(false);
}

void SettingsDialog::createHotkeyPage(QVBoxLayout *layout)
{
    auto *intro = new QLabel(tr("Diese Tasten nehmen auf, auch wenn NSL Snap im Infobereich läuft oder ein anderes "
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
                                    "Taste NSL Snap nicht. Abhilfe: in den Windows-Einstellungen unter "
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
