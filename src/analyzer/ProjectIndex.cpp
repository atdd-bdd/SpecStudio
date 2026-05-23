#include "ProjectIndex.h"
#include "../model/Project.h"
#include "../model/ProjectFile.h"
#include "../model/FileType.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

void ProjectIndex::rebuild(Project* project)
{
    m_dataNames.clear();
    m_scenarioNames.clear();
    m_stepNames.clear();

    for (auto* file : project->files()) {
        if (file->type() == FileType::FeatureX || file->type() == FileType::Feature)
            parseFile(file->absolutePath());
    }
}

void ProjectIndex::parseFile(const QString& absolutePath)
{
    QFile f(absolutePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&f);
    static QRegularExpression reScenario(R"(^\s*Scenario(?:\s+Outline)?:\s*(.+)$)",
                                         QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reStep(R"(^\s*(?:Given|When|Then|And|But)\s+(.+)$)",
                                     QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reData(R"(^\s*Data\s+(\w+)\b)",
                                     QRegularExpression::CaseInsensitiveOption);

    while (!in.atEnd()) {
        QString line = in.readLine();

        auto m = reScenario.match(line);
        if (m.hasMatch()) {
            m_scenarioNames.insert(m.captured(1).trimmed(), absolutePath);
            continue;
        }
        m = reStep.match(line);
        if (m.hasMatch()) {
            m_stepNames.insert(m.captured(1).trimmed(), absolutePath);
            continue;
        }
        m = reData.match(line);
        if (m.hasMatch()) {
            m_dataNames.insert(m.captured(1).trimmed(), absolutePath);
        }
    }
}

QStringList ProjectIndex::stepCompletions(const QString& prefix,
                                           StepScope      scope,
                                           const QString& currentFile,
                                           const QString& currentFolder) const
{
    QStringList results;
    for (auto it = m_stepNames.constBegin(); it != m_stepNames.constEnd(); ++it) {
        if (!it.key().startsWith(prefix, Qt::CaseInsensitive)) continue;

        switch (scope) {
        case StepScope::File:
            if (it.value() != currentFile) continue;
            break;
        case StepScope::Folder:
            if (QFileInfo(it.value()).absolutePath() != currentFolder) continue;
            break;
        case StepScope::Project:
            break;
        }

        if (!results.contains(it.key()))
            results.append(it.key());
    }
    return results;
}
