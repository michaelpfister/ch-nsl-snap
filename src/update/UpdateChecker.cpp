#include "update/UpdateChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {
constexpr int kTimeoutMs = 15000;
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::check()
{
    if (m_reply)
        return;
    QNetworkRequest request(Releases::apiUrl());
    // GitHub's API refuses requests without a User-Agent.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Snapslash/%1").arg(Version::current().toString()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::onFinished);
}

void UpdateChecker::onFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        // Without an account GitHub allows 60 requests per hour and address.
        if (status == 403 || status == 429)
            emit failed(tr("GitHub nimmt gerade keine weiteren Anfragen an. Bitte später nochmals versuchen."));
        else
            emit failed(tr("Keine Verbindung zu GitHub: %1").arg(reply->errorString()));
        return;
    }
    const std::optional<QList<Releases::Release>> releases = Releases::parse(reply->readAll());
    if (!releases) {
        emit failed(tr("Die Antwort von GitHub war nicht lesbar."));
        return;
    }
    emit finished(*releases);
}

bool UpdateChecker::isDue(const QDateTime &lastCheck, const QDateTime &now)
{
    // A clock set back (last check "in the future") also counts as due.
    return !lastCheck.isValid() || lastCheck > now || lastCheck.secsTo(now) >= 24 * 60 * 60;
}
