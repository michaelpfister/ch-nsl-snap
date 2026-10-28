#pragma once

#include <QDialog>

class GlobalHotkeys;
class QCheckBox;
class QVBoxLayout;

// "Einstellungen" (F2, F-46). Changes apply immediately.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // `hotkeys` may be null where the app has no global hotkeys (F-42).
    explicit SettingsDialog(GlobalHotkeys *hotkeys, QWidget *parent = nullptr);
    ~SettingsDialog() override;

signals:
    void advancedModeChanged(bool enabled);
    // The styles for new objects were reset to the defaults (F-22).
    void stylesReset();

private:
    // "Tastenkürzel": the global hotkeys, and whether each one works.
    void createHotkeyPage(QVBoxLayout *layout);

    GlobalHotkeys *m_hotkeys = nullptr;
    QCheckBox *m_advanced = nullptr;
};
