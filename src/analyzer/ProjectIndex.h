#pragma once

#include <QMultiMap>
#include <QStringList>
#include "../app/AppSettings.h"

class Project;

class ProjectIndex
{
public:
    void rebuild(Project* project);

    // Returns step text suggestions filtered by scope
    QStringList stepCompletions(const QString& prefix,
                                StepScope      scope,
                                const QString& currentFile,
                                const QString& currentFolder) const;

    const QMultiMap<QString, QString>& dataNames()     const { return m_dataNames; }
    const QMultiMap<QString, QString>& scenarioNames() const { return m_scenarioNames; }
    const QMultiMap<QString, QString>& stepNames()     const { return m_stepNames; }

private:
    void parseFile(const QString& absolutePath);

    QMultiMap<QString, QString> m_dataNames;      // name → filePath
    QMultiMap<QString, QString> m_scenarioNames;  // name → filePath
    QMultiMap<QString, QString> m_stepNames;      // text → filePath
};
