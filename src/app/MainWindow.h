#pragma once

#include "editor/Tool.h"

#include <QMainWindow>

#include <memory>

class CaptureBackend;
class EditorView;
class PropertiesPanel;
class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;
class QUndoStack;
class ToolRail;
struct CaptureResult;
struct FrameStyle;
struct RedactStyle;
struct ReplaceStyle;
class ReplaceItem;
class RedactItem;
struct Theme;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Re-colours icons after a light/dark switch; the style sheet itself is
    // set on the application (Theme::apply).
    void applyTheme(const Theme &theme);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWidget *createTopBar();
    void createShortcuts();

    void captureWindow();
    void onCaptured(const CaptureResult &result);
    void onCaptureEnded();

    void openImage();
    void pasteImage();
    void loadImageFile(const QString &path);
    void saveImage();
    void saveImageAs();
    bool writeImage(const QString &path);
    void showSavedFile();
    void copyImage();
    void copyFilePath();
    void clearEditor();
    void showSettings();

    void setImage(const QImage &image, double scale, const QString &name, const QString &source);
    void onToolChanged(ToolId id);
    void onSelectionChanged();
    void onFrameStyleChanged(const FrameStyle &style);
    void onRedactStyleChanged(const RedactStyle &style);
    void onReplaceStyleChanged(const ReplaceStyle &style);
    ReplaceItem *selectedReplace() const;
    QList<RedactItem *> selectedRedactions() const;
    void updateObjectList();
    void selectObject(int index);
    void updateActions();
    void updateZoomLabel();

    // Declared before m_view, which needs it at construction.
    QUndoStack *m_undoStack = nullptr;
    EditorView *m_view = nullptr;
    ToolRail *m_toolRail = nullptr;
    PropertiesPanel *m_properties = nullptr;
    std::unique_ptr<CaptureBackend> m_capture;

    QString m_imageName;  // for the suggested file name (F-41)
    QString m_savedPath;  // last saved file, for Ctrl+E and Ctrl+Alt+C

    // Top bar.
    QLabel *m_logo = nullptr;
    QToolButton *m_windowButton = nullptr;
    QToolButton *m_regionButton = nullptr;
    QToolButton *m_screenButton = nullptr;
    QLabel *m_timerIcon = nullptr;
    QComboBox *m_timer = nullptr;
    QToolButton *m_undoButton = nullptr;
    QToolButton *m_redoButton = nullptr;
    // Zoom (D-017): minus, percentage with a menu, plus.
    QToolButton *m_zoomOutButton = nullptr;
    QToolButton *m_zoomButton = nullptr;
    QToolButton *m_zoomInButton = nullptr;
    QToolButton *m_fitButton = nullptr;
    QAction *m_fitAction = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_saveButton = nullptr;

    // Status bar.
    QLabel *m_imageInfo = nullptr;
    QLabel *m_toolHint = nullptr;
};
