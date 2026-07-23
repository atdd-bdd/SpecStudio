#include "GitHubClient.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

GitHubClient::GitHubClient(const QString& host, const QString& personalAccessToken, QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_token(personalAccessToken)
{}

QString GitHubClient::apiBaseUrl(const QString& host)
{
    if (host.isEmpty() || host.compare("github.com", Qt::CaseInsensitive) == 0)
        return QStringLiteral("https://api.github.com");
    // GitHub Enterprise Server exposes the REST API under /api/v3 on the
    // Enterprise host itself, rather than a separate api.<host> subdomain.
    return QStringLiteral("https://%1/api/v3").arg(host);
}

GitHubClient::CreateRepoResult GitHubClient::createPrivateRepo(const QString& repoName)
{
    CreateRepoResult result;

    QNetworkRequest req(QUrl(apiBaseUrl(m_host) + "/user/repos"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    req.setHeader(QNetworkRequest::UserAgentHeader, "SpecStudio");

    QJsonObject body;
    body["name"]    = repoName;
    body["private"] = true;

    QNetworkAccessManager nam;
    QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray responseBody = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    const QJsonObject responseObj = QJsonDocument::fromJson(responseBody).object();

    if (statusCode < 200 || statusCode >= 300) {
        result.ok = false;
        result.errorMessage = responseObj.value("message").toString();
        if (result.errorMessage.isEmpty())
            result.errorMessage = QStringLiteral("HTTP %1").arg(statusCode);
        return result;
    }

    result.ok       = true;
    result.cloneUrl = responseObj.value("clone_url").toString();
    result.htmlUrl  = responseObj.value("html_url").toString();
    return result;
}
