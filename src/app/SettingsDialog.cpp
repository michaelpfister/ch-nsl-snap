#include "app/SettingsDialog.h"

#include "app/FileNames.h"
#include "app/Settings.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QGridLayout>
#include <QUrl>
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

#include <algorithm>
#include <functional>

namespace {

// A global hotkey as two lists: modifiers and key. Recording a key press
// would not work for Print, which the desktop keeps for itself.
class HotkeyEdit : public QWidget
{
public:
    std::function<void()> onChanged;

    explicit HotkeyEdit(QWidget *parent)
        : QWidget(parent)
        , m_modifiers(new QComboBox(this))
        , m_key(new QComboBox(this))
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        const QList<std::pair<QString, Qt::KeyboardModifiers>> modifiers = {
            {tr("–"), Qt::NoModifier},
            {QStringLiteral("Shift"), Qt::ShiftModifier},
            {QStringLiteral("Ctrl"), Qt::ControlModifier},
            {QStringLiteral("Alt"), Qt::AltModifier},
            {QStringLiteral("Ctrl + Alt"), Qt::ControlModifier | Qt::AltModifier},
            {QStringLiteral("Ctrl + Shift"), Qt::ControlModifier | Qt::ShiftModifier},
            {QStringLiteral("Alt + Shift"), Qt::AltModifier | Qt::ShiftModifier},
            {QStringLiteral("Ctrl + Alt + Shift"), Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier},
            {QStringLiteral("Super"), Qt::MetaModifier},
            {QStringLiteral("Super + Shift"), Qt::MetaModifier | Qt::ShiftModifier},
        };
        for (const auto &[name, mods] : modifiers)
            m_modifiers->addItem(name, int(mods));
        m_key->addItem(tr("Aus"), 0);
        for (Qt::Key key : HotkeyKeys::offeredKeys())
            m_key->addItem(QKeySequence(key).toString(QKeySequence::NativeText), int(key));
        m_modifiers->setAccessibleName(tr("Zusatztasten"));
        m_key->setAccessibleName(tr("Taste"));
        layout->addWidget(m_modifiers, 1);
        layout->addWidget(new QLabel(QStringLiteral("+"), this));
        layout->addWidget(m_key, 1);
        const auto changed = [this] {
            m_modifiers->setEnabled(m_key->currentData().toInt() != 0);
            if (onChanged)
                onChanged();
        };
        connect(m_modifiers, &QComboBox::activated, this, changed);
        connect(m_key, &QComboBox::activated, this, changed);
    }

    QKeySequence key() const
    {
        const int key = m_key->currentData().toInt();
        if (key == 0)
            return {};
        return QKeySequence(QKeyCombination(Qt::KeyboardModifiers(m_modifiers->currentData().toInt()), Qt::Key(key)));
    }

    void setKey(const QKeySequence &sequence)
    {
        const QKeyCombination combination = sequence.isEmpty() ? QKeyCombination() : sequence[0];
        const int keyRow = m_key->findData(sequence.isEmpty() ? 0 : int(combination.key()));
        m_key->setCurrentIndex(std::max(0, keyRow));
        const int modifierRow = m_modifiers->findData(int(combination.keyboardModifiers()));
        m_modifiers->setCurrentIndex(std::max(0, modifierRow));
        m_modifiers->setEnabled(keyRow > 0);
    }

private:
    QComboBox *m_modifiers;
    QComboBox *m_key;
};

} // namespace

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

    // --- Tastenkürzel (F-42, D-031) ---
    layout = page(tr("Tastenkürzel"));
    const GlobalHotkeys hotkeys = Settings::globalHotkeys();
    auto *hotkeysOn = new QCheckBox(tr("Globale Tastenkürzel verwenden"), this);
    hotkeysOn->setChecked(hotkeys.enabled);
    layout->addWidget(hotkeysOn);
    auto *hotkeysHint = new QLabel(tr("Sie wirken auch, wenn ein anderes Programm aktiv ist oder NSL Snap "
                                      "im Infobereich läuft. Der Timer gilt auch für sie."), this);
    hotkeysHint->setProperty("role", QStringLiteral("hint"));
    hotkeysHint->setWordWrap(true);
    hotkeysHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(hotkeysHint);
    layout->addSpacing(8);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    const QList<std::pair<HotkeyAction, QString>> actions = {
        {HotkeyAction::Last, tr("Zuletzt verwendete Art")},
        {HotkeyAction::Screen, tr("Bildschirm unter dem Mauszeiger")},
        {HotkeyAction::Window, tr("Aktives Fenster")},
        {HotkeyAction::Region, tr("Bereich")},
    };
    QList<std::pair<HotkeyAction, HotkeyEdit *>> edits;
    for (const auto &[action, name] : actions) {
        auto *edit = new HotkeyEdit(this);
        edit->setKey(hotkeys.key(action));
        grid->addWidget(new QLabel(name, this), grid->rowCount(), 0);
        grid->addWidget(edit, grid->rowCount() - 1, 1);
        edits.append({action, edit});
    }
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

#ifdef NSL_HAVE_GNOME_BACKEND
    auto *takeOver = new QCheckBox(tr("Gleiche Tasten von GNOMEs Bildschirmfoto-Funktion übernehmen"), this);
    takeOver->setChecked(hotkeys.takeOverSystemKeys);
    layout->addSpacing(8);
    layout->addWidget(takeOver);
    auto *takeOverHint = new QLabel(tr("Sonst öffnet GNOME bei Print weiterhin sein eigenes Bildschirmfoto. "
                                       "Beim Deaktivieren der Erweiterung bekommt GNOME die Tasten zurück."), this);
    takeOverHint->setProperty("role", QStringLiteral("hint"));
    takeOverHint->setWordWrap(true);
    takeOverHint->setContentsMargins(26, 0, 0, 0);
    layout->addWidget(takeOverHint);
#endif
#ifdef Q_OS_WIN
    // Q-15: Windows 11 opens the Snipping Tool on Print unless this is off.
    auto *windowsHint = new QLabel(tr("Windows 11 öffnet bei Print sonst das Snipping Tool: dafür in den "
                                      "Windows-Einstellungen «Bildschirmaufnahme mit der Druck-Taste öffnen» "
                                      "ausschalten."), this);
    windowsHint->setProperty("role", QStringLiteral("hint"));
    windowsHint->setWordWrap(true);
    auto *openWindowsSettings = new QPushButton(tr("Windows-Einstellung öffnen"), this);
    openWindowsSettings->setProperty("role", QStringLiteral("secondary"));
    connect(openWindowsSettings, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:easeofaccess-keyboard"))); });
    layout->addSpacing(8);
    layout->addWidget(windowsHint);
    auto *windowsRow = new QHBoxLayout;
    windowsRow->addWidget(openWindowsSettings);
    windowsRow->addStretch(1);
    layout->addLayout(windowsRow);
#endif

    m_hotkeyProblem = new QLabel(this);
    m_hotkeyProblem->setProperty("role", QStringLiteral("warning"));
    m_hotkeyProblem->setWordWrap(true);
    m_hotkeyProblem->hide();
    layout->addSpacing(8);
    layout->addWidget(m_hotkeyProblem);

    auto *defaults = new QPushButton(tr("Standard-Tasten"), this);
    defaults->setProperty("role", QStringLiteral("secondary"));
    auto *defaultsRow = new QHBoxLayout;
    defaultsRow->addWidget(defaults);
    defaultsRow->addStretch(1);
    layout->addSpacing(8);
    layout->addLayout(defaultsRow);
    layout->addStretch(1);

    // Every change is stored and applied at once.
    const auto store = [this, hotkeysOn, edits
#ifdef NSL_HAVE_GNOME_BACKEND
                        , takeOver
#endif
    ] {
        GlobalHotkeys keys = Settings::globalHotkeys();
        keys.enabled = hotkeysOn->isChecked();
#ifdef NSL_HAVE_GNOME_BACKEND
        keys.takeOverSystemKeys = takeOver->isChecked();
#endif
        for (const auto &[action, edit] : edits) {
            edit->setEnabled(keys.enabled);
            keys.setKey(action, edit->key());
        }
        Settings::setGlobalHotkeys(keys);
        emit hotkeysChanged();
    };
    for (const auto &[action, edit] : edits) {
        edit->setEnabled(hotkeys.enabled);
        edit->onChanged = store;
    }
    connect(hotkeysOn, &QCheckBox::toggled, this, store);
#ifdef NSL_HAVE_GNOME_BACKEND
    connect(takeOver, &QCheckBox::toggled, this, store);
#endif
    connect(defaults, &QPushButton::clicked, this, [edits, store] {
        const GlobalHotkeys standard;
        for (const auto &[action, edit] : edits)
            edit->setKey(standard.key(action));
        store();
    });

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

    connect(drag, &QRadioButton::toggled, this, &SettingsDialog::hotkeysChanged); // hotkey regions too

    connect(m_advanced, &QCheckBox::toggled, this, [this](bool enabled) {
        Settings::setAdvancedMode(enabled);
        emit advancedModeChanged(enabled);
    });
}

void SettingsDialog::setHotkeyProblem(const QString &problem)
{
    m_hotkeyProblem->setText(problem);
    m_hotkeyProblem->setVisible(!problem.isEmpty());
}
