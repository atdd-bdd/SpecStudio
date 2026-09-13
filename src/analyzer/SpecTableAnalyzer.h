#pragma once

#include "AnalysisResult.h"
#include "SpecTableIndex.h"

#include <QList>
#include <QMap>
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
    void checkDomainTermColumnTypes       (const QString& filePath, const QMap<QString, QString>& dtTypes, QList<Diagnostic>& out) const;
    void checkDomainTermVsDataTypeNames   (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    void checkUnrecognizedLines           (const QString& filePath, QList<Diagnostic>& out) const;
    void checkStepsWithTableButNoAttrSet  (const QString& filePath, QList<Diagnostic>& out) const;
    void checkAttributeFieldTypes         (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;

    // A Collection's element type has to be an Entity: a production class is
    // written for an Entity and not for an Attributes block, so a Collection of
    // the latter generates code referring to a type nothing writes.
    void checkCollectionElementTypes      (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;

    // A name declared in two files: the generator tests the first and warns
    // about the rest, so the second declaration is silently not tested.
    void checkDuplicateDeclarations       (const QString& filePath, QList<Diagnostic>& out) const;

    // An Examples: table's columns against the fields its AttributeSet declares,
    // and each cell against its field's type.
    void checkExamplesTableContents       (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;

    // The Default column of an Attributes/Entity block against the type declared
    // beside it on the same row.
    void checkAttributeDefaultValues      (const QString& filePath, QList<Diagnostic>& out) const;

    // The fields an AttributeSet declares, as name -> type. Empty for a built-in
    // set such as ValidValues, which has no declaration to read.
    QMap<QString, QString> fieldTypesOf   (const QString& attrSetName) const;

    // PROTOTYPE — checks that read the converter's own parse tree rather than
    // re-reading the file with regular expressions. See SpecTableModelChecks.cpp.
    void runModelChecks     (const QString& filePath, QList<Diagnostic>& out) const;
    void checkParseMessages (const QString& filePath, const struct SpectableFile& file,
                             QList<Diagnostic>& out) const;
    void checkEmptyScenarios(const QString& filePath, const struct SpectableFile& file,
                             QList<Diagnostic>& out) const;

    static Diagnostic makeDiag(const QString& filePath, int line,
                                const QString& msg,
                                Diagnostic::Severity sev = Diagnostic::Severity::Error);
    static void validateDataTypeValue(const QString& filePath, int lineNo,
                                      const QString& value, const QString& dtype,
                                      QList<Diagnostic>& out);

    SpecTableIndex* m_index;
};
