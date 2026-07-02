#include "Project.h"
#include "ProjectFile.h"
#include "../git/GitClient.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

Project::Project(const QString& name, const QString& rootPath, QObject* parent)
    : QObject(parent)
    , m_name(name)
    , m_rootPath(rootPath)
    , m_git(new GitClient(rootPath, this))
{}

Project::~Project()
{
    qDeleteAll(m_files);
}

void Project::setRootPath(const QString& path)
{
    m_rootPath = path;
    m_git->setRepoPath(path);
}

void Project::scanFiles()
{
    qDeleteAll(m_files);
    m_files.clear();

    // Collect output directories defined in any *.specconfig at the project root.
    // Files inside these directories are generated output — skip them.
    QStringList excludedPrefixes;
    {
        QDir rootDir(m_rootPath);
        const QStringList configs = rootDir.entryList({ "*.specconfig" }, QDir::Files);
        for (const QString& cfg : configs) {
            QFile f(rootDir.absoluteFilePath(cfg));
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
            QString outDir = obj["outputDirectory"].toString().trimmed();
            if (outDir.isEmpty()) continue;
            if (!QFileInfo(outDir).isAbsolute())
                outDir = rootDir.absoluteFilePath(outDir);
            outDir = QDir::cleanPath(outDir);
            if (!outDir.endsWith('/')) outDir += '/';
            excludedPrefixes << outDir;
        }
    }

    QDirIterator it(m_rootPath,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

    QDir root(m_rootPath);

    while (it.hasNext()) {
        QString abs = it.next();
        QFileInfo fi(abs);

        // Skip directories and hidden files; skip .git contents
        if (fi.isDir()) continue;
        if (fi.fileName().startsWith('.')) continue;

        QString rel = root.relativeFilePath(abs);

        // Skip anything inside a .git directory
        if (rel.startsWith(".git/") || rel.contains("/.git/")) continue;

        // Skip files inside the configured output directory
        const QString absClean = QDir::cleanPath(abs) + '/';
        bool excluded = false;
        for (const QString& prefix : excludedPrefixes) {
            if (absClean.startsWith(prefix, Qt::CaseInsensitive)) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;

        m_files.append(new ProjectFile(abs, rel));
    }
}
