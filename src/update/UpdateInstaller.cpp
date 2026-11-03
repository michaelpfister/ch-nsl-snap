#include "update/UpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>

namespace {

// The ZIP holds one folder with snapslash.exe and the Qt files.
const char *const kZipFolder = "Snapslash";

// Programs from the Windows folder by full path, never through PATH.
QString windowsProgram(const QString &relativePath)
{
    const QString root = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"));
    return QDir::toNativeSeparators(root + QLatin1Char('/') + relativePath);
}

} // namespace

UpdateInstaller::UpdateInstaller(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_appDir(QCoreApplication::applicationDirPath())
{
}

UpdateInstaller::~UpdateInstaller()
{
    cancel();
}

bool UpdateInstaller::isSupported()
{
    return Releases::installation() == Releases::Installation::PortableWindows;
}

void UpdateInstaller::setApplicationDir(const QString &dir)
{
    m_appDir = QDir::cleanPath(dir);
}

QString UpdateInstaller::stageDir() const
{
    return m_appDir + QStringLiteral(".update");
}

QString UpdateInstaller::zipPath() const
{
    return m_appDir + QStringLiteral(".update.zip");
}

QString UpdateInstaller::newAppDir() const
{
    return stageDir() + QLatin1Char('/') + QLatin1String(kZipFolder);
}

void UpdateInstaller::prepare(const Releases::Release &release)
{
    if (isRunning())
        return;
    cleanUp(); // leftovers of an earlier attempt

    const QString name = Releases::windowsAssetName(release.version);
    const Releases::Asset *asset = release.asset(name);
    if (!asset) {
        fail(tr("Zu Version %1 gibt es kein Paket «%2».").arg(release.version.toString(), name));
        return;
    }
    m_asset = *asset;
    m_expectedSha256 = asset->sha256;
    if (!m_expectedSha256.isEmpty()) {
        download(m_asset.url);
        return;
    }
    // Older GitHub answers have no digest: the release's checksum file.
    const Releases::Asset *sums = release.asset(Releases::checksumFileName());
    if (!sums) {
        fail(tr("Zu Version %1 gibt es keine Prüfsumme; ohne sie wird nichts installiert.")
                 .arg(release.version.toString()));
        return;
    }
    m_fetchingChecksums = true;
    download(sums->url);
}

void UpdateInstaller::download(const QUrl &url)
{
    if (!m_fetchingChecksums) {
        m_file.setFileName(zipPath());
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            fail(tr("Der Ordner neben der App ist nicht beschreibbar: %1")
                     .arg(QDir::toNativeSeparators(QFileInfo(m_appDir).absolutePath())));
            return;
        }
        m_hash.reset();
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Snapslash/%1").arg(Version::current().toString()));
    // GitHub answers with a redirect to its download servers; Qt follows
    // redirects that stay on https by default.
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_fetchingChecksums)
            return; // small: read at the end
        const QByteArray chunk = m_reply->readAll();
        m_file.write(chunk);
        m_hash.addData(chunk);
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (!m_fetchingChecksums)
            emit progress(received, total > 0 ? total : m_asset.size > 0 ? m_asset.size : -1);
    });
    connect(m_reply, &QNetworkReply::finished, this, &UpdateInstaller::onDownloadFinished);
}

void UpdateInstaller::onDownloadFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        fail(tr("Der Download ist fehlgeschlagen: %1").arg(reply->errorString()));
        return;
    }
    if (m_fetchingChecksums) {
        m_fetchingChecksums = false;
        m_expectedSha256 = Releases::parseChecksums(reply->readAll()).value(m_asset.name);
        if (m_expectedSha256.isEmpty()) {
            fail(tr("Die Prüfsumme für «%1» fehlt; ohne sie wird nichts installiert.").arg(m_asset.name));
            return;
        }
        download(m_asset.url);
        return;
    }
    const QByteArray rest = reply->readAll();
    m_file.write(rest);
    m_hash.addData(rest);
    m_file.close();
    verifyAndUnpack();
}

void UpdateInstaller::verifyAndUnpack()
{
    if (m_hash.result().toHex() != m_expectedSha256) {
        fail(tr("Die heruntergeladene Datei stimmt nicht mit der Prüfsumme überein und wurde gelöscht."));
        return;
    }
#ifdef Q_OS_WIN
    if (!QDir().mkpath(stageDir())) {
        fail(tr("Der Ordner für die neue Version lässt sich nicht anlegen: %1").arg(QDir::toNativeSeparators(stageDir())));
        return;
    }
    // tar.exe comes with Windows 10 (1803) and later and unpacks ZIP files.
    m_unpack = new QProcess(this);
    connect(m_unpack, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        const QByteArray output = m_unpack->readAllStandardError();
        m_unpack->deleteLater();
        m_unpack = nullptr;
        const bool unpacked = status == QProcess::NormalExit && exitCode == 0
                              && QFileInfo::exists(newAppDir() + QStringLiteral("/snapslash.exe"));
        if (!unpacked) {
            fail(tr("Das Paket liess sich nicht entpacken. %1").arg(QString::fromLocal8Bit(output).trimmed()));
            return;
        }
        QFile::remove(zipPath());
        emit ready();
    });
    connect(m_unpack, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_unpack->deleteLater();
            m_unpack = nullptr;
            fail(tr("Windows hat kein tar.exe zum Entpacken (ab Windows 10, Version 1803)."));
        }
    });
    m_unpack->start(windowsProgram(QStringLiteral("System32/tar.exe")),
                    {QStringLiteral("-xf"), QDir::toNativeSeparators(zipPath()), QStringLiteral("-C"),
                     QDir::toNativeSeparators(stageDir())});
#else
    fail(tr("Nur die portable Windows-Version kann sich selbst ersetzen."));
#endif
}

QString UpdateInstaller::writeSwapScript(const QString &directory)
{
    QFile source(QStringLiteral(":/update/swap-folder.ps1"));
    const QString path = directory + QStringLiteral("/snapslash-swap-folder.ps1");
    QFile target(path);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    target.write(source.readAll());
    return path;
}

bool UpdateInstaller::startSwap(qint64 processId, const QString &executable) const
{
    const QString script = writeSwapScript(QDir::tempPath());
    if (script.isEmpty())
        return false;
    QStringList arguments = {
        QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
        QStringLiteral("-File"), QDir::toNativeSeparators(script),
        QStringLiteral("-ProcessId"), QString::number(processId),
        QStringLiteral("-AppDir"), QDir::toNativeSeparators(m_appDir),
        QStringLiteral("-NewDir"), QDir::toNativeSeparators(newAppDir()),
        QStringLiteral("-Stage"), QDir::toNativeSeparators(stageDir()),
    };
    if (!executable.isEmpty())
        arguments << QStringLiteral("-Exe") << executable;
    // Started detached, so it outlives the app; its working folder is the
    // parent, never the app folder it is about to move.
    return QProcess::startDetached(windowsProgram(QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe")),
                                   arguments, QFileInfo(m_appDir).absolutePath());
}

void UpdateInstaller::cancel()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_unpack) {
        m_unpack->disconnect(this);
        m_unpack->kill();
        m_unpack->waitForFinished(2000);
        m_unpack->deleteLater();
        m_unpack = nullptr;
    }
    m_fetchingChecksums = false;
    if (m_file.isOpen())
        m_file.close();
    QFile::remove(zipPath());
}

void UpdateInstaller::discard()
{
    cancel();
    cleanUp();
}

void UpdateInstaller::fail(const QString &message)
{
    cancel();
    cleanUp();
    emit failed(message);
}

void UpdateInstaller::cleanUp()
{
    QFile::remove(zipPath());
    QDir stage(stageDir());
    if (stage.exists())
        stage.removeRecursively();
}
