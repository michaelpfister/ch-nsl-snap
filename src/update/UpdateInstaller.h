#pragma once

#include "update/Releases.h"

#include <QCryptographicHash>
#include <QFile>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

// Installs another version over a portable Windows installation (D-037),
// newer or older alike:
//   1. prepare(): download the release's ZIP for this CPU, check its SHA-256,
//      unpack it next to the app folder;
//   2. startSwap(): start a hidden PowerShell script that waits until the
//      app has quit, moves the app folder aside, moves the new one into its
//      place and starts it. If that fails, it puts the old folder back.
// Between the two, the caller asks the user about unsaved work and quits.
class UpdateInstaller : public QObject
{
    Q_OBJECT

public:
    explicit UpdateInstaller(QObject *parent = nullptr);
    ~UpdateInstaller() override;

    // Whether this installation can replace itself at all.
    static bool isSupported();

    // The folder with snapslash.exe; the default is the running program's.
    // The tests point it elsewhere.
    void setApplicationDir(const QString &dir);
    QString applicationDir() const { return m_appDir; }

    void prepare(const Releases::Release &release);
    void cancel();
    // Cancels and removes what prepare() unpacked (the user said no).
    void discard();
    bool isRunning() const { return m_reply != nullptr || m_unpack != nullptr; }

    // After ready(): starts the script for the process `processId` (the
    // app itself, which must then quit). Without `executable` the script
    // starts nothing afterwards. Returns false if PowerShell did not start.
    bool startSwap(qint64 processId, const QString &executable = QStringLiteral("snapslash.exe")) const;
    // The unpacked new app folder (valid after ready()).
    QString newAppDir() const;
    // The PowerShell script, written to this path by startSwap().
    static QString writeSwapScript(const QString &directory);

signals:
    // While downloading; `total` is -1 when the size is unknown.
    void progress(qint64 received, qint64 total);
    void ready();
    // A German message for the user.
    void failed(const QString &message);

private:
    void download(const QUrl &url);
    void onDownloadFinished();
    void verifyAndUnpack();
    void fail(const QString &message);
    void cleanUp();
    QString stageDir() const; // next to the app folder, on the same drive
    QString zipPath() const;

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QProcess *m_unpack = nullptr;
    QString m_appDir;
    Releases::Asset m_asset;
    QByteArray m_expectedSha256;
    bool m_fetchingChecksums = false;
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
};
