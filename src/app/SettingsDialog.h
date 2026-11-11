#pragma once

#include "update/Releases.h"

#include <QDialog>

class GlobalHotkeys;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class UpdateChecker;

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
    // "Updates" (F-49): the releases from a check, for the update button
    // of the main window; and a version the user chose to install.
    void releasesLoaded(const QList<Releases::Release> &releases);
    void installRequested(const Releases::Release &release);

private:
    // "Tastenkürzel": the global hotkeys, and whether each one works.
    void createHotkeyPage(QVBoxLayout *layout);
    // "Updates": automatic check, pre-releases, and every version to choose
    // from, older ones included (D-037).
    void createUpdatePage(QVBoxLayout *layout);
    void checkForUpdates();
    void fillVersions();

    GlobalHotkeys *m_hotkeys = nullptr;
    QCheckBox *m_advanced = nullptr;
    UpdateChecker *m_updates = nullptr;
    QList<Releases::Release> m_releases;
    QLabel *m_updateStatus = nullptr;
    QPushButton *m_checkButton = nullptr;
    QComboBox *m_versions = nullptr;
    QPushButton *m_installVersion = nullptr;
};
