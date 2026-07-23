#include "SolutionSerializer.h"
#include "Solution.h"
#include "Project.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

bool SolutionSerializer::save(Solution* solution, QString& errorOut)
{
    QJsonObject root;
    root["version"] = 2;
    root["name"]    = solution->name();
    root["sharingMode"] = (solution->sharingMode() == Solution::SharingMode::GitHub)
                        ? "github" : "sharedFiles";
    if (solution->sharingMode() == Solution::SharingMode::GitHub)
        root["gitHubHost"] = solution->gitHubHost();

    QJsonArray projects;
    for (const auto* proj : solution->projects()) {
        QJsonObject p;
        p["name"]         = proj->name();
        p["relativePath"] = QDir(solution->rootPath()).relativeFilePath(proj->rootPath());
        projects.append(p);
    }
    root["projects"] = projects;

    QString filePath = solution->rootPath() + QDir::separator()
                       + solution->name() + kExtension;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorOut = QStringLiteral("Cannot write to '%1': %2").arg(filePath, file.errorString());
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// Best-effort backward-compat inference for pre-version-2 .sspec files, which
// have no "sharingMode" key: a solution that already has a git repo with an
// "origin" remote was, in practice, already being used as a shared GitHub
// repo (from the earlier per-solution-repo work), so treat it as GitHub mode
// rather than forcing every existing user to re-run "Share with Git".
static bool hasOriginRemote(const QString& rootPath)
{
    if (!QDir(rootPath + "/.git").exists())
        return false;

    QProcess proc;
    proc.setWorkingDirectory(rootPath);
    proc.start("git", {"config", "--get", "remote.origin.url"});
    if (!proc.waitForFinished(5000))
        return false;
    return proc.exitCode() == 0 && !proc.readAllStandardOutput().trimmed().isEmpty();
}

Solution* SolutionSerializer::load(const QString& sspecPath, QString& errorOut)
{
    QFile file(sspecPath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorOut = QStringLiteral("Cannot open '%1': %2").arg(sspecPath, file.errorString());
        return nullptr;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (doc.isNull()) {
        errorOut = QStringLiteral("Parse error in '%1': %2").arg(sspecPath, parseErr.errorString());
        return nullptr;
    }

    QJsonObject root = doc.object();
    QString solutionName = root["name"].toString();
    QString rootPath     = QFileInfo(sspecPath).absolutePath();

    auto* solution = new Solution(solutionName, rootPath);

    if (root.contains("sharingMode")) {
        const QString modeStr = root["sharingMode"].toString();
        if (modeStr == "github") {
            solution->setSharingMode(Solution::SharingMode::GitHub);
            solution->setGitHubHost(root.value("gitHubHost").toString(QStringLiteral("github.com")));
        } else {
            solution->setSharingMode(Solution::SharingMode::SharedFiles);
        }
    } else {
        // Pre-version-2 file: infer from repo state instead of defaulting blindly.
        solution->setSharingMode(hasOriginRemote(rootPath) ? Solution::SharingMode::GitHub
                                                            : Solution::SharingMode::SharedFiles);
    }

    for (const QJsonValue& val : root["projects"].toArray()) {
        QJsonObject p = val.toObject();
        QString name    = p["name"].toString();
        QString relPath = p["relativePath"].toString();
        QString absPath = QDir(rootPath).absoluteFilePath(relPath);

        auto* project = new Project(name, absPath);
        project->scanFiles();
        solution->addProject(project);
    }

    return solution;
}
