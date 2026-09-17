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
    // ── Names across files, read from the index ──────────────────────────
    void checkDomainTermDuplicates        (const QString& filePath, QList<Diagnostic>& out) const;
    void checkDomainTermVsDataTypeNames   (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;

    // A Collection's element type has to be an Entity: a production class is
    // written for an Entity and not for an Attributes block, so a Collection of
    // the latter generates code referring to a type nothing writes.
    void checkCollectionElementTypes      (const QString& filePath, const SpecTableSymbols& visible, QList<Diagnostic>& out) const;

    // A name declared in two files: the generator tests the first and warns
    // about the rest, so the second declaration is silently not tested.
    void checkDuplicateDeclarations       (const QString& filePath, QList<Diagnostic>& out) const;

    // ── One file's contents, read from the converter's parse tree ────────
    // See SpecTableModelChecks.cpp. The file arrives with its siblings merged
    // in as context, exactly as the converter sees it before generating.
    void runModelChecks     (const QString& filePath, QList<Diagnostic>& out) const;
    void checkParseMessages (const QString& filePath, const struct SpectableFile& file,
                             QList<Diagnostic>& out) const;
    // A step's ": Name" must be an attribute set, DataType or Collection; a
    // step applying a BusinessRule or Calculation must name one that exists.
    void checkStepRefs      (const QString& filePath, const struct SpectableFile& file,
                             const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    // A BusinessRule, Calculation or DataType should say what it is.
    void checkDescriptions  (const QString& filePath, const struct SpectableFile& file,
                             QList<Diagnostic>& out) const;
    // A BusinessRule or Calculation needs an Examples: table, a DataType needs
    // a table of some kind, and the set an Examples: names must exist.
    void checkExamples      (const QString& filePath, const struct SpectableFile& file,
                             const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    // Every "=Name" must name a Define.
    void checkDefineRefs    (const QString& filePath, const struct SpectableFile& file,
                             const SpecTableSymbols& visible, QList<Diagnostic>& out) const;
    // Reads the parse tree, not the file — see SpecTableModelChecks.cpp.
    // Asks the index, because the two declarations are usually in different files.
    void checkNameDeclaredAsTwoKinds(const QString& filePath, const SpecTableSymbols& visible,
                                     QList<Diagnostic>& out) const;
    // Reads the parse tree, not the file — see SpecTableModelChecks.cpp.
    void checkDomainTermColumnTypes(const QString& filePath, const struct SpectableFile& file,
                                    const QMap<QString, QString>& dtTypes,
                                    QList<Diagnostic>& out) const;
    void checkAttributeFieldTypes(const QString& filePath, const struct SpectableFile& file,
                                 const SpecTableSymbols& visible,
                                 QList<Diagnostic>& out) const;
    void checkEmptyScenarios(const QString& filePath, const struct SpectableFile& file,
                             QList<Diagnostic>& out) const;
    void checkDuplicateFieldNames    (const QString& filePath, const struct SpectableFile& file,
                                      QList<Diagnostic>& out) const;
    void checkDuplicateScenarioNames (const QString& filePath, const struct SpectableFile& file,
                                      QList<Diagnostic>& out) const;
    void checkEmptyAttrSets          (const QString& filePath, const struct SpectableFile& file,
                                      QList<Diagnostic>& out) const;

    static Diagnostic makeDiag(const QString& filePath, int line,
                                const QString& msg,
                                Diagnostic::Severity sev = Diagnostic::Severity::Error);

    SpecTableIndex* m_index;
};
