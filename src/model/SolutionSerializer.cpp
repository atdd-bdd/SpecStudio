#include "SolutionSerializer.h"
#include "Solution.h"
#include "Project.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool SolutionSerializer::save(Solution* solution, QString& errorOut)
{
    QJsonObject root;
    root["version"] = 1;
    root["name"]    = solution->name();
    root["repoScope"] = (solution->repoScope() == Solution::RepoScope::Combined)
                       ? "combined" : "separate";

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
    solution->setRepoScope(root["repoScope"].toString() == "combined"
                           ? Solution::RepoScope::Combined : Solution::RepoScope::Separate);

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
