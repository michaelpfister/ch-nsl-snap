#include "app/SingleInstance.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>

#ifndef Q_OS_WIN
#include <unistd.h> // getuid()
#endif

namespace {

// A running app answers at once when it is there; a stale socket file
// from a crashed app refuses the connection.
constexpr int kConnectTimeoutMs = 1000;

const QString kArguments = QStringLiteral("arguments");
const QString kOutcome = QStringLiteral("outcome");
const QString kDetail = QStringLiteral("detail");

QString outcomeName(CommandLine::Outcome outcome)
{
    switch (outcome) {
    case CommandLine::Outcome::Saved: return QStringLiteral("saved");
    case CommandLine::Outcome::Opened: return QStringLiteral("opened");
    case CommandLine::Outcome::Cancelled: return QStringLiteral("cancelled");
    case CommandLine::Outcome::Failed: break;
    }
    return QStringLiteral("failed");
}

CommandLine::Outcome outcomeFromName(const QString &name)
{
    if (name == QLatin1String("saved"))
        return CommandLine::Outcome::Saved;
    if (name == QLatin1String("opened"))
        return CommandLine::Outcome::Opened;
    if (name == QLatin1String("cancelled"))
        return CommandLine::Outcome::Cancelled;
    return CommandLine::Outcome::Failed;
}

// Both sides send one JSON object on one line.
QByteArray line(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

SingleInstance::~SingleInstance() = default;

QString SingleInstance::defaultName()
{
#if defined(Q_OS_WIN)
    // A named pipe; the user name keeps users on one machine apart.
    return QStringLiteral("ch.netslash.Snap-") + qEnvironmentVariable("USERNAME");
#else
    // A full path in the user's own runtime folder; only without one in
    // the shared temporary folder, with the user's id in the name.
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty())
        return runtime + QStringLiteral("/ch.netslash.Snap.sock");
    return QDir::tempPath() + QStringLiteral("/ch.netslash.Snap-%1.sock").arg(getuid());
#endif
}

std::optional<SingleInstance::Answer> SingleInstance::forward(const QStringList &arguments, const QString &name)
{
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(kConnectTimeoutMs))
        return std::nullopt;

    socket.write(line({{kArguments, QJsonArray::fromStringList(arguments)}}));
    socket.flush();
    // No timeout: the user may take as long as they like to pick.
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(-1))
            return Answer{CommandLine::Outcome::Failed, QStringLiteral("The running NSL Snap closed the connection.")};
    }
    const QJsonObject answer = QJsonDocument::fromJson(socket.readLine()).object();
    return Answer{outcomeFromName(answer.value(kOutcome).toString()), answer.value(kDetail).toString()};
}

bool SingleInstance::listen(const QString &name)
{
    m_server = new QLocalServer(this);
    // Only this user may connect (file mode 0600, or the pipe's ACL).
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(name)) {
        // Left over from an app that crashed: forward() found nobody there.
        QLocalServer::removeServer(name);
        if (!m_server->listen(name))
            return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { onRequest(socket); });
    }
}

void SingleInstance::onRequest(QLocalSocket *socket)
{
    if (!socket->canReadLine())
        return; // the rest of the line is still on its way
    const QJsonObject request = QJsonDocument::fromJson(socket->readLine()).object();
    QStringList arguments;
    for (const QJsonValue &value : request.value(kArguments).toArray())
        arguments << value.toString();
    // The caller may give up (Ctrl+C) before the user has picked.
    const QPointer<QLocalSocket> guard(socket);
    const Reply reply = [guard](const Answer &answer) {
        if (!guard)
            return;
        guard->write(line({{kOutcome, outcomeName(answer.outcome)}, {kDetail, answer.detail}}));
        guard->flush();
        guard->disconnectFromServer();
    };
    if (m_handler)
        m_handler(arguments, reply);
    else
        reply({CommandLine::Outcome::Failed, QStringLiteral("NSL Snap is not ready yet.")});
}
