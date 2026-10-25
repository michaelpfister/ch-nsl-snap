#pragma once

#include <QMainWindow>

#include <memory>

class CaptureBackend;
class EditorView;
class QUndoStack;
struct CaptureResult;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void createActions();

    void captureWindow();
    void onCaptured(const CaptureResult &result);
    void onCaptureEnded();

    void openImage();
    void loadImageFile(const QString &path);
    void saveImage();
    void copyImage();
    void clearEditor();

    void setImage(const QImage &image, double scale, const QString &name);
    void updateActions();

    EditorView *m_view = nullptr;
    QUndoStack *m_undoStack = nullptr;
    std::unique_ptr<CaptureBackend> m_capture;

    // Used for the suggested file name when saving (F-41).
    QString m_imageName;

    QAction *m_captureWindowAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_clearAction = nullptr;
};
