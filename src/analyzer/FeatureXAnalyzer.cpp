#include "FeatureXAnalyzer.h"
#include "ProjectIndex.h"
#include "../app/AppSettings.h"
#include "../model/Project.h"

FeatureXAnalyzer::FeatureXAnalyzer(AppSettings* settings, ProjectIndex* index)
    : m_settings(settings)
    , m_index(index)
{}

QList<Diagnostic> FeatureXAnalyzer::analyzeProject(Project* project)
{
    QList<Diagnostic> diags;
    if (!project || !m_index) return diags;

    const QString& root = project->rootPath();

    // --- Unique Data names ---
    // Data names must always be unique across the project
    for (auto it = m_index->dataNames().constBegin();
         it != m_index->dataNames().constEnd(); ++it)
    {
        if (m_index->dataNames().count(it.key()) > 1) {
            Diagnostic d;
            d.filePath = it.value();
            d.message  = QStringLiteral("Duplicate Data name '%1'").arg(it.key());
            d.severity = Diagnostic::Severity::Error;
            diags.append(d);
        }
    }

    // --- Unique Scenario names (optional) ---
    if (m_settings && m_settings->uniqueScenarioNames(root)) {
        for (auto it = m_index->scenarioNames().constBegin();
             it != m_index->scenarioNames().constEnd(); ++it)
        {
            if (m_index->scenarioNames().count(it.key()) > 1) {
                Diagnostic d;
                d.filePath = it.value();
                d.message  = QStringLiteral("Duplicate Scenario name '%1'").arg(it.key());
                d.severity = Diagnostic::Severity::Error;
                diags.append(d);
            }
        }
    }

    // --- Unique Step names (optional) ---
    if (m_settings && m_settings->uniqueStepNames(root)) {
        for (auto it = m_index->stepNames().constBegin();
             it != m_index->stepNames().constEnd(); ++it)
        {
            if (m_index->stepNames().count(it.key()) > 1) {
                Diagnostic d;
                d.filePath = it.value();
                d.message  = QStringLiteral("Duplicate Step name '%1'").arg(it.key());
                d.severity = Diagnostic::Severity::Warning;
                diags.append(d);
            }
        }
    }

    return diags;
}
