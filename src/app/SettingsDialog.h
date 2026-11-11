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
    // `countdownHasEscape`: Escape cancels the countdown on this desktop;
    // where it does not, the click on it cannot be switched off (D-025).
    explicit SettingsDialog(GlobalHotkeys *hotkeys, bool countdownHasEscape, QWidget *parent = nullptr);
    ~SettingsDialog() override;

signals:
    void advancedModeChanged(bool enabled);
    // Series of captures switched on or off (F-52).
    void autoSaveChanged(bool enabled);
    // The styles for new objects were reset to the defaults (F-22).
    void stylesReset();
    // "Updates" (F-49): the releases from a check, for the update button
    // of the main window; and a version the user chose to install.
    void releasesLoaded(const QList<Releases::Release> &releases);
    void installRequested(const Releases::Release &release);

private:
    // "Tastenkürzel": the global hotkeys, and whether each one works.
    void createHotkeyPage(QVBoxLayout *layout);
    // "Countdown" on the page "Allgemein" (F-04): where it appears, click.
    void createCountdownSection(QVBoxLayout *layout, bool hasEscape);
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
