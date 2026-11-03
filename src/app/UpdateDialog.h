#pragma once

#include "update/Releases.h"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class UpdateInstaller;

// Shows one release (F-49, D-037): what is new, and how to get it. On the
// portable Windows version it downloads and unpacks it; the caller then
// starts the swap and quits (readyToSwap()). Elsewhere it explains the
// steps for Nix or a build from source. Works for older versions alike.
class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(const Releases::Release &release, QWidget *parent = nullptr);
    ~UpdateDialog() override;

    // After accept(): the new version is unpacked next to the app folder.
    bool readyToSwap() const { return m_ready; }
    UpdateInstaller *installer() const { return m_installer; }

    // The steps for an installation that cannot replace itself, as
    // Markdown with the commands for this release.
    static QString instructions(Releases::Installation installation, const Releases::Release &release);

private:
    void install();

    Releases::Release m_release;
    UpdateInstaller *m_installer = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_installButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    bool m_ready = false;
};
