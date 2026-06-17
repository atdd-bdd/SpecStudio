#pragma once

#include "AnalysisResult.h"
#include "SpecTableIndex.h"

#include <QList>
#include <QString>

class SpecTableAnalyzer
{
public:
    explicit SpecTableAnalyzer(SpecTableIndex* index);

    QList<Diagnostic> analyzeFile(const QString& filePath) const;

private:
    void checkImports       (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkStepRefs      (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkDescriptions  (const QString& filePath, QList<Diagnostic>& out) const;
    void checkDataTypeTables(const QString& filePath, QList<Diagnostic>& out) const;

    static Diagnostic makeDiag(const QString& filePath, int line,
                                const QString& msg,
                                Diagnostic::Severity sev = Diagnostic::Severity::Error);

    SpecTableIndex* m_index;
};
