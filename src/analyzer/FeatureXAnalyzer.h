#pragma once

#include "AnalysisResult.h"
#include <QList>

class Project;
class AppSettings;
class ProjectIndex;

class FeatureXAnalyzer
{
public:
    FeatureXAnalyzer(AppSettings* settings, ProjectIndex* index);

    QList<Diagnostic> analyzeProject(Project* project);

private:
    AppSettings*  m_settings = nullptr;
    ProjectIndex* m_index    = nullptr;
};
