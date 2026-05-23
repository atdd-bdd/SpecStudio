#include "Project.h"
#include "ProjectFile.h"
#include "../git/GitClient.h"

#include <QDir>
#include <QDirIterator>

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

void Project::scanFiles()
{
    qDeleteAll(m_files);
    m_files.clear();

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

        m_files.append(new ProjectFile(abs, rel));
    }
}
