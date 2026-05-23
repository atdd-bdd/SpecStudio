#pragma once

#include <QObject>
#include <QList>

class ProjectFile;
class GitClient;

class Project : public QObject
{
    Q_OBJECT

public:
    explicit Project(const QString& name, const QString& rootPath, QObject* parent = nullptr);
    ~Project() override;

    QString     name()     const { return m_name; }
    QString     rootPath() const { return m_rootPath; }
    GitClient*  git()      const { return m_git; }

    const QList<ProjectFile*>& files() const { return m_files; }

    void scanFiles();

private:
    QString             m_name;
    QString             m_rootPath;
    GitClient*          m_git  = nullptr;
    QList<ProjectFile*> m_files;
};
