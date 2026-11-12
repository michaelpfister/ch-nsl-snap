#pragma once

#include "update/Releases.h"

#include <QStringList>
#include <QWidget>

class GlobalHotkeys;
class QBoxLayout;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QEventLoop;
class QFrame;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class UpdateChecker;

// "Einstellungen" (F2, F-46). Changes apply immediately.
//
// Not a window of its own but an overlay over the main window (D-044):
// a card over the dimmed window, never larger than it, with the pages
// on the left (on top when the window is narrow) and each page scrolling
// inside the card. Escape, the close button or a click beside the card
// close it.
class SettingsDialog : public QWidget
{
    Q_OBJECT

public:
    // `hotkeys` may be null where the app has no global hotkeys (F-42).
    // `countdownHasEscape`: Escape cancels the countdown on this desktop;
    // where it does not, the click on it cannot be switched off (D-025).
    explicit SettingsDialog(GlobalHotkeys *hotkeys, bool countdownHasEscape, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    // Shows the overlay over the window and waits until it is closed,
    // like QDialog::exec().
    void exec();
    void close();
    // The overlay is open (the global hotkeys wait meanwhile).
    static bool isOpen();

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

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // The card's place and size in the window, and wide or narrow layout.
    void arrange();
    void showPage(int index);
    // The navigation's icons in the theme's colours.
    void applyNavIcons();
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
    QFrame *m_card = nullptr;
    QGridLayout *m_cardLayout = nullptr;
    QWidget *m_navPane = nullptr;
    QLabel *m_navTitle = nullptr;
    QScrollArea *m_navScroll = nullptr;
    QWidget *m_navButtons = nullptr;
    QBoxLayout *m_navButtonsLayout = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QWidget *m_header = nullptr;
    QLabel *m_headerTitle = nullptr;
    QStackedWidget *m_pages = nullptr;
    QStringList m_titles;
    bool m_narrow = false;
    QEventLoop *m_loop = nullptr;
    QCheckBox *m_advanced = nullptr;
    UpdateChecker *m_updates = nullptr;
    QList<Releases::Release> m_releases;
    QLabel *m_updateStatus = nullptr;
    QPushButton *m_checkButton = nullptr;
    QComboBox *m_versions = nullptr;
    QPushButton *m_installVersion = nullptr;
};
