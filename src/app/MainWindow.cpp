#include "app/MainWindow.h"
#include "app/FileNames.h"
#ifdef NSL_HAVE_GNOME_BACKEND
#include "app/AppService.h"
#endif

#include "app/PropertiesPanel.h"
#include "app/Settings.h"
#include "app/SettingsDialog.h"
#include "app/ToolRail.h"
#include "capture/CaptureBackend.h"
#include "editor/ArrowItem.h"
#include "editor/Commands.h"
#include "editor/EditorScene.h"
#include "editor/EditorView.h"
#include "editor/FrameItem.h"
#include "editor/MarkerItem.h"
#include "editor/RedactItem.h"
#include "editor/ReplaceItem.h"
#include "editor/TextItem.h"
#include "imaging/WindowImage.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QAction>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QUrl>

#include <algorithm>

#ifdef NSL_HAVE_GNOME_BACKEND
#include <QDBusConnection>
#include <QDBusMessage>
#endif

namespace {

// Time the compositor needs to unmap our window before the capture starts,
// so the app does not show up in it (D-011).
constexpr int kHideDelayMs = 250;

QToolButton *segmentButton(const QString &text, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIconSize(QSize(16, 16));
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QToolButton *iconButton(const QString &tooltip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setProperty("role", QStringLiteral("icon"));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QPushButton *textButton(const QString &text, const QString &role, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setProperty("role", role);
    button->setIconSize(QSize(16, 16));
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_undoStack(new QUndoStack(this))
    , m_view(new EditorView(m_undoStack, this))
    , m_toolRail(new ToolRail(this))
    , m_properties(new PropertiesPanel(this))
    , m_capture(createCaptureBackend())
{
    setWindowTitle(QStringLiteral("NSL Snap"));
    resize(1280, 800);

    // Layout: top bar above [tool rail | editor | properties].
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("Central"));
    auto *column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(createTopBar());
    auto *row = new QHBoxLayout;
    row->setSpacing(0);
    row->addWidget(m_toolRail);
    row->addWidget(m_view, 1);
    row->addWidget(m_properties);
    column->addLayout(row, 1);
    setCentralWidget(central);

    m_imageInfo = new QLabel(this);
    m_toolHint = new QLabel(this);
    statusBar()->addWidget(m_imageInfo, 1);
    statusBar()->addPermanentWidget(m_toolHint);
    statusBar()->setSizeGripEnabled(false);

    connect(m_view, &EditorView::fileDropped, this, &MainWindow::loadImageFile);
    connect(m_view, &EditorView::zoomChanged, this, &MainWindow::updateZoomLabel);
    connect(m_toolRail, &ToolRail::toolChanged, this, &MainWindow::onToolChanged);

    // Selection and object list (D-012).
    EditorScene *scene = m_view->editorScene();
    loadDefaultStyles();
    connect(scene, &QGraphicsScene::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(scene, &EditorScene::annotationsChanged, this, &MainWindow::updateObjectList);
    connect(m_undoStack, &QUndoStack::indexChanged, this, &MainWindow::onSelectionChanged);
    connect(m_properties, &PropertiesPanel::frameStyleChanged, this, &MainWindow::onFrameStyleChanged);
    connect(m_properties, &PropertiesPanel::redactStyleChanged, this, &MainWindow::onRedactStyleChanged);
    connect(m_properties, &PropertiesPanel::replaceStyleChanged, this, &MainWindow::onReplaceStyleChanged);
    connect(m_properties, &PropertiesPanel::textStyleChanged, this, &MainWindow::onTextStyleChanged);
    connect(m_properties, &PropertiesPanel::arrowStyleChanged, this, &MainWindow::onArrowStyleChanged);
    connect(m_properties, &PropertiesPanel::markerStyleChanged, this, &MainWindow::onMarkerStyleChanged);
    // Eyedropper of the colour popup: one click into the image.
    connect(m_properties, &PropertiesPanel::eyedropperRequested, this, [this] {
        m_view->startColorPick();
        if (m_view->isPickingColor())
            m_toolHint->setText(tr("Klicken, um die Farbe zu übernehmen · Esc bricht ab"));
    });
    connect(m_view, &EditorView::colorPicked, this, [this](const QColor &color) {
        m_toolHint->setText(toolInfo(m_toolRail->currentTool()).hint);
        m_properties->finishEyedropper(color);
    });
    connect(m_properties, &PropertiesPanel::rematchRequested, this, [this] {
        if (ReplaceItem *item = selectedReplace()) {
            // Recognise on a copy, then push the result as one undo step.
            const ReplaceStyle before = item->style();
            item->matchOriginal();
            const ReplaceStyle matched = item->style();
            item->setStyle(before);
            m_undoStack->push(new SetReplaceStyleCommand(item, matched));
        }
    });
    connect(m_view, &EditorView::replaceCutsText, this, [this] {
        statusBar()->showMessage(tr("Der Bereich schneidet den Originaltext an: Teile davon bleiben sichtbar. "
                                    "Bereich an den Griffen vergrössern."), 8000);
    });
    connect(m_view, &EditorView::replaceNoPlainBackground, this, [this] {
        statusBar()->showMessage(tr("Kein einfarbiger Hintergrund: Die Fläche ist mit der Hauptfarbe gefüllt, "
                                    "der Text mittig gesetzt. Farben und Grösse im Panel anpassen."), 8000);
    });
    // Right after drawing a replaced text, the cursor is in the text field.
    connect(m_view, &EditorView::annotationCreated, this, [this](RectItem *item) {
        if (item->type() == ReplaceItem::Type)
            m_properties->focusReplaceText();
    });
    // Crop (F-12): the panel shows the draft's size and confirms or resets it.
    connect(m_view, &EditorView::cropDraftChanged, this, [this](const QRect &rect) {
        m_properties->setCropSize(rect.size());
    });
    connect(m_view, &EditorView::cropConfirmed, this, [this] { finishCrop(true); });
    connect(m_properties, &PropertiesPanel::cropApplyRequested, this, [this] { finishCrop(true); });
    connect(m_properties, &PropertiesPanel::cropResetRequested, this, [this] {
        m_view->setCropDraft(m_view->editorScene()->baseImage().rect());
    });
    connect(scene, &EditorScene::cropChanged, this, &MainWindow::updateImageInfo);
    connect(m_properties, &PropertiesPanel::objectClicked, this, &MainWindow::selectObject);
    connect(m_properties, &PropertiesPanel::objectMoved, this, [this](int from, int to) {
        m_undoStack->push(new ReorderItemsCommand(m_view->editorScene(), from, to));
    });

    // Dim background (F-27): the switch exists twice (rail and panel). Both
    // push an undo step; the scene's state is shown in both.
    connect(m_properties, &PropertiesPanel::dimSettingsChanged, this, [this](const DimSettings &settings) {
        if (m_view->hasImage() && settings != m_view->editorScene()->dimSettings())
            m_undoStack->push(new SetDimCommand(m_view->editorScene(), settings));
        else
            m_properties->setDimSettings(m_view->editorScene()->dimSettings());
    });
    // The switch always starts at 45 % without blur (DimSettings defaults);
    // the blur buttons below it pick none, light or strong.
    connect(m_toolRail, &ToolRail::dimToggled, this, [this](bool enabled) {
        if (!m_view->hasImage()) {
            m_toolRail->showDim(m_view->editorScene()->dimSettings());
            return;
        }
        DimSettings settings;
        settings.enabled = enabled;
        if (!enabled) {
            settings = m_view->editorScene()->dimSettings();
            settings.enabled = false;
        }
        m_undoStack->push(new SetDimCommand(m_view->editorScene(), settings));
    });
    connect(m_toolRail, &ToolRail::blurChosen, this, [this](int blur) {
        DimSettings settings = m_view->editorScene()->dimSettings();
        if (settings.blur == blur)
            return;
        settings.blur = blur;
        // Computing a new blur takes a moment; show that something happens.
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_undoStack->push(new SetDimCommand(m_view->editorScene(), settings));
        m_view->editorScene()->dimLayer(); // compute now, while the cursor waits
        QApplication::restoreOverrideCursor();
    });
    connect(m_view->editorScene(), &EditorScene::dimSettingsChanged, this, [this] {
        const DimSettings settings = m_view->editorScene()->dimSettings();
        m_properties->setDimSettings(settings);
        m_toolRail->showDim(settings);
    });
    m_properties->setAdvancedMode(Settings::advancedMode());

    connect(m_capture.get(), &CaptureBackend::captured, this, &MainWindow::onCaptured);
    connect(m_capture.get(), &CaptureBackend::cancelled, this, [this] {
        onCaptureEnded(!m_hiddenBeforeCapture);
        statusBar()->showMessage(tr("Aufnahme abgebrochen."), 3000);
    });
    connect(m_capture.get(), &CaptureBackend::failed, this, [this](const QString &message) {
        onCaptureEnded();
        QMessageBox::warning(this, tr("Aufnahme fehlgeschlagen"), message);
    });

    // Feedback on Kopieren / Speichern lasts a moment (showDone).
    m_copyFeedback = new QTimer(this);
    m_saveFeedback = new QTimer(this);
    for (QTimer *timer : {m_copyFeedback, m_saveFeedback}) {
        timer->setSingleShot(true);
        timer->setInterval(1600);
    }
    connect(m_copyFeedback, &QTimer::timeout, this, [this] { restoreButton(m_copyButton); });
    connect(m_saveFeedback, &QTimer::timeout, this, [this] { restoreButton(m_saveButton); });

    createTrayIcon();
    createShortcuts();
    onToolChanged(m_toolRail->currentTool());

    // Global hotkeys (F-42): Windows registers them in the app, GNOME's
    // extension gets the keys and preferences.
    connect(m_capture.get(), &CaptureBackend::hotkeyPressed, this, &MainWindow::onHotkey);
    pushHotkeyOptions();
    QTimer::singleShot(1500, this, &MainWindow::pointOutHotkeyProblem);
    updateActions();
    updateZoomLabel();
}

MainWindow::~MainWindow()
{
    // The scene emits selection signals while it deletes its items; the
    // panels may already be gone by then.
    m_view->editorScene()->disconnect(this);
    m_undoStack->disconnect(this);
}

QWidget *MainWindow::createTopBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("TopBar"));
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(52);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    // Logo and name.
    m_logo = new QLabel(bar);
    auto *name = new QLabel(QStringLiteral("NSL Snap"), bar);
    name->setObjectName(QStringLiteral("AppName"));
    layout->addWidget(m_logo);
    layout->addWidget(name);
    layout->addSpacing(8);

    // Capture modes (F-01–F-03). The highlighted one is the last used mode.
    auto *modes = new QFrame(bar);
    modes->setProperty("role", QStringLiteral("segment"));
    auto *modesLayout = new QHBoxLayout(modes);
    modesLayout->setContentsMargins(3, 3, 3, 3);
    modesLayout->setSpacing(2);
    m_windowButton = segmentButton(tr("Fenster"), modes);
    m_regionButton = segmentButton(tr("Bereich"), modes);
    m_screenButton = segmentButton(tr("Bildschirm"), modes);
    m_windowButton->setToolTip(tr("Fenster aufnehmen (Ctrl+3)"));
    m_regionButton->setToolTip(m_capture->supportsRegion() ? tr("Bereich aufnehmen (Ctrl+5)")
                                                           : tr("Die Bereichsaufnahme folgt auf dieser Plattform"));
    m_regionButton->setEnabled(m_capture->supportsRegion());
    m_windowButton->setChecked(true);
    m_screenButton->setToolTip(m_capture->supportsRegion() ? tr("Bildschirm aufnehmen (Ctrl+1)")
                                                           : tr("Die Bildschirmaufnahme folgt auf dieser Plattform"));
    m_screenButton->setEnabled(m_capture->supportsRegion());
    modesLayout->addWidget(m_windowButton);
    modesLayout->addWidget(m_regionButton);
    modesLayout->addWidget(m_screenButton);
    layout->addWidget(modes);
    connect(m_windowButton, &QToolButton::clicked, this, &MainWindow::captureWindow);
    connect(m_regionButton, &QToolButton::clicked, this, &MainWindow::captureRegion);
    connect(m_screenButton, &QToolButton::clicked, this, &MainWindow::captureScreen);

    // Timer (F-04). The countdown itself comes with the overlay.
    m_timerIcon = new QLabel(bar);
    auto *timerLabel = new QLabel(tr("Timer"), bar);
    timerLabel->setProperty("role", QStringLiteral("muted"));
    m_timer = new QComboBox(bar);
    // Delayed capture (F-04): the seconds as item data; remembered.
    m_timer->addItem(tr("Aus"), 0);
    for (int seconds : {3, 5, 10})
        m_timer->addItem(tr("%1 s").arg(seconds), seconds);
    m_timer->setCurrentIndex(std::max(0, m_timer->findData(Settings::captureDelay())));
    m_timer->setAccessibleName(tr("Timer"));
    m_timer->setToolTip(tr("Verzögerte Aufnahme: Countdown, dann einfrieren und auswählen. Esc bricht ab."));
    m_timer->setFocusPolicy(Qt::NoFocus);
    connect(m_timer, &QComboBox::currentIndexChanged, this,
            [this] {
                Settings::setCaptureDelay(m_timer->currentData().toInt());
                pushHotkeyOptions(); // hotkey captures use the timer too (F-42)
            });
    layout->addWidget(m_timerIcon);
    layout->addWidget(timerLabel);
    layout->addWidget(m_timer);

    // Undo / redo (F-11).
    m_undoButton = iconButton(tr("Rückgängig (Ctrl+Z)"), bar);
    m_redoButton = iconButton(tr("Wiederholen (Ctrl+Y)"), bar);
    // A text being typed becomes its own step first (F-25).
    connect(m_undoButton, &QToolButton::clicked, this, [this] { m_view->finishTextEditing(); m_undoStack->undo(); });
    connect(m_redoButton, &QToolButton::clicked, this, [this] { m_view->finishTextEditing(); m_undoStack->redo(); });
    connect(m_undoStack, &QUndoStack::canUndoChanged, m_undoButton, &QWidget::setEnabled);
    connect(m_undoStack, &QUndoStack::canRedoChanged, m_redoButton, &QWidget::setEnabled);
    m_undoButton->setEnabled(false);
    m_redoButton->setEnabled(false);
    layout->addWidget(m_undoButton);
    layout->addWidget(m_redoButton);
    // Clear (F-10): remove the image and start over.
    m_clearButton = iconButton(tr("Editor leeren (Ctrl+Entf)"), bar);
    connect(m_clearButton, &QToolButton::clicked, this, &MainWindow::clearEditor);
    layout->addWidget(m_clearButton);

    layout->addStretch(1);

    // Zoom (D-017): minus, the current zoom with a menu, plus.
    auto *zoom = new QFrame(bar);
    zoom->setProperty("role", QStringLiteral("segment"));
    auto *zoomLayout = new QHBoxLayout(zoom);
    zoomLayout->setContentsMargins(3, 3, 3, 3);
    zoomLayout->setSpacing(0);
    m_zoomOutButton = iconButton(tr("Verkleinern (Ctrl+Minus)"), zoom);
    m_zoomInButton = iconButton(tr("Vergrössern (Ctrl+Plus)"), zoom);
    m_zoomButton = new QToolButton(zoom);
    m_zoomButton->setToolTip(tr("Zoomstufe"));
    m_zoomButton->setPopupMode(QToolButton::InstantPopup);
    m_zoomButton->setFocusPolicy(Qt::NoFocus);
    m_zoomButton->setMinimumWidth(78); // room for "Einpassen"
    auto *zoomMenu = new QMenu(m_zoomButton);
    m_fitAction = zoomMenu->addAction(tr("Einpassen"), QKeySequence(Qt::CTRL | Qt::Key_0), m_view, &EditorView::fitToWindow);
    m_fitAction->setCheckable(true);
    zoomMenu->addSeparator();
    for (int percent : {50, 100, 200, 400}) {
        QAction *action = zoomMenu->addAction(QStringLiteral("%1 %").arg(percent), this,
                                              [this, percent] { m_view->setZoom(percent / 100.0); });
        if (percent == 100)
            action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_0));
    }
    m_zoomButton->setMenu(zoomMenu);
    connect(m_zoomOutButton, &QToolButton::clicked, m_view, &EditorView::zoomOut);
    connect(m_zoomInButton, &QToolButton::clicked, m_view, &EditorView::zoomIn);
    zoomLayout->addWidget(m_zoomOutButton);
    zoomLayout->addWidget(m_zoomButton);
    zoomLayout->addWidget(m_zoomInButton);
    // Back to "fills the area" in one click; shown pressed while active.
    m_fitButton = iconButton(tr("Einpassen: Bild füllt die Fläche (Ctrl+0)"), zoom);
    m_fitButton->setCheckable(true);
    connect(m_fitButton, &QToolButton::clicked, this, [this] {
        m_view->fitToWindow();
        updateZoomLabel(); // keeps the button pressed even if the zoom did not change
    });
    zoomLayout->addWidget(m_fitButton);
    layout->addWidget(zoom);

    m_settingsButton = iconButton(tr("Einstellungen (F2)"), bar);
    connect(m_settingsButton, &QToolButton::clicked, this, &MainWindow::showSettings);
    layout->addWidget(m_settingsButton);

    m_copyButton = textButton(tr("Kopieren"), QStringLiteral("secondary"), bar);
    m_copyButton->setToolTip(tr("Bild in die Zwischenablage kopieren (Ctrl+C)"));
    m_saveButton = textButton(tr("Speichern"), QStringLiteral("primary"), bar);
    m_saveButton->setToolTip(tr("Speichern unter: Ordner und Name wählen (Ctrl+Shift+S)\n"
                                "Ctrl+S speichert direkt im eingestellten Ordner"));
    connect(m_copyButton, &QPushButton::clicked, this, &MainWindow::copyImage);
    // The button asks where to save; Ctrl+S saves directly, like WinSnap.
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveImageAs);
    layout->addWidget(m_copyButton);
    layout->addWidget(m_saveButton);

    return bar;
}

void MainWindow::createShortcuts()
{
    // App shortcuts after WinSnap (docs/04-features.md, F-46).
    const auto add = [this](const QKeySequence &key, auto slot) {
        auto *action = new QAction(this);
        action->setShortcut(key);
        connect(action, &QAction::triggered, this, slot);
        addAction(action);
        return action;
    };
    add(QKeySequence(Qt::CTRL | Qt::Key_N), &MainWindow::captureLastMode);
    add(QKeySequence(Qt::CTRL | Qt::Key_1), &MainWindow::captureScreen);
    add(QKeySequence(Qt::CTRL | Qt::Key_3), &MainWindow::captureWindow);
    add(QKeySequence(Qt::CTRL | Qt::Key_5), &MainWindow::captureRegion);
    add(QKeySequence::Open, &MainWindow::openImage);
    add(QKeySequence::Paste, &MainWindow::pasteImage);
    add(QKeySequence::Save, &MainWindow::saveImage);
    add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), &MainWindow::saveImageAs);
    add(QKeySequence(Qt::CTRL | Qt::Key_E), &MainWindow::showSavedFile);
    add(QKeySequence::Copy, &MainWindow::copyImage);
    add(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C), &MainWindow::copyFilePath);
    add(QKeySequence(Qt::CTRL | Qt::Key_Delete), &MainWindow::clearEditor);
    add(QKeySequence(Qt::Key_F2), &MainWindow::showSettings);

    // Zoom (D-017). Ctrl+0 (fit) and Ctrl+Alt+0 (100 %) sit in the zoom menu.
    auto *zoomIn = new QAction(this);
    // Ctrl + "=" too: on many layouts "+" needs Shift.
    zoomIn->setShortcuts({QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    connect(zoomIn, &QAction::triggered, m_view, &EditorView::zoomIn);
    addAction(zoomIn);
    auto *zoomOut = new QAction(this);
    zoomOut->setShortcuts(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, m_view, &EditorView::zoomOut);
    addAction(zoomOut);
    // Menu shortcuts only work while the menu is open; make them global.
    addActions(m_zoomButton->menu()->actions());
    // Delete removes the selected annotations, not the image (F-46).
    auto *remove = new QAction(this);
    remove->setShortcuts({QKeySequence::Delete, QKeySequence(Qt::Key_Backspace)});
    connect(remove, &QAction::triggered, m_view, &EditorView::deleteSelection);
    addAction(remove);

    add(QKeySequence::Undo, [this] { m_undoStack->undo(); });
    auto *redo = new QAction(this);
    redo->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(redo, &QAction::triggered, m_undoStack, &QUndoStack::redo);
    addAction(redo);
}

void MainWindow::createTrayIcon()
{
    // F-45. On GNOME only with the AppIndicator extension (Zorin has it);
    // without a tray the window works as before and closing quits.
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/logo.svg")), this);
    m_tray->setToolTip(tr("NSL Snap"));
    auto *menu = new QMenu(this);
    menu->addAction(tr("Fenster aufnehmen"), this, &MainWindow::captureWindow);
    menu->addAction(tr("Bereich aufnehmen"), this, &MainWindow::captureRegion);
    menu->addAction(tr("Bildschirm aufnehmen"), this, &MainWindow::captureScreen);
    menu->addSeparator();
    menu->addAction(tr("Editor öffnen"), this, &MainWindow::showEditor);
    menu->addAction(tr("Einstellungen"), this, [this] {
        showEditor();
        showSettings();
    });
    menu->addSeparator();
    menu->addAction(tr("Beenden"), this, &MainWindow::quit);
    m_tray->setContextMenu(menu);
    // A click on the icon opens the editor (where the tray supports it).
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showEditor();
    });
    m_tray->show();
    // The app keeps running while only the tray icon is left.
    QApplication::setQuitOnLastWindowClosed(false);
}

void MainWindow::showEditor()
{
    showNormal();
    raise();
    activateWindow();
}

bool MainWindow::hideToTray()
{
    if (!m_tray || !Settings::closeToTray())
        return false;
    hide();
    // Once, so nobody wonders where the app went.
    if (!Settings::trayHintShown()) {
        m_tray->showMessage(tr("NSL Snap läuft weiter"),
                            tr("Aufnehmen und Öffnen über das Symbol im Infobereich. "
                               "Beenden ebenfalls dort; das Verhalten lässt sich in den Einstellungen ändern."));
        Settings::setTrayHintShown(true);
    }
    return true;
}

void MainWindow::quit()
{
    // Unsaved work is not lost without a question.
    if (hasUnsavedChanges()) {
        showEditor();
        if (!confirmDiscard(tr("NSL Snap beenden"), tr("Trotzdem beenden?")))
            return;
    }
    m_quitting = true;
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Closing the window keeps the app in the tray (setting, F-45).
    if (!m_quitting && hideToTray()) {
        event->ignore();
        return;
    }
    if (!m_quitting && hasUnsavedChanges() && !confirmDiscard(tr("NSL Snap beenden"), tr("Trotzdem beenden?"))) {
        event->ignore();
        return;
    }
    event->accept();
    QApplication::quit(); // also when the tray keeps the app alive
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Escape: leave the current tool first, then the selection, then hide
    // to the tray (F-46, F-45).
    // In the crop tool Enter confirms and Escape discards the draft, also
    // when the focus is not in the editor.
    if (m_toolRail->currentTool() == ToolId::Crop
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Escape)) {
        finishCrop(event->key() != Qt::Key_Escape);
        return;
    }
    if (event->key() == Qt::Key_Escape && m_toolRail->currentTool() != ToolId::Select) {
        m_toolRail->setCurrentTool(ToolId::Select);
        return;
    }
    if (event->key() == Qt::Key_Escape && !m_view->editorScene()->selectedItems().isEmpty()) {
        m_view->editorScene()->clearSelection();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_tray && hideToTray())
        return;
    QMainWindow::keyPressEvent(event);
}

void MainWindow::applyTheme(const Theme &theme)
{
    QIcon logo(QStringLiteral(":/icons/logo.svg"));
    m_logo->setPixmap(logo.pixmap(QSize(26, 26)));

    // Segment buttons: muted when off, full text colour when on.
    m_windowButton->setIcon(Icons::toggleIcon(QStringLiteral("window"), theme.muted, theme.text));
    m_regionButton->setIcon(Icons::toggleIcon(QStringLiteral("region"), theme.muted, theme.text));
    m_screenButton->setIcon(Icons::toggleIcon(QStringLiteral("screen"), theme.muted, theme.text));
    m_timerIcon->setPixmap(Icons::icon(QStringLiteral("timer"), theme.muted).pixmap(QSize(16, 16)));
    m_undoButton->setIcon(Icons::icon(QStringLiteral("undo"), theme.muted));
    m_redoButton->setIcon(Icons::icon(QStringLiteral("redo"), theme.muted));
    m_settingsButton->setIcon(Icons::icon(QStringLiteral("settings"), theme.muted));
    m_clearButton->setIcon(Icons::icon(QStringLiteral("clear"), theme.muted));
    m_successColor = theme.success;
    m_zoomOutButton->setIcon(Icons::icon(QStringLiteral("zoom-out"), theme.muted));
    m_zoomInButton->setIcon(Icons::icon(QStringLiteral("zoom-in"), theme.muted));
    m_fitButton->setIcon(Icons::toggleIcon(QStringLiteral("fit"), theme.muted, theme.softText));
    m_copyButton->setIcon(Icons::icon(QStringLiteral("copy"), theme.text));
    m_saveButton->setIcon(Icons::icon(QStringLiteral("save"), QColor(Qt::white)));

    m_toolRail->applyTheme(theme);
    m_properties->applyTheme(theme);
}

void MainWindow::finishCrop(bool apply)
{
    if (apply)
        m_view->applyCrop();
    else
        m_view->cancelCrop();
    m_toolRail->setCurrentTool(ToolId::Select);
}

void MainWindow::loadDefaultStyles()
{
    // Styles for new objects as the user left them (F-22).
    EditorScene *scene = m_view->editorScene();
    scene->setFrameStyle(Settings::frameStyle());
    scene->setTextStyle(Settings::textStyle());
    scene->setArrowStyle(Settings::arrowStyle());
    scene->setMarkerStyle(Settings::markerStyle());
    scene->setRedactStyle(Settings::redactStyle());
    scene->setReplaceText(Settings::replaceText());
}

void MainWindow::onToolChanged(ToolId id)
{
    m_view->setTool(id);
    m_toolHint->setText(toolInfo(id).hint);
    onSelectionChanged();
}

namespace {

// The property group for new objects of a tool.
PanelSection sectionFor(ToolId tool)
{
    switch (tool) {
    case ToolId::Pixelate: return PanelSection::Redact;
    case ToolId::ReplaceText: return PanelSection::Replace;
    case ToolId::Crop: return PanelSection::Crop;
    case ToolId::Text: return PanelSection::Text;
    case ToolId::Arrow: return PanelSection::Arrow;
    case ToolId::Marker: return PanelSection::Marker;
    default: return PanelSection::Frame;
    }
}

} // namespace

void MainWindow::onSelectionChanged()
{
    const EditorScene *scene = m_view->editorScene();
    const QList<FrameItem *> selected = scene->selectedFrames();
    const QList<RedactItem *> redactions = selectedRedactions();
    ReplaceItem *replace = selectedReplace();
    const QList<TextItem *> texts = selectedTexts();
    const QList<ArrowItem *> arrows = selectedArrows();
    const QList<MarkerItem *> markers = selectedMarkers();

    // The panel shows the style of the first selected object of each kind.
    if (!redactions.isEmpty())
        m_properties->setRedactStyle(redactions.first()->style());
    if (replace)
        m_properties->setReplaceStyle(replace->style());
    if (!texts.isEmpty())
        m_properties->setTextStyle(texts.first()->style());
    if (!arrows.isEmpty())
        m_properties->setArrowStyle(arrows.first()->style());
    if (!markers.isEmpty())
        m_properties->setMarkerStyle(markers.first()->style());

    // Frames win in a mixed selection; otherwise the first kind selected.
    if (!selected.isEmpty()) {
        if (redactions.isEmpty())
            m_properties->showSections({PanelSection::Frame});
        else
            m_properties->showSections({PanelSection::Frame, PanelSection::Redact});
        // Numbered or not; the "Nummer" field tells.
        m_properties->showSelection(tr("Rahmen"), int(selected.size()));
        // Show where the number really is, and offer only corners where it
        // fits for every selected frame.
        FrameStyle style = selected.first()->style();
        style.badge = selected.first()->effectiveBadge();
        m_properties->setFrameStyle(style);
        QList<BadgePosition> fitting = selected.first()->fittingBadgePositions();
        for (const FrameItem *frame : selected)
            fitting.removeIf([frame](BadgePosition p) { return !frame->fittingBadgePositions().contains(p); });
        if (fitting.isEmpty() && style.badge != BadgePosition::None)
            fitting << style.badge; // nothing fits: keep the current one selectable
        m_properties->setBadgeOptions(fitting);
    } else if (!redactions.isEmpty()) {
        m_properties->showSelection(tr("Verpixeln"), int(redactions.size()));
        m_properties->showSections({PanelSection::Redact});
    } else if (replace) {
        m_properties->showSelection(tr("Text ersetzen"), 1);
        m_properties->showSections({PanelSection::Replace});
    } else if (!texts.isEmpty()) {
        m_properties->showSelection(tr("Text"), int(texts.size()));
        m_properties->showSections({PanelSection::Text});
    } else if (!arrows.isEmpty()) {
        m_properties->showSelection(tr("Pfeil"), int(arrows.size()));
        m_properties->showSections({PanelSection::Arrow});
    } else if (!markers.isEmpty()) {
        m_properties->showSelection(tr("Marker"), int(markers.size()));
        m_properties->showSections({PanelSection::Marker});
    } else {
        // Style for new objects; only the number tool draws with a number.
        const ToolId tool = m_toolRail->currentTool();
        FrameStyle style = scene->frameStyle();
        if (tool != ToolId::Number)
            style.badge = BadgePosition::None;
        m_properties->showTool(tool);
        m_properties->setFrameStyle(style);
        m_properties->setRedactStyle(scene->redactStyle());
        ReplaceStyle replaceDefaults;
        replaceDefaults.text = scene->replaceText();
        m_properties->setReplaceStyle(replaceDefaults);
        m_properties->setTextStyle(scene->textStyle());
        m_properties->setArrowStyle(scene->arrowStyle());
        m_properties->setMarkerStyle(scene->markerStyle());
        m_properties->showSections({sectionFor(tool)});
        m_properties->setBadgeOptions({BadgePosition::TopRight, BadgePosition::TopLeft,
                                       BadgePosition::BottomRight, BadgePosition::BottomLeft});
    }
    updateObjectList();
}

void MainWindow::onFrameStyleChanged(const FrameStyle &style)
{
    // The panel style becomes the default for new frames (F-22) and is
    // applied to the selected frames as one undo step.
    EditorScene *scene = m_view->editorScene();
    // "Ohne Nummer" does not change where the number tool puts its numbers.
    FrameStyle defaults = style;
    if (defaults.badge == BadgePosition::None)
        defaults.badge = scene->frameStyle().badge;
    scene->setFrameStyle(defaults);
    Settings::setFrameStyle(defaults);
    const QList<FrameItem *> selected = scene->selectedFrames();
    if (!selected.isEmpty())
        m_undoStack->push(new SetFrameStyleCommand(selected, style));
}

QList<RedactItem *> MainWindow::selectedRedactions() const
{
    QList<RedactItem *> result;
    for (RectItem *item : m_view->editorScene()->selectedAnnotations()) {
        if (item->type() == RedactItem::Type)
            result << static_cast<RedactItem *>(item);
    }
    return result;
}

ReplaceItem *MainWindow::selectedReplace() const
{
    // Replaced texts are edited one at a time: the text and its position
    // belong to one place in the image.
    const QList<RectItem *> selected = m_view->editorScene()->selectedAnnotations();
    if (selected.size() == 1 && selected.first()->type() == ReplaceItem::Type)
        return static_cast<ReplaceItem *>(selected.first());
    return nullptr;
}

QList<ArrowItem *> MainWindow::selectedArrows() const
{
    QList<ArrowItem *> result;
    for (RectItem *item : m_view->editorScene()->selectedAnnotations()) {
        if (item->type() == ArrowItem::Type)
            result << static_cast<ArrowItem *>(item);
    }
    return result;
}

QList<MarkerItem *> MainWindow::selectedMarkers() const
{
    QList<MarkerItem *> result;
    for (RectItem *item : m_view->editorScene()->selectedAnnotations()) {
        if (item->type() == MarkerItem::Type)
            result << static_cast<MarkerItem *>(item);
    }
    return result;
}

void MainWindow::onArrowStyleChanged(const ArrowStyle &style)
{
    // Default for new arrows, and applied to the selected ones (F-44).
    m_view->editorScene()->setArrowStyle(style);
    Settings::setArrowStyle(style);
    QList<ArrowItem *> arrows = selectedArrows();
    arrows.removeIf([&style](const ArrowItem *item) { return item->style() == style; });
    if (!arrows.isEmpty())
        m_undoStack->push(new SetArrowStyleCommand(arrows, style));
}

void MainWindow::onMarkerStyleChanged(const MarkerStyle &style)
{
    // Default for new markers, and applied to the selected ones (F-28).
    m_view->editorScene()->setMarkerStyle(style);
    Settings::setMarkerStyle(style);
    QList<MarkerItem *> markers = selectedMarkers();
    markers.removeIf([&style](const MarkerItem *item) { return item->style() == style; });
    if (!markers.isEmpty())
        m_undoStack->push(new SetMarkerStyleCommand(markers, style));
}

QList<TextItem *> MainWindow::selectedTexts() const
{
    QList<TextItem *> result;
    for (RectItem *item : m_view->editorScene()->selectedAnnotations()) {
        if (item->type() == TextItem::Type)
            result << static_cast<TextItem *>(item);
    }
    return result;
}

void MainWindow::onTextStyleChanged(const TextStyle &style)
{
    // Default for new texts, and applied to the selected ones (F-25).
    EditorScene *scene = m_view->editorScene();
    scene->setTextStyle(style);
    Settings::setTextStyle(style);
    QList<TextItem *> texts = selectedTexts();
    texts.removeIf([&style](const TextItem *item) { return item->style().sameLook(style); });
    if (!texts.isEmpty())
        m_undoStack->push(new SetTextStyleCommand(texts, style));
}

void MainWindow::onReplaceStyleChanged(const ReplaceStyle &style)
{
    m_view->editorScene()->setReplaceText(style.text); // default for the next one
    Settings::setReplaceText(style.text);
    if (ReplaceItem *item = selectedReplace())
        m_undoStack->push(new SetReplaceStyleCommand(item, style));
}

void MainWindow::onRedactStyleChanged(const RedactStyle &style)
{
    // Default for new areas, and applied to the selected ones (F-26).
    m_view->editorScene()->setRedactStyle(style);
    Settings::setRedactStyle(style);
    const QList<RedactItem *> selected = selectedRedactions();
    if (!selected.isEmpty())
        m_undoStack->push(new SetRedactStyleCommand(selected, style));
}

void MainWindow::updateObjectList()
{
    QStringList labels;
    QList<bool> selected;
    for (const RectItem *item : m_view->editorScene()->annotations()) {
        labels << item->label();
        selected << item->isSelected();
    }
    m_properties->setObjects(labels, selected);
}

void MainWindow::selectObject(int index)
{
    const QList<RectItem *> items = m_view->editorScene()->annotations();
    if (index < 0 || index >= items.size())
        return;
    m_view->editorScene()->clearSelection();
    items[index]->setSelected(true);
    m_view->ensureVisible(items[index]);
}

void MainWindow::updateActions()
{
    const bool hasImage = m_view->hasImage();
    m_saveButton->setEnabled(hasImage);
    m_copyButton->setEnabled(hasImage);
    m_clearButton->setEnabled(hasImage);
    if (!hasImage)
        m_imageInfo->setText(tr("Kein Bild"));
}

void MainWindow::updateZoomLabel()
{
    const bool hasImage = m_view->hasImage();
    m_zoomButton->setText(hasImage ? QStringLiteral("%1 %").arg(qRound(m_view->zoom() * 100)) : QStringLiteral("–"));
    m_fitAction->setChecked(hasImage && m_view->isFitMode());
    m_fitButton->setChecked(hasImage && m_view->isFitMode());
    m_fitButton->setEnabled(hasImage);
    for (QWidget *w : {static_cast<QWidget *>(m_zoomOutButton), static_cast<QWidget *>(m_zoomButton),
                       static_cast<QWidget *>(m_zoomInButton)})
        w->setEnabled(hasImage);
    m_zoomOutButton->setEnabled(hasImage && m_view->zoom() > EditorView::kMinZoom + 0.001);
    m_zoomInButton->setEnabled(hasImage && m_view->zoom() < EditorView::kMaxZoom - 0.001);
}

void MainWindow::captureWindow()
{
    startCapture(CaptureResult::Kind::Window);
}

void MainWindow::captureRegion()
{
    startCapture(CaptureResult::Kind::Region);
}

void MainWindow::captureScreen()
{
    startCapture(CaptureResult::Kind::Screen);
}

void MainWindow::captureLastMode()
{
    startCapture(m_lastMode);
}

QToolButton *MainWindow::modeButton(CaptureResult::Kind kind) const
{
    switch (kind) {
    case CaptureResult::Kind::Region: return m_regionButton;
    case CaptureResult::Kind::Screen: return m_screenButton;
    case CaptureResult::Kind::Window: break;
    }
    return m_windowButton;
}

void MainWindow::startCapture(CaptureResult::Kind kind, bool direct)
{
    if (kind != CaptureResult::Kind::Window && !m_capture->supportsRegion()) {
        statusBar()->showMessage(tr("Diese Aufnahmeart folgt auf dieser Plattform."), 4000);
        return;
    }
    if (!m_capture->isAvailable()) {
        QMessageBox::warning(this, tr("Aufnahme nicht möglich"), m_capture->unavailableReason());
        modeButton(m_lastMode)->setChecked(true);
        return;
    }
    modeButton(kind)->setChecked(true);
    // The overlay can switch to region mode, so it always gets the setting.
    m_capture->setRegionByDragging(Settings::regionByDragging());
    m_capture->setDelay(m_timer->currentData().toInt());
    for (QToolButton *button : {m_windowButton, m_regionButton, m_screenButton})
        button->setEnabled(false);
    // The app hides first, so it is not in the frozen screen (D-011).
    // Started from the tray, a cancelled capture leaves it there.
    m_hiddenBeforeCapture = !isVisible();
    hide();
    // `direct` (hotkeys): the active window or the monitor under the
    // pointer, without the overlay.
    void (CaptureBackend::*capture)() = direct ? &CaptureBackend::captureActiveWindow : &CaptureBackend::captureWindow;
    if (kind == CaptureResult::Kind::Region)
        capture = &CaptureBackend::captureRegion;
    else if (kind == CaptureResult::Kind::Screen)
        capture = direct ? &CaptureBackend::captureScreenAtPointer : &CaptureBackend::captureScreen;
    QTimer::singleShot(kHideDelayMs, m_capture.get(), capture);
}

void MainWindow::pushHotkeyOptions()
{
    HotkeyOptions options;
    options.hotkeys = Settings::globalHotkeys();
    options.delay = m_timer->currentData().toInt();
    options.regionByDragging = Settings::regionByDragging();
    options.lastMode = m_lastMode;
    m_capture->setHotkeyOptions(options);
}

void MainWindow::onHotkey(HotkeyAction action)
{
    // Windows (F-42); a capture already running ignores the key.
    if (!m_windowButton->isEnabled())
        return;
    switch (action) {
    case HotkeyAction::Last: startCapture(m_lastMode); break;
    case HotkeyAction::Region: startCapture(CaptureResult::Kind::Region); break;
    case HotkeyAction::Screen: startCapture(CaptureResult::Kind::Screen, true); break;
    case HotkeyAction::Window: startCapture(CaptureResult::Kind::Window, true); break;
    }
}

void MainWindow::pointOutHotkeyProblem()
{
    // Once (Q-15): e.g. Windows 11 keeps Print for its Snipping Tool.
    if (Settings::hotkeyNoticeShown() || !Settings::globalHotkeys().enabled)
        return;
    const QString problem = m_capture->hotkeyProblem();
    if (problem.isEmpty())
        return;
    Settings::setHotkeyNoticeShown(true);
    QMessageBox box(QMessageBox::Information, tr("Globale Tastenkürzel"), problem, QMessageBox::Ok, this);
    QPushButton *settings = box.addButton(tr("Einstellungen öffnen"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == settings)
        showSettings();
}

#ifdef NSL_HAVE_GNOME_BACKEND
void MainWindow::connectAppService(AppService *service)
{
    // The extension has a hotkey capture ready (F-42, D-031).
    connect(service, &AppService::captureReady, this, [this] { m_capture->takeHotkeyCapture(); });
    // A second start of the app shows this window.
    connect(service, &AppService::activateRequested, this, &MainWindow::showEditor);
    // Before a hotkey capture the window goes away, like for captures
    // started here (D-011), and comes back after it.
    service->hideForCapture = [this] {
        m_hiddenForHotkey = isVisible();
        if (m_hiddenForHotkey)
            hide();
        return m_hiddenForHotkey;
    };
    connect(service, &AppService::captureCancelled, this, [this] {
        if (m_hiddenForHotkey)
            showEditor();
        m_hiddenForHotkey = false;
    });
}
#endif

void MainWindow::onCaptured(const CaptureResult &result)
{
    m_hiddenForHotkey = false;
    onCaptureEnded();
    // The user may have switched the mode in the overlay; that one counts.
    m_lastMode = result.kind;
    modeButton(m_lastMode)->setChecked(true);
    pushHotkeyOptions(); // Print uses the last mode
    if (result.kind == CaptureResult::Kind::Region) {
        // A region is used as it is: no window edges to clean (F-03).
        setImage(result.image, result.scale, tr("Bereich"), tr("Bereich"));
        return;
    }
    if (result.kind == CaptureResult::Kind::Screen) {
        setImage(result.image, result.scale, tr("Bildschirm"), tr("Bildschirm"));
        return;
    }
    const QImage image = WindowImage::prepare(result);
    setImage(image, result.scale, result.title, tr("Fenster «%1»").arg(result.title));
}

void MainWindow::onCaptureEnded(bool showWindow)
{
    m_windowButton->setEnabled(true);
    m_regionButton->setEnabled(m_capture->supportsRegion());
    m_screenButton->setEnabled(m_capture->supportsRegion());
    modeButton(m_lastMode)->setChecked(true); // after a cancel: the last real mode
    if (showWindow)
        showEditor();
}

void MainWindow::openImage()
{
    QStringList filters;
    filters << tr("Bilder (*.png *.jpg *.jpeg *.webp)") << tr("Alle Dateien (*)");
    const QString path = QFileDialog::getOpenFileName(this, tr("Bild öffnen"), QString(),
                                                      filters.join(QStringLiteral(";;")));
    if (!path.isEmpty())
        loadImageFile(path);
}

void MainWindow::pasteImage()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (mime && mime->hasImage()) {
        setImage(qvariant_cast<QImage>(mime->imageData()), 1.0, tr("Zwischenablage"), tr("Aus der Zwischenablage"));
        return;
    }
    if (mime && mime->hasUrls() && mime->urls().first().isLocalFile()) {
        loadImageFile(mime->urls().first().toLocalFile());
        return;
    }
    statusBar()->showMessage(tr("Die Zwischenablage enthält kein Bild."), 3000);
}

void MainWindow::loadImageFile(const QString &path)
{
    QImageReader reader(path);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Bild öffnen"),
                             tr("«%1» konnte nicht geöffnet werden: %2")
                                 .arg(QFileInfo(path).fileName(), reader.errorString()));
        return;
    }
    const QString name = QFileInfo(path).completeBaseName();
    setImage(image, EditorScene::scaleFromMetadata(image), name, QFileInfo(path).fileName());
    m_handedOff = true; // the file is already on disk
}

void MainWindow::setImage(const QImage &image, double scale, const QString &name, const QString &source)
{
    // A new image starts a new document: older undo steps no longer apply.
    m_undoStack->clear();
    m_view->setImage(image, scale);
    m_imageName = name;
    m_imageSource = source;
    m_savedPath.clear();
    m_handedOff = false;
    updateImageInfo();
    updateActions();
    updateZoomLabel();
}

void MainWindow::updateImageInfo()
{
    const EditorScene *scene = m_view->editorScene();
    if (!scene->hasImage())
        return; // updateActions shows "Kein Bild"
    // The size of the result, i.e. of the crop (F-12).
    const QSize size = scene->cropRect().size();
    QString text = tr("%1 · %2 × %3 px · %4×")
                       .arg(m_imageSource).arg(size.width()).arg(size.height())
                       .arg(QLocale().toString(scene->imageScale(), 'g', 3));
    if (scene->isCropped())
        text += tr(" · zugeschnitten");
    m_imageInfo->setText(text);
}

void MainWindow::saveImage()
{
    // Ctrl+S saves without asking, like WinSnap (F-41).
    if (!m_view->hasImage())
        return;
    const QString folder = Settings::saveFolder();
    QDir().mkpath(folder);
    writeImage(QDir(folder).filePath(suggestedFileName()));
}

void MainWindow::saveImageAs()
{
    if (!m_view->hasImage())
        return;
    // Starts in the folder of the last save, else in the one from the settings.
    const QString folder = m_lastSaveFolder.isEmpty() ? Settings::saveFolder() : m_lastSaveFolder;
    QDir().mkpath(folder);
    QString path = QFileDialog::getSaveFileName(this, tr("Bild speichern"), QDir(folder).filePath(suggestedFileName()),
                                                tr("PNG-Bild (*.png)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");
    writeImage(path);
}

QString MainWindow::suggestedFileName() const
{
    return FileNames::fromPattern(Settings::fileNamePattern(), m_imageName, QDateTime::currentDateTime());
}

bool MainWindow::writeImage(const QString &path)
{
    if (!m_view->renderedImage().save(path, "PNG")) {
        QMessageBox::warning(this, tr("Bild speichern"), tr("«%1» konnte nicht gespeichert werden.").arg(path));
        return false;
    }
    m_savedPath = path;
    m_lastSaveFolder = QFileInfo(path).absolutePath();
    markHandedOff();
    showDone(m_saveButton, tr("Gespeichert"));
    statusBar()->showMessage(tr("Gespeichert: %1 · Ctrl+E zeigt die Datei").arg(path), 6000);
    return true;
}

void MainWindow::markHandedOff()
{
    // Saved or copied: clearing needs no question until the next change.
    m_handedOff = true;
    m_undoStack->setClean();
}

bool MainWindow::hasUnsavedChanges() const
{
    return m_view->hasImage() && (!m_handedOff || !m_undoStack->isClean());
}

bool MainWindow::confirmDiscard(const QString &title, const QString &question)
{
    const auto answer = QMessageBox::question(
        this, title, tr("Das Bild wurde seit der letzten Änderung weder gespeichert noch kopiert. %1").arg(question),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    return answer == QMessageBox::Yes;
}

void MainWindow::showDone(QPushButton *button, const QString &text)
{
    // A short confirmation on the button itself: a check mark and "Kopiert"
    // or "Gespeichert", then back. The width stays, so nothing jumps.
    const bool primary = button == m_saveButton;
    QTimer *timer = primary ? m_saveFeedback : m_copyFeedback;
    if (!timer->isActive()) {
        button->setProperty("normalText", button->text());
        button->setProperty("normalIcon", button->icon());
        button->setMinimumWidth(button->width());
    }
    button->setText(text);
    button->setIcon(Icons::icon(QStringLiteral("check"), primary ? QColor(Qt::white) : m_successColor));
    button->setProperty("done", true);
    button->style()->unpolish(button); // the style sheet has a "done" look
    button->style()->polish(button);
    timer->start();
}

void MainWindow::restoreButton(QPushButton *button)
{
    button->setText(button->property("normalText").toString());
    button->setIcon(button->property("normalIcon").value<QIcon>());
    button->setProperty("done", false);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

void MainWindow::showSavedFile()
{
    if (m_savedPath.isEmpty()) {
        statusBar()->showMessage(tr("Das Bild ist noch nicht gespeichert."), 3000);
        return;
    }
#ifdef NSL_HAVE_GNOME_BACKEND
    // Selects the file in the file manager (freedesktop FileManager1 interface).
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                  QStringLiteral("/org/freedesktop/FileManager1"),
                                                  QStringLiteral("org.freedesktop.FileManager1"),
                                                  QStringLiteral("ShowItems"));
    message << QStringList{QUrl::fromLocalFile(m_savedPath).toString()} << QString();
    if (QDBusConnection::sessionBus().send(message))
        return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_savedPath).absolutePath()));
}

void MainWindow::copyImage()
{
    if (!m_view->hasImage())
        return;
    QApplication::clipboard()->setImage(m_view->renderedImage());
    markHandedOff();
    showDone(m_copyButton, tr("Kopiert"));
    statusBar()->showMessage(tr("In die Zwischenablage kopiert."), 3000);
}

void MainWindow::copyFilePath()
{
    if (m_savedPath.isEmpty()) {
        statusBar()->showMessage(tr("Das Bild ist noch nicht gespeichert."), 3000);
        return;
    }
    QApplication::clipboard()->setText(m_savedPath);
    statusBar()->showMessage(tr("Dateipfad kopiert."), 3000);
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(this);
    connect(&dialog, &SettingsDialog::advancedModeChanged, m_properties, &PropertiesPanel::setAdvancedMode);
    connect(&dialog, &SettingsDialog::hotkeysChanged, this, [this, &dialog] {
        pushHotkeyOptions();
        dialog.setHotkeyProblem(m_capture->hotkeyProblem());
    });
    dialog.setHotkeyProblem(Settings::globalHotkeys().enabled ? m_capture->hotkeyProblem() : QString());
    connect(&dialog, &SettingsDialog::stylesReset, this, [this] {
        loadDefaultStyles();
        onSelectionChanged(); // the panel shows the defaults again
    });
    dialog.exec();
}

void MainWindow::clearEditor()
{
    if (!m_view->hasImage())
        return;
    // Only unsaved work needs a question (F-10).
    if (hasUnsavedChanges() && !confirmDiscard(tr("Editor leeren"), tr("Trotzdem leeren?")))
        return;
    m_undoStack->clear();
    m_view->clear();
    m_imageName.clear();
    m_savedPath.clear();
    updateActions();
    updateZoomLabel();
}
