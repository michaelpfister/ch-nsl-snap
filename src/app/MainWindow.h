#pragma once

#include "app/CommandLine.h"
#include "app/SingleInstance.h"
#include "capture/CaptureResult.h"
#include "editor/Tool.h"

#include <QColor>
#include <QMainWindow>

#include <memory>
#include <optional>

class CaptureBackend;
class GlobalHotkeys;
class EditorView;
class PropertiesPanel;
class QComboBox;
class QLabel;
class QPushButton;
class QSystemTrayIcon;
class QTimer;
class QToolButton;
class QUndoStack;
class ToolRail;
struct FrameStyle;
struct RedactStyle;
struct ReplaceStyle;
struct TextStyle;
struct ArrowStyle;
struct MarkerStyle;
class ArrowItem;
class MarkerItem;
class TextItem;
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

    // A request from the command line (F-48), from this start or handed
    // over by a later one (SingleInstance). Without a capture it shows the
    // editor; with one it captures, then saves to the save folder or, with
    // `open`, shows the capture in the editor. `reply` is called once with
    // the outcome and the saved path or the error.
    void runRequest(const CommandLine::Request &request, const SingleInstance::Reply &reply);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *createTopBar();
    void createShortcuts();
    // Tray icon (F-45), if the desktop has a tray.
    void createTrayIcon();
    void showEditor();
    // Hides the window if the tray icon is there and the setting allows it.
    bool hideToTray();
    // "Beenden": asks first if the image is neither saved nor copied.
    void quit();

    void captureWindow();
    void captureRegion();
    void captureScreen();
    // Ctrl+N: the mode of the last capture (F-46).
    void captureLastMode();
    // `delay` in seconds; -1: the timer in the top bar (F-04).
    // `activeWindow`: the window with the focus, without picking (F-42).
    void startCapture(CaptureResult::Kind kind, int delay = -1, bool activeWindow = false);
    // Global hotkeys (F-42): registers the keys from the settings.
    void createGlobalHotkeys();
    // Once on Windows: Print opens the Snipping Tool (D-035).
    void showPrintKeyHint();
    QToolButton *modeButton(CaptureResult::Kind kind) const;
    void onCaptured(const CaptureResult &result);
    void onCaptureEnded(bool showWindow = true);
    // A capture from the command line without --open: straight into the
    // save folder, the editor's image stays as it is. Returns the path, or
    // an empty string and the error in `error`.
    QString saveCapture(const CaptureResult &result, QString *error);
    // Ends the command-line request, if one is running.
    void finishRequest(CommandLine::Outcome outcome, const QString &detail = QString());

    void openImage();
    void pasteImage();
    void loadImageFile(const QString &path);
    void saveImage();
    void saveImageAs();
    bool writeImage(const QString &path);
    // The file name from the pattern in the settings (F-41).
    QString suggestedFileName() const;
    // After saving or copying: no question when clearing (F-10).
    void markHandedOff();
    bool hasUnsavedChanges() const;
    // "… weder gespeichert noch kopiert. <question>" with Yes / Cancel.
    bool confirmDiscard(const QString &title, const QString &question);
    // Feedback on Kopieren / Speichern: check mark and text for a moment.
    void showDone(QPushButton *button, const QString &text);
    void restoreButton(QPushButton *button);
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
    void onTextStyleChanged(const TextStyle &style);
    // Takes the styles for new objects from the settings (F-22).
    void loadDefaultStyles();
    QList<TextItem *> selectedTexts() const;
    void onArrowStyleChanged(const ArrowStyle &style);
    void onMarkerStyleChanged(const MarkerStyle &style);
    QList<ArrowItem *> selectedArrows() const;
    QList<MarkerItem *> selectedMarkers() const;
    ReplaceItem *selectedReplace() const;
    QList<RedactItem *> selectedRedactions() const;
    void updateObjectList();
    void selectObject(int index);
    void updateActions();
    void updateZoomLabel();
    void updateImageInfo();
    // Applies or cancels the crop draft and goes back to the select tool.
    void finishCrop(bool apply);

    // Declared before m_view, which needs it at construction.
    QUndoStack *m_undoStack = nullptr;
    EditorView *m_view = nullptr;
    ToolRail *m_toolRail = nullptr;
    PropertiesPanel *m_properties = nullptr;
    std::unique_ptr<CaptureBackend> m_capture;
    GlobalHotkeys *m_hotkeys = nullptr;
    bool m_capturing = false; // from the start until captured/cancelled/failed
    // The mode of the last capture; the overlay can switch it (W, R).
    CaptureResult::Kind m_lastMode = CaptureResult::Kind::Window;

    QString m_imageName;  // for the suggested file name (F-41)
    QString m_imageSource; // status bar: where the image came from
    QString m_savedPath;  // last saved file, for Ctrl+E and Ctrl+Alt+C
    QString m_lastSaveFolder; // the save dialog starts here
    bool m_handedOff = false; // saved or copied since the image came in
    QSystemTrayIcon *m_tray = nullptr;
    bool m_quitting = false;
    bool m_hiddenBeforeCapture = false;
    // The command-line request whose capture is running (F-48).
    struct PendingRequest
    {
        CommandLine::Request request;
        SingleInstance::Reply reply;
    };
    std::optional<PendingRequest> m_request;

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
    QToolButton *m_clearButton = nullptr;
    QTimer *m_copyFeedback = nullptr;
    QTimer *m_saveFeedback = nullptr;
    QColor m_successColor; // theme colour of the check mark

    // Status bar.
    QLabel *m_imageInfo = nullptr;
    QLabel *m_toolHint = nullptr;
};
