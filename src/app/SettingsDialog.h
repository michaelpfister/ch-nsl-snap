#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;

// "Einstellungen" (F2, F-46). Changes apply immediately.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Shows why some hotkeys do not work (the backend says), or nothing.
    void setHotkeyProblem(const QString &problem);

signals:
    // A global hotkey setting changed (F-42); already stored.
    void hotkeysChanged();
    void advancedModeChanged(bool enabled);
    // The styles for new objects were reset to the defaults (F-22).
    void stylesReset();

private:
    QCheckBox *m_advanced = nullptr;
    QLabel *m_hotkeyProblem = nullptr;
};
