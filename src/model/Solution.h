#pragma once

#include <QObject>
#include <QList>

class Project;
class GitClient;

class Solution : public QObject
{
    Q_OBJECT

public:
    // Separate: every Project owns its own git repo (the default, and the
    // only mode that existed before this). Combined: every Project in the
    // solution shares one repo rooted at the solution folder.
    enum class RepoScope { Separate, Combined };

    explicit Solution(const QString& name, const QString& rootPath, QObject* parent = nullptr);
    ~Solution() override;

    QString name()     const { return m_name; }
    QString rootPath() const { return m_rootPath; }

    const QList<Project*>& projects() const { return m_projects; }

    void    addProject(Project* project);
    void    removeProject(Project* project);
    Project* projectForFile(const QString& absoluteFilePath) const;

    RepoScope  repoScope() const { return m_repoScope; }
    void       setRepoScope(RepoScope scope) { m_repoScope = scope; }
    // Only meaningful when repoScope() == Combined; every project shares this instance.
    GitClient* git() const { return m_git; }

private:
    QString        m_name;
    QString        m_rootPath;
    QList<Project*> m_projects;
    RepoScope      m_repoScope = RepoScope::Separate;
    GitClient*     m_git       = nullptr;
};
