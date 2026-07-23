#include "Solution.h"
#include "Project.h"
#include "../git/GitClient.h"

Solution::Solution(const QString& name, const QString& rootPath, QObject* parent)
    : QObject(parent)
    , m_name(name)
    , m_rootPath(rootPath)
    , m_git(new GitClient(rootPath, this))
{}

Solution::~Solution()
{
    qDeleteAll(m_projects);
}

void Solution::addProject(Project* project)
{
    project->setParent(this);
    m_projects.append(project);
}

void Solution::removeProject(Project* project)
{
    m_projects.removeOne(project);
    project->setParent(nullptr);
}

Project* Solution::projectForFile(const QString& absoluteFilePath) const
{
    for (auto* proj : m_projects)
        if (absoluteFilePath.startsWith(proj->rootPath()))
            return proj;
    return nullptr;
}
