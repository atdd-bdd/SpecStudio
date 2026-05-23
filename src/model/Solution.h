#pragma once

#include <QObject>
#include <QList>

class Project;

class Solution : public QObject
{
    Q_OBJECT

public:
    explicit Solution(const QString& name, const QString& rootPath, QObject* parent = nullptr);
    ~Solution() override;

    QString name()     const { return m_name; }
    QString rootPath() const { return m_rootPath; }

    const QList<Project*>& projects() const { return m_projects; }

    void    addProject(Project* project);
    void    removeProject(Project* project);
    Project* projectForFile(const QString& absoluteFilePath) const;

private:
    QString        m_name;
    QString        m_rootPath;
    QList<Project*> m_projects;
};
