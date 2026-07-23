#include "GitInstaller.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>

namespace {

bool downloadToFile(QWidget* parent, const QUrl& url, const QString& destPath, QString& errorOut)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "SpecStudio");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QProgressDialog progress(QObject::tr("Downloading Git installer..."), QObject::tr("Cancel"), 0, 0, parent);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &progress,
        [&progress](qint64 received, qint64 total) {
            if (total > 0) { progress.setMaximum(int(total)); progress.setValue(int(received)); }
        });
    progress.show();
    loop.exec();

    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) errorOut = reply->errorString();
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    if (!ok) return false;

    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorOut = out.errorString();
        return false;
    }
    out.write(data);
    return true;
}

bool fetchJson(const QUrl& url, QJsonDocument& docOut, QString& errorOut)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "SpecStudio");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) errorOut = reply->errorString();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if (!ok) return false;

    QJsonParseError parseErr;
    docOut = QJsonDocument::fromJson(body, &parseErr);
    if (docOut.isNull()) {
        errorOut = parseErr.errorString();
        return false;
    }
    return true;
}

} // namespace

bool GitInstaller::isGitInstalled()
{
    QProcess proc;
    proc.start("git", {"--version"});
    if (!proc.waitForFinished(3000))
        return false;
    return proc.exitCode() == 0;
}

bool GitInstaller::ensureGitInstalled(QWidget* parent, const QString& contextLabel)
{
    if (isGitInstalled())
        return true;

    const auto reply = QMessageBox::question(parent, QObject::tr("Git Not Found"),
        QObject::tr("Git is required to %1. Would you like to download and install it now?")
            .arg(contextLabel),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes)
        return false;

    QString error;
    QJsonDocument releaseDoc;
    if (!fetchJson(QUrl("https://api.github.com/repos/git-for-windows/git/releases/latest"),
                   releaseDoc, error)) {
        QMessageBox::critical(parent, QObject::tr("Download Failed"),
            QObject::tr("Could not check for the latest Git installer: %1").arg(error));
        return false;
    }

    QString downloadUrl;
    const QJsonArray assets = releaseDoc.object().value("assets").toArray();
    static const QRegularExpression namePattern(R"(^Git-.*-64-bit\.exe$)");
    for (const QJsonValue& v : assets) {
        const QJsonObject asset = v.toObject();
        const QString name = asset.value("name").toString();
        if (namePattern.match(name).hasMatch()) {
            downloadUrl = asset.value("browser_download_url").toString();
            break;
        }
    }
    if (downloadUrl.isEmpty()) {
        QMessageBox::critical(parent, QObject::tr("Download Failed"),
            QObject::tr("Could not find a Git for Windows installer in the latest release."));
        return false;
    }

    const QString installerPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/SpecStudio-GitInstaller.exe";
    if (!downloadToFile(parent, QUrl(downloadUrl), installerPath, error)) {
        QMessageBox::critical(parent, QObject::tr("Download Failed"),
            QObject::tr("Could not download the Git installer: %1").arg(error));
        return false;
    }

    auto* installProc = new QProcess();
    installProc->start(installerPath, {});
    if (!installProc->waitForStarted(10000)) {
        QMessageBox::critical(parent, QObject::tr("Install Failed"),
            QObject::tr("Could not launch the Git installer."));
        installProc->deleteLater();
        return false;
    }

    // Block this flow (without freezing SpecStudio's own UI) until the
    // installer — running with its normal, non-silent GUI — exits.
    QEventLoop loop;
    QObject::connect(installProc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                      &loop, &QEventLoop::quit);
    loop.exec();
    installProc->deleteLater();

    if (!isGitInstalled()) {
        QMessageBox::critical(parent, QObject::tr("Cannot Continue"),
            QObject::tr("Cannot %1 due to git still not being installed.").arg(contextLabel));
        return false;
    }
    return true;
}
