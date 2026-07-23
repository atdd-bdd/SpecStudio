#pragma once

#include <QObject>
#include <QList>

class Project;
class GitClient;

class Solution : public QObject
{
    Q_OBJECT

public:
    // SharedFiles: no git calls at all, ever. GitHub: SpecStudio manages a single
    // repo (rooted at the solution folder) end-to-end, including remote creation.
    enum class SharingMode { SharedFiles, GitHub };

    explicit Solution(const QString& name, const QString& rootPath, QObject* parent = nullptr);
    ~Solution() override;

    QString name()     const { return m_name; }
    QString rootPath() const { return m_rootPath; }

    const QList<Project*>& projects() const { return m_projects; }

    void    addProject(Project* project);
    void    removeProject(Project* project);
    Project* projectForFile(const QString& absoluteFilePath) const;

    // Every project in the solution shares this one repo, rooted at the solution folder.
    GitClient* git() const { return m_git; }

    SharingMode sharingMode() const { return m_sharingMode; }
    void        setSharingMode(SharingMode mode) { m_sharingMode = mode; }

    // Only meaningful when sharingMode() == GitHub. Defaults to public github.com;
    // set to a corporate host for GitHub Enterprise Server.
    QString gitHubHost() const { return m_gitHubHost; }
    void    setGitHubHost(const QString& host) { m_gitHubHost = host; }

private:
    QString        m_name;
    QString        m_rootPath;
    QList<Project*> m_projects;
    GitClient*     m_git       = nullptr;
    SharingMode    m_sharingMode = SharingMode::SharedFiles;
    QString        m_gitHubHost  = QStringLiteral("github.com");
};
