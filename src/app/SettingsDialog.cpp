#include "app/SettingsDialog.h"

#include "app/CountdownPositionPicker.h"
#include "app/FileNames.h"
#include "app/GlobalHotkeys.h"
#include "app/GnomeExtensionSetup.h"
#include "ui/AppDialog.h"
#include "app/Language.h"
#include "app/Settings.h"
#include "update/UpdateChecker.h"
#include "update/UpdateInstaller.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QScrollArea>
#include <QDateTime>
#include <QDir>
#include <QApplication>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QEventLoop>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>

#include <QVBoxLayout>

#include <algorithm>

namespace {

int openOverlays = 0;
// The card: at most this large (mockup "Einstellungen", D-044).
constexpr int kCardWidth = 880;
constexpr int kCardHeight = 640;
constexpr int kMargin = 24;
constexpr int kNarrowMargin = 16;
// A card narrower than this shows the navigation as a row on top.
constexpr int kNarrowBelow = 720;
constexpr int kNavWidth = 208;

} // namespace

SettingsDialog::SettingsDialog(GlobalHotkeys *hotkeys, bool countdownHasEscape, QWidget *parent)
    : QWidget(parent ? parent->window() : nullptr)
    , m_hotkeys(hotkeys)
{
    // An overlay over the whole window (D-044): it takes every click and
    // key, and follows the window's size.
    setObjectName(QStringLiteral("SettingsOverlay"));
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    if (parentWidget())
        parentWidget()->installEventFilter(this);

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("SettingsCard"));
    m_card->setAttribute(Qt::WA_StyledBackground);

    // "Einstellungen" above the navigation (wide), or in the header (narrow).
    m_navPane = new QWidget(m_card);
    m_navPane->setObjectName(QStringLiteral("SettingsNav"));
    m_navPane->setAttribute(Qt::WA_StyledBackground);
    auto *navLayout = new QVBoxLayout(m_navPane);
    navLayout->setContentsMargins(12, 20, 12, 12);
    navLayout->setSpacing(8);
    m_navTitle = new QLabel(tr("Einstellungen"), m_navPane);
    m_navTitle->setProperty("role", QStringLiteral("settingsTitle"));
    m_navTitle->setContentsMargins(8, 0, 0, 4);
    navLayout->addWidget(m_navTitle);
    m_navButtons = new QWidget;
    m_navButtonsLayout = new QBoxLayout(QBoxLayout::TopToBottom, m_navButtons);
    m_navButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_navButtonsLayout->setSpacing(4);
    m_navScroll = new QScrollArea(m_navPane);
    m_navScroll->setWidget(m_navButtons);
    m_navScroll->setWidgetResizable(true);
    m_navScroll->setFrameShape(QFrame::NoFrame);
    m_navScroll->viewport()->setAutoFillBackground(false);
    m_navButtons->setAutoFillBackground(false);
    navLayout->addWidget(m_navScroll, 1);
    m_navGroup = new QButtonGroup(this);

    // The header: the page's name and the close button.
    m_header = new QWidget(m_card);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(28, 16, 16, 12);
    m_headerTitle = new QLabel(m_header);
    m_headerTitle->setProperty("role", QStringLiteral("pageTitle"));
    headerLayout->addWidget(m_headerTitle, 1);
    auto *close = new QToolButton(m_header);
    close->setObjectName(QStringLiteral("SettingsClose"));
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Schliessen (Esc)"));
    close->setAccessibleName(tr("Einstellungen schliessen"));
    connect(close, &QToolButton::clicked, this, &SettingsDialog::close);
    headerLayout->addWidget(close);

    m_pages = new QStackedWidget(m_card);
    m_cardLayout = new QGridLayout(m_card);
    m_cardLayout->setContentsMargins(0, 0, 0, 0);
    m_cardLayout->setSpacing(0);

    // Each page scrolls inside the card, which is never larger than the
    // window: nothing reaches past the screen (D-044).
    const auto page = [this](const QString &title, const QString &icon) {
        auto *widget = new QWidget;
        auto *pageLayout = new QVBoxLayout(widget);
        pageLayout->setContentsMargins(28, 8, 28, 28);
        pageLayout->setSpacing(6);
        auto *scroll = new QScrollArea;
        scroll->setWidget(widget);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // The card's background shows through, not the scroll area's.
        scroll->viewport()->setAutoFillBackground(false);
        widget->setAutoFillBackground(false);
        m_pages->addWidget(scroll);

        auto *button = new QPushButton(title, m_navButtons);
        button->setProperty("role", QStringLiteral("nav"));
        button->setCheckable(true);
        button->setAutoDefault(false);
        button->setProperty("iconName", icon);
        m_navGroup->addButton(button, m_pages->count() - 1);
        m_navButtonsLayout->addWidget(button);
        m_titles.append(title);
        return pageLayout;
    };

    // --- Allgemein ---
    QVBoxLayout *layout = page(tr("Allgemein"), QStringLiteral("settings"));

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

    // The question before quitting with unsaved work; the dialog's checkbox
    // "in Zukunft nicht mehr nachfragen" turns it off, this turns it on again.
    layout->addSpacing(16);
    auto *quitTitle = new QLabel(tr("Beenden"), this);
    quitTitle->setProperty("role", QStringLiteral("section"));
    layout->addWidget(quitTitle);
    auto *confirmQuit = new QCheckBox(tr("Nachfragen, wenn das Bild weder gespeichert noch kopiert ist"), this);
    confirmQuit->setChecked(Settings::confirmQuitUnsaved());
    layout->addWidget(confirmQuit);
    connect(confirmQuit, &QCheckBox::toggled, this, [](bool enabled) { Settings::setConfirmQuitUnsaved(enabled); });

    layout->addStretch(1);

    // --- Aufnahme: how regions are picked, the countdown, the extension ---
    layout = page(tr("Aufnahme"), QStringLiteral("region"));
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
        createHotkeyPage(page(tr("Tastenkürzel"), QStringLiteral("select")));

    // --- Speichern (F-41): folder and file name pattern ---
    layout = page(tr("Speichern"), QStringLiteral("save"));
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
    layout = page(tr("Farben und Stile"), QStringLiteral("marker"));
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
        AppDialog dialog(this, AppDialog::Kind::Question, tr("Farben und Stile zurücksetzen?"),
                         tr("Farben, Linienstärken und Schriftgrössen für neue Objekte gehen auf den "
                            "Standard zurück, und die zuletzt verwendeten eigenen Farben werden vergessen."));
        dialog.addButton(tr("Abbrechen"), AppDialog::Role::Cancel);
        QPushButton *confirm = dialog.addButton(tr("Zurücksetzen"), AppDialog::Role::Destructive);
        if (dialog.exec() != confirm)
            return;
        Settings::resetStyles();
        emit stylesReset();
        reset->setText(tr("Zurückgesetzt"));
        reset->setEnabled(false);
    });
    layout->addStretch(1);

    // --- Updates (F-49, D-037) ---
    createUpdatePage(page(tr("Updates"), QStringLiteral("redo")));
    m_navButtonsLayout->addStretch(1);
    applyNavIcons();

    // The list of versions loads when its page is opened, if automatic
    // checks are allowed; otherwise only with "Jetzt suchen".
    const int updatePage = m_pages->count() - 1;
    connect(m_navGroup, &QButtonGroup::idClicked, this, [this, updatePage](int index) {
        showPage(index);
        if (index == updatePage && m_releases.isEmpty() && Settings::checkForUpdates())
            checkForUpdates();
    });
    showPage(0);

    connect(m_advanced, &QCheckBox::toggled, this, [this](bool enabled) {
        Settings::setAdvancedMode(enabled);
        emit advancedModeChanged(enabled);
    });

    hide();
}

SettingsDialog::~SettingsDialog()
{
    if (m_hotkeys)
        m_hotkeys->setSuspended(false);
}

bool SettingsDialog::isOpen()
{
    return openOverlays > 0;
}

void SettingsDialog::exec()
{
    QWidget *window = parentWidget();
    if (!window)
        return;
    setGeometry(window->rect());
    arrange();
    show();
    raise();
    // The overlay itself has the focus, so Escape works at once; Tab then
    // walks into the card (no focus ring until then).
    setFocus(Qt::OtherFocusReason);

    // Waits here, as QDialog::exec did, until the overlay is closed or
    // the window goes away; the window's shortcuts rest meanwhile.
    QEventLoop loop;
    m_loop = &loop;
    qApp->installEventFilter(this);
    ++openOverlays;
    loop.exec();
    --openOverlays;
    qApp->removeEventFilter(this);
    m_loop = nullptr;
    hide();
}

void SettingsDialog::close()
{
    if (m_loop)
        m_loop->quit();
}

void SettingsDialog::showPage(int index)
{
    if (QAbstractButton *button = m_navGroup->button(index))
        button->setChecked(true);
    m_pages->setCurrentIndex(index);
    m_headerTitle->setText(m_narrow ? tr("Einstellungen") : m_titles.value(index));
}

void SettingsDialog::applyNavIcons()
{
    const Theme theme = Theme::current();
    for (QAbstractButton *button : m_navGroup->buttons())
        button->setIcon(Icons::toggleIcon(button->property("iconName").toString(), theme.text, theme.softText));
}

void SettingsDialog::arrange()
{
    // At most 880 × 640, never larger than the window minus a margin; below
    // 720 px the navigation moves into a row on top (D-044).
    const QRect area = rect();
    const bool narrow = area.width() < kNarrowBelow + 2 * kMargin;
    const int margin = narrow ? kNarrowMargin : kMargin;
    const QSize size(std::min(kCardWidth, area.width() - 2 * margin), std::min(kCardHeight, area.height() - 2 * margin));
    const QSize cardSize = narrow ? area.size() - QSize(2 * margin, 2 * margin) : size;
    m_card->setGeometry(QRect(QPoint(), cardSize.expandedTo(QSize(1, 1))).translated(area.center() - QRect(QPoint(), cardSize).center()));

    if (narrow == m_narrow && m_cardLayout->count() > 0)
        return;
    m_narrow = narrow;
    while (m_cardLayout->count() > 0)
        m_cardLayout->takeAt(0);
    m_card->setProperty("narrow", narrow);
    m_card->style()->unpolish(m_card);
    m_card->style()->polish(m_card);
    m_navPane->style()->unpolish(m_navPane);
    m_navPane->style()->polish(m_navPane);
    if (narrow) {
        // Header ("Einstellungen", ✕) / row of pages / the page.
        m_navTitle->hide();
        m_navButtonsLayout->setDirection(QBoxLayout::LeftToRight);
        m_navPane->layout()->setContentsMargins(16, 0, 16, 8);
        m_navPane->setMinimumWidth(0);
        m_navPane->setMaximumWidth(QWIDGETSIZE_MAX);
        m_navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_navScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_navScroll->setFixedHeight(m_navButtons->sizeHint().height() + 12);
        m_navPane->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_header->layout()->setContentsMargins(20, 14, 12, 8);
        m_cardLayout->addWidget(m_header, 0, 0);
        m_cardLayout->addWidget(m_navPane, 1, 0);
        m_cardLayout->addWidget(m_pages, 2, 0);
        m_cardLayout->setRowStretch(0, 0);
        m_cardLayout->setRowStretch(1, 0);
        m_cardLayout->setRowStretch(2, 1);
        m_cardLayout->setColumnStretch(0, 1);
        m_cardLayout->setColumnStretch(1, 0);
    } else {
        // Navigation on the left, header and page on the right.
        m_navTitle->show();
        m_navButtonsLayout->setDirection(QBoxLayout::TopToBottom);
        m_navPane->layout()->setContentsMargins(12, 20, 12, 12);
        m_navPane->setFixedWidth(kNavWidth);
        m_navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_navScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_navScroll->setMinimumHeight(0);
        m_navScroll->setMaximumHeight(QWIDGETSIZE_MAX);
        m_navPane->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        m_header->layout()->setContentsMargins(28, 16, 16, 12);
        m_cardLayout->addWidget(m_navPane, 0, 0, 2, 1);
        m_cardLayout->addWidget(m_header, 0, 1);
        m_cardLayout->addWidget(m_pages, 1, 1);
        m_cardLayout->setRowStretch(0, 0);
        m_cardLayout->setRowStretch(1, 1);
        m_cardLayout->setRowStretch(2, 0);
        m_cardLayout->setColumnStretch(0, 0);
        m_cardLayout->setColumnStretch(1, 1);
    }
    showPage(m_pages->currentIndex());
}

void SettingsDialog::paintEvent(QPaintEvent *)
{
    // The dimmed window, and a soft shadow under the card (a few rounded
    // rectangles; a graphics effect would slow down scrolling the pages).
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Theme::current().dark ? QColor(0, 0, 0, 140) : QColor(15, 17, 19, 102));
    painter.setPen(Qt::NoPen);
    const QRectF card = QRectF(m_card->geometry()).translated(0, 10);
    for (int i = 1; i <= 8; ++i) {
        painter.setBrush(QColor(0, 0, 0, 6));
        painter.drawRoundedRect(card.adjusted(-2 * i, -2 * i, 2 * i, 2 * i), 14 + 2 * i, 14 + 2 * i);
    }
}

void SettingsDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    arrange();
}

void SettingsDialog::mousePressEvent(QMouseEvent *event)
{
    // Beside the card: close; nothing is lost, every change applies at once.
    if (!m_card->geometry().contains(event->position().toPoint()))
        close();
    event->accept();
}

void SettingsDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    // No key reaches the window behind the overlay.
    event->accept();
}

bool SettingsDialog::focusNextPrevChild(bool next)
{
    // Tab stays in the overlay: step along Qt's chain until it lands here.
    for (int i = 0; i < 200; ++i) {
        if (!QWidget::focusNextPrevChild(next))
            return false;
        QWidget *focused = QApplication::focusWidget();
        if (focused && isAncestorOf(focused))
            return true;
    }
    return true;
}

bool SettingsDialog::eventFilter(QObject *watched, QEvent *event)
{
    // The window changed size: so does the overlay.
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    // The window went away (tray, quit): close.
    if (watched == parentWidget() && event->type() == QEvent::Hide)
        close();
    // The window's shortcuts rest while the overlay is open, except
    // those of widgets inside it (and of dialogs on top of it).
    if (event->type() == QEvent::Shortcut && m_loop) {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget || !(widget == this || isAncestorOf(widget) || widget->window() != window()))
            return true;
    }
    if (event->type() == QEvent::ThemeChange || event->type() == QEvent::PaletteChange)
        applyNavIcons();
    return QWidget::eventFilter(watched, event);
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
