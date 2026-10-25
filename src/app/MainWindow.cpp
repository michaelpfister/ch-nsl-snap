#include "app/MainWindow.h"

#include "app/PropertiesPanel.h"
#include "app/ToolRail.h"
#include "capture/CaptureBackend.h"
#include "editor/EditorView.h"
#include "imaging/WindowImage.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QAction>
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

#ifdef NSL_HAVE_GNOME_BACKEND
#include <QDBusConnection>
#include <QDBusMessage>
#endif

namespace {

// Time the compositor needs to unmap our window before the capture starts,
// so the app does not show up in it (D-011).
constexpr int kHideDelayMs = 250;

// "2026-10-25_16-27-43_Home.png", as in the PoC client.
QString suggestedFileName(const QString &name)
{
    QString slug = name;
    slug.replace(QRegularExpression(QStringLiteral(R"([^\w.-]+)")), QStringLiteral("_"));
    slug = slug.left(60);
    while (slug.startsWith(u'_'))
        slug.remove(0, 1);
    while (slug.endsWith(u'_'))
        slug.chop(1);
    if (slug.isEmpty())
        slug = QStringLiteral("screenshot");
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    return stamp + u'_' + slug + QStringLiteral(".png");
}

QString defaultSaveDir()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return QDir(pictures.isEmpty() ? QDir::homePath() : pictures).filePath(QStringLiteral("NSL Snap"));
}

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
    , m_view(new EditorView(this))
    , m_toolRail(new ToolRail(this))
    , m_properties(new PropertiesPanel(this))
    , m_undoStack(new QUndoStack(this))
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

    // The dim toggle exists twice (rail and panel); keep them in sync.
    connect(m_toolRail->dimButton(), &QToolButton::toggled, m_properties->dimCheckBox(), &QAbstractButton::setChecked);
    connect(m_properties->dimCheckBox(), &QAbstractButton::toggled, m_toolRail->dimButton(), &QAbstractButton::setChecked);

    connect(m_capture.get(), &CaptureBackend::captured, this, &MainWindow::onCaptured);
    connect(m_capture.get(), &CaptureBackend::cancelled, this, [this] {
        onCaptureEnded();
        statusBar()->showMessage(tr("Aufnahme abgebrochen."), 3000);
    });
    connect(m_capture.get(), &CaptureBackend::failed, this, [this](const QString &message) {
        onCaptureEnded();
        QMessageBox::warning(this, tr("Aufnahme fehlgeschlagen"), message);
    });

    createShortcuts();
    onToolChanged(m_toolRail->currentTool());
    updateActions();
    updateZoomLabel();
}

MainWindow::~MainWindow() = default;

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
    m_windowButton->setChecked(true);
    for (QToolButton *button : {m_regionButton, m_screenButton}) {
        button->setEnabled(false);
        button->setToolTip(tr("Folgt mit dem Aufnahme-Overlay (D-011)"));
    }
    modesLayout->addWidget(m_windowButton);
    modesLayout->addWidget(m_regionButton);
    modesLayout->addWidget(m_screenButton);
    layout->addWidget(modes);
    connect(m_windowButton, &QToolButton::clicked, this, &MainWindow::captureWindow);

    // Timer (F-04). The countdown itself comes with the overlay.
    m_timerIcon = new QLabel(bar);
    auto *timerLabel = new QLabel(tr("Timer"), bar);
    timerLabel->setProperty("role", QStringLiteral("muted"));
    m_timer = new QComboBox(bar);
    m_timer->addItems({tr("Aus"), tr("3 s"), tr("5 s"), tr("10 s")});
    m_timer->setAccessibleName(tr("Timer"));
    m_timer->setEnabled(false);
    m_timer->setToolTip(tr("Folgt mit dem Aufnahme-Overlay (D-011)"));
    m_timer->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_timerIcon);
    layout->addWidget(timerLabel);
    layout->addWidget(m_timer);

    // Undo / redo (F-11).
    m_undoButton = iconButton(tr("Rückgängig (Ctrl+Z)"), bar);
    m_redoButton = iconButton(tr("Wiederholen (Ctrl+Y)"), bar);
    connect(m_undoButton, &QToolButton::clicked, m_undoStack, &QUndoStack::undo);
    connect(m_redoButton, &QToolButton::clicked, m_undoStack, &QUndoStack::redo);
    connect(m_undoStack, &QUndoStack::canUndoChanged, m_undoButton, &QWidget::setEnabled);
    connect(m_undoStack, &QUndoStack::canRedoChanged, m_redoButton, &QWidget::setEnabled);
    m_undoButton->setEnabled(false);
    m_redoButton->setEnabled(false);
    layout->addWidget(m_undoButton);
    layout->addWidget(m_redoButton);

    layout->addStretch(1);

    m_zoomLabel = new QLabel(bar);
    m_zoomLabel->setProperty("role", QStringLiteral("muted"));
    layout->addWidget(m_zoomLabel);

    m_copyButton = textButton(tr("Kopieren"), QStringLiteral("secondary"), bar);
    m_copyButton->setToolTip(tr("Bild in die Zwischenablage kopieren (Ctrl+C)"));
    m_saveButton = textButton(tr("Speichern"), QStringLiteral("primary"), bar);
    m_saveButton->setToolTip(tr("Im Ordner «NSL Snap» speichern (Ctrl+S)"));
    connect(m_copyButton, &QPushButton::clicked, this, &MainWindow::copyImage);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveImage);
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
    add(QKeySequence(Qt::CTRL | Qt::Key_N), &MainWindow::captureWindow); // last mode; only window so far
    add(QKeySequence(Qt::CTRL | Qt::Key_3), &MainWindow::captureWindow);
    add(QKeySequence::Open, &MainWindow::openImage);
    add(QKeySequence::Paste, &MainWindow::pasteImage);
    add(QKeySequence::Save, &MainWindow::saveImage);
    add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), &MainWindow::saveImageAs);
    add(QKeySequence(Qt::CTRL | Qt::Key_E), &MainWindow::showSavedFile);
    add(QKeySequence::Copy, &MainWindow::copyImage);
    add(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C), &MainWindow::copyFilePath);
    add(QKeySequence(Qt::CTRL | Qt::Key_Delete), &MainWindow::clearEditor);

    add(QKeySequence::Undo, [this] { m_undoStack->undo(); });
    auto *redo = new QAction(this);
    redo->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(redo, &QAction::triggered, m_undoStack, &QUndoStack::redo);
    addAction(redo);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Escape: leave the current tool first (F-46). Hiding to the tray
    // follows with the tray icon (F-45).
    if (event->key() == Qt::Key_Escape && m_toolRail->currentTool() != ToolId::Select) {
        m_toolRail->setCurrentTool(ToolId::Select);
        return;
    }
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
    m_copyButton->setIcon(Icons::icon(QStringLiteral("copy"), theme.text));
    m_saveButton->setIcon(Icons::icon(QStringLiteral("save"), QColor(Qt::white)));

    m_toolRail->applyTheme(theme);
    m_properties->applyTheme(theme);
}

void MainWindow::onToolChanged(ToolId id)
{
    m_properties->showTool(id);
    m_toolHint->setText(toolInfo(id).hint);
}

void MainWindow::updateActions()
{
    const bool hasImage = m_view->hasImage();
    m_saveButton->setEnabled(hasImage);
    m_copyButton->setEnabled(hasImage);
    if (!hasImage)
        m_imageInfo->setText(tr("Kein Bild"));
}

void MainWindow::updateZoomLabel()
{
    m_zoomLabel->setText(m_view->hasImage() ? QStringLiteral("%1 %").arg(qRound(m_view->zoom() * 100)) : QString());
}

void MainWindow::captureWindow()
{
    if (!m_capture->isAvailable()) {
        QMessageBox::warning(this, tr("Aufnahme nicht möglich"), m_capture->unavailableReason());
        return;
    }
    m_windowButton->setChecked(true);
    m_windowButton->setEnabled(false);
    hide();
    QTimer::singleShot(kHideDelayMs, m_capture.get(), &CaptureBackend::captureWindow);
}

void MainWindow::onCaptured(const CaptureResult &result)
{
    onCaptureEnded();
    const QImage image = WindowImage::prepare(result);
    setImage(image, result.scale, result.title, tr("Fenster «%1»").arg(result.title));
}

void MainWindow::onCaptureEnded()
{
    m_windowButton->setEnabled(true);
    show();
    raise();
    activateWindow();
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
    setImage(image, 1.0, name, QFileInfo(path).fileName());
}

void MainWindow::setImage(const QImage &image, double scale, const QString &name, const QString &source)
{
    // A new image starts a new document: older undo steps no longer apply.
    m_undoStack->clear();
    m_view->setImage(image, scale);
    m_imageName = name;
    m_savedPath.clear();
    m_imageInfo->setText(tr("%1 · %2 × %3 px · %4×")
                             .arg(source).arg(image.width()).arg(image.height())
                             .arg(QLocale().toString(scale, 'g', 3)));
    updateActions();
    updateZoomLabel();
}

void MainWindow::saveImage()
{
    // Ctrl+S saves without asking, like WinSnap (F-41).
    if (!m_view->hasImage())
        return;
    QDir().mkpath(defaultSaveDir());
    writeImage(QDir(defaultSaveDir()).filePath(suggestedFileName(m_imageName)));
}

void MainWindow::saveImageAs()
{
    if (!m_view->hasImage())
        return;
    QDir().mkpath(defaultSaveDir());
    const QString suggested = QDir(defaultSaveDir()).filePath(suggestedFileName(m_imageName));
    const QString path = QFileDialog::getSaveFileName(this, tr("Bild speichern unter"), suggested,
                                                      tr("PNG-Bild (*.png)"));
    if (!path.isEmpty())
        writeImage(path);
}

bool MainWindow::writeImage(const QString &path)
{
    if (!m_view->image().save(path, "PNG")) {
        QMessageBox::warning(this, tr("Bild speichern"), tr("«%1» konnte nicht gespeichert werden.").arg(path));
        return false;
    }
    m_savedPath = path;
    statusBar()->showMessage(tr("Gespeichert: %1 · Ctrl+E zeigt die Datei").arg(path), 6000);
    return true;
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
    QApplication::clipboard()->setImage(m_view->image());
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

void MainWindow::clearEditor()
{
    if (!m_view->hasImage())
        return;
    const auto answer = QMessageBox::question(this, tr("Leeren"),
                                              tr("Das Bild und alle Änderungen aus dem Editor entfernen?"));
    if (answer != QMessageBox::Yes)
        return;
    m_undoStack->clear();
    m_view->clear();
    m_imageName.clear();
    m_savedPath.clear();
    updateActions();
    updateZoomLabel();
}
