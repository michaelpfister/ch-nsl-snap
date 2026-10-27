#pragma once

#include "app/CommandLine.h"

#include <QObject>
#include <QStringList>

#include <functional>
#include <optional>

class QLocalServer;
class QLocalSocket;

// One NSL Snap per user (D-034). The first start listens on a local
// socket (a Unix socket in XDG_RUNTIME_DIR, a named pipe on Windows) that
// only the same user may open. A later start, e.g. from a key binding,
// sends its command line there, waits until the running app has done the
// capture, prints the answer and exits; so a capture never starts a
// second tray icon or editor.
//
// Every capture asked for this way is interactive as always (D-003): the
// running app shows its overlay, and the user picks.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // How a request ended: the outcome and the saved path or an error.
    struct Answer
    {
        CommandLine::Outcome outcome = CommandLine::Outcome::Failed;
        QString detail;
    };
    using Reply = std::function<void(const Answer &answer)>;
    // Called in the running app for each request; it must call `reply`
    // exactly once, also much later (after the user has picked).
    using Handler = std::function<void(const QStringList &arguments, const Reply &reply)>;

    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    // The socket's name for this user; tests pass their own.
    static QString defaultName();

    // Sends `arguments` (without the program) to the app listening on
    // `name` and blocks until it answers. Nothing if no app is listening.
    static std::optional<Answer> forward(const QStringList &arguments, const QString &name = defaultName());

    // Starts listening; false if another app got there first.
    bool listen(const QString &name = defaultName());
    void setHandler(const Handler &handler) { m_handler = handler; }

private:
    void onNewConnection();
    void onRequest(QLocalSocket *socket);

    QLocalServer *m_server = nullptr;
    Handler m_handler;
};
