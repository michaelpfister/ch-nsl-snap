#pragma once

#include "update/Releases.h"

#include <QDateTime>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

// Asks GitHub for the published releases (D-037). One request per check,
// without any data about the user or their captures.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    // Starts a check; ignored while one is running. Ends with finished()
    // or failed().
    void check();
    bool isRunning() const { return m_reply != nullptr; }

    // The automatic check at start runs at most once a day.
    static bool isDue(const QDateTime &lastCheck, const QDateTime &now);

signals:
    // All releases, newest first.
    void finished(const QList<Releases::Release> &releases);
    // A German message for the user.
    void failed(const QString &message);

private:
    void onFinished();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
};
