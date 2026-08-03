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

#ifdef Q_OS_WIN

bool downloadToFile(QWidget* parent, const QUrl& url, const QString& destPath, QString& errorOut)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "AlignThree");
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
    req.setHeader(QNetworkRequest::UserAgentHeader, "AlignThree");
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

// Download the current Git for Windows release and run its normal installer UI.
bool installGit(QWidget* parent, const QString& contextLabel)
{
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
        + "/AlignThree-GitInstaller.exe";
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
    return GitInstaller::isGitInstalled();
}

#elif defined(Q_OS_MACOS)

// git arrives with the Xcode Command Line Tools. `xcode-select --install` puts
// up Apple's own download dialog and returns immediately, so there is nothing to
// wait on -- the user comes back once it has finished.
//
// Note that merely *checking* for git can trigger the same Apple dialog: on a
// machine without the tools, /usr/bin/git is a stub that prompts. So by the time
// this runs the user may already have seen it.
bool installGit(QWidget* parent, const QString& contextLabel)
{
    const auto reply = QMessageBox::question(parent, QObject::tr("Git Not Found"),
        QObject::tr("Git is required to %1.\n\n"
                    "On macOS git comes with the Xcode Command Line Tools. "
                    "Would you like to start that installation now?").arg(contextLabel),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes)
        return false;

    QProcess::startDetached("xcode-select", {"--install"});
    QMessageBox::information(parent, QObject::tr("Finish in the Installer"),
        QObject::tr("Apple's installer is starting in a separate window.\n\n"
                    "When it has finished, try again — AlignThree does not "
                    "control that installation and cannot tell when it is done."));
    return false;   // not installed *yet*; the caller must not proceed
}

#else

// Linux: never download a binary. git belongs to the distribution's package
// manager, installing it needs root, and a GUI application quietly invoking
// sudo is not something to do. Name the package manager if it can be identified
// and show the exact command, which the user can run themselves.
bool installGit(QWidget* parent, const QString& contextLabel)
{
    struct Manager { const char* probe; const char* command; };
    static const Manager managers[] = {
        { "apt-get", "sudo apt install git" },
        { "dnf",     "sudo dnf install git" },
        { "pacman",  "sudo pacman -S git"   },
        { "zypper",  "sudo zypper install git" },
        { "apk",     "sudo apk add git"     },
    };

    QString command;
    for (const Manager& m : managers) {
        QProcess which;
        which.start("which", { QString::fromLatin1(m.probe) });
        which.waitForFinished(2000);
        if (which.exitCode() == 0) { command = QString::fromLatin1(m.command); break; }
    }
    if (command.isEmpty())
        command = QObject::tr("(install git using your distribution's package manager)");

    QMessageBox::information(parent, QObject::tr("Git Not Found"),
        QObject::tr("Git is required to %1, and is not installed.\n\n"
                    "Install it with:\n\n    %2\n\n"
                    "then try again.").arg(contextLabel, command));
    return false;
}

#endif

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

    // How git is obtained differs completely per platform, so installGit() above
    // is compiled per platform rather than branched at run time. Windows is the
    // only one SpecStudio installs itself: there is a single official installer,
    // and it needs no elevation the user has not already granted. macOS defers to
    // Apple's Command Line Tools installer, and Linux to the distribution's
    // package manager, because on both the alternative is downloading a binary
    // from the internet and running it with root, which is not this program's
    // business.
    //
    // This used to be unconditional: every platform fetched the Git for Windows
    // release and tried to execute a .exe.
    return installGit(parent, contextLabel);
}
