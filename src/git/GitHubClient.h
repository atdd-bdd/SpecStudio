#pragma once

#include <QObject>
#include <QString>

// Wraps the GitHub REST API (or a GitHub Enterprise Server instance's
// equivalent) for the one operation SpecStudio needs to fully automate:
// creating a new private repository to back a solution.
class GitHubClient : public QObject
{
    Q_OBJECT

public:
    struct CreateRepoResult
    {
        bool    ok = false;
        QString cloneUrl;       // e.g. https://github.com/owner/repo.git
        QString htmlUrl;
        QString errorMessage;   // populated when !ok
    };

    // host: "github.com" for public GitHub, or a corporate GitHub Enterprise
    // Server hostname. personalAccessToken: used as a Bearer token.
    GitHubClient(const QString& host, const QString& personalAccessToken, QObject* parent = nullptr);

    // Creates a new private repository under the PAT owner's personal account.
    // Blocks the calling flow (via an internal QEventLoop) until the request
    // completes, so callers can use it like a synchronous call.
    CreateRepoResult createPrivateRepo(const QString& repoName);

private:
    static QString apiBaseUrl(const QString& host);

    QString m_host;
    QString m_token;
};
