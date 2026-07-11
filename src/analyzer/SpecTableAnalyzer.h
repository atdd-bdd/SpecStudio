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
    void checkImports               (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkInserts               (const QString& filePath, QList<Diagnostic>& out) const;
    void checkStepRefs              (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkDescriptions          (const QString& filePath, QList<Diagnostic>& out) const;
    void checkExamples              (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkDefineRefs            (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkCleanup               (const QString& filePath, QList<Diagnostic>& out) const;
    void checkTableColumnConsistency (const QString& filePath, QList<Diagnostic>& out) const;
    void checkStepTableContents      (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkDomainTermDuplicates        (const QString& filePath, QList<Diagnostic>& out) const;
    void checkUnrecognizedLines           (const QString& filePath, QList<Diagnostic>& out) const;
    void checkStepsWithTableButNoAttrSet  (const QString& filePath, QList<Diagnostic>& out) const;

    static Diagnostic makeDiag(const QString& filePath, int line,
                                const QString& msg,
                                Diagnostic::Severity sev = Diagnostic::Severity::Error);
    static void validateDataTypeValue(const QString& filePath, int lineNo,
                                      const QString& value, const QString& dtype,
                                      QList<Diagnostic>& out);

    SpecTableIndex* m_index;
};
