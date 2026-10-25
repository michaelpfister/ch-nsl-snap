#include "app/MainWindow.h"

#include "capture/CaptureBackend.h"
#include "editor/EditorView.h"
#include "imaging/WindowImage.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>

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

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_view(new EditorView(this))
    , m_undoStack(new QUndoStack(this))
    , m_capture(createCaptureBackend())
{
    setWindowTitle(QStringLiteral("NSL Snap"));
    setCentralWidget(m_view);
    resize(1100, 750);

    connect(m_view, &EditorView::fileDropped, this, &MainWindow::loadImageFile);
    connect(m_capture.get(), &CaptureBackend::captured, this, &MainWindow::onCaptured);
    connect(m_capture.get(), &CaptureBackend::cancelled, this, [this] {
        onCaptureEnded();
        statusBar()->showMessage(tr("Aufnahme abgebrochen."), 3000);
    });
    connect(m_capture.get(), &CaptureBackend::failed, this, [this](const QString &message) {
        onCaptureEnded();
        QMessageBox::warning(this, tr("Aufnahme fehlgeschlagen"), message);
    });

    createActions();
    updateActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    auto *toolbar = addToolBar(tr("Werkzeuge"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_captureWindowAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("camera-photo")),
                                               tr("Fenster aufnehmen"), this, &MainWindow::captureWindow);
    m_captureWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    toolbar->addSeparator();

    auto *openAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("document-open")),
                                          tr("Öffnen…"), this, &MainWindow::openImage);
    openAction->setShortcut(QKeySequence::Open);

    m_saveAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("document-save")),
                                      tr("Speichern…"), this, &MainWindow::saveImage);
    m_saveAction->setShortcut(QKeySequence::Save);

    m_copyAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")),
                                      tr("Kopieren"), this, &MainWindow::copyImage);
    m_copyAction->setShortcut(QKeySequence::Copy);
    toolbar->addSeparator();

    QAction *undoAction = m_undoStack->createUndoAction(this, tr("Rückgängig"));
    undoAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    undoAction->setShortcut(QKeySequence::Undo);
    toolbar->addAction(undoAction);

    QAction *redoAction = m_undoStack->createRedoAction(this, tr("Wiederholen"));
    redoAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-redo")));
    redoAction->setShortcut(QKeySequence::Redo);
    toolbar->addAction(redoAction);
    toolbar->addSeparator();

    m_clearAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("edit-clear")),
                                       tr("Leeren"), this, &MainWindow::clearEditor);
}

void MainWindow::updateActions()
{
    const bool hasImage = m_view->hasImage();
    m_saveAction->setEnabled(hasImage);
    m_copyAction->setEnabled(hasImage);
    m_clearAction->setEnabled(hasImage);
}

void MainWindow::captureWindow()
{
    if (!m_capture->isAvailable()) {
        QMessageBox::warning(this, tr("Aufnahme nicht möglich"), m_capture->unavailableReason());
        return;
    }
    m_captureWindowAction->setEnabled(false);
    hide();
    QTimer::singleShot(kHideDelayMs, m_capture.get(), &CaptureBackend::captureWindow);
}

void MainWindow::onCaptured(const CaptureResult &result)
{
    onCaptureEnded();
    const QImage image = WindowImage::prepare(result);
    setImage(image, result.scale, result.title);
    statusBar()->showMessage(tr("Fenster «%1» aufgenommen (%2 × %3 Pixel).")
                                 .arg(result.title).arg(image.width()).arg(image.height()),
                             5000);
}

void MainWindow::onCaptureEnded()
{
    m_captureWindowAction->setEnabled(true);
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
    setImage(image, 1.0, QFileInfo(path).completeBaseName());
}

void MainWindow::setImage(const QImage &image, double scale, const QString &name)
{
    // A new image starts a new document: older undo steps no longer apply.
    m_undoStack->clear();
    m_view->setImage(image, scale);
    m_imageName = name;
    updateActions();
}

void MainWindow::saveImage()
{
    QDir().mkpath(defaultSaveDir());
    const QString suggested = QDir(defaultSaveDir()).filePath(suggestedFileName(m_imageName));
    const QString path = QFileDialog::getSaveFileName(this, tr("Bild speichern"), suggested,
                                                      tr("PNG-Bild (*.png)"));
    if (path.isEmpty())
        return;
    if (!m_view->image().save(path, "PNG")) {
        QMessageBox::warning(this, tr("Bild speichern"),
                             tr("«%1» konnte nicht gespeichert werden.").arg(path));
        return;
    }
    statusBar()->showMessage(tr("Gespeichert: %1").arg(path), 5000);
}

void MainWindow::copyImage()
{
    QApplication::clipboard()->setImage(m_view->image());
    statusBar()->showMessage(tr("In die Zwischenablage kopiert."), 3000);
}

void MainWindow::clearEditor()
{
    const auto answer = QMessageBox::question(this, tr("Leeren"),
                                              tr("Das Bild und alle Änderungen aus dem Editor entfernen?"));
    if (answer != QMessageBox::Yes)
        return;
    m_undoStack->clear();
    m_view->clear();
    m_imageName.clear();
    updateActions();
}
