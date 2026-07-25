#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class CSharpGenerator
{
public:
    struct Options {
        QString     nsPrefix     = "gherkinexecutor";  // namespace prefix
        QString     outputDir;                          // destination directory
        QString     sourceRoot;                         // project root; used to derive subfolder from file path
        bool        overwriteGlue = false;              // force-overwrite the glue file
        bool        copySpectable = true;               // copy source .spectable to output dir
        QStringList extraImports;                       // injected after auto-usings in every generated file
        QString     tagFilter;                          // boolean tag expression; empty = generate all
        bool        createProductionClasses  = false;   // generate production class stubs
        QString     productionClassesDir;               // output folder for production classes
        QString     productionClassesNamespace;         // C# namespace for production classes
    };

    // Generate all output files; returns list of "SEVERITY:LINE:message" strings
    QStringList generate(const SpectableFile& file, const Options& opts);

    // Public so production-class helpers can call them
    static QString csharpType(const QString& specType);
    static QString toCamelCase(const QString& fieldName);

private:
    // Parse expression for Typed conversion
    static QString parseExpr(const QString& field, const QString& specType,
                             const SpectableFile* file = nullptr);

    // Identifier helpers
    static QString toClassName(const QString& name);
    static QString toMethodName(const QString& keyword, const QString& stepText);

    // Table resolution: given a step and the file context,
    // returns a list of rows (each row = ordered values matching the AttrSet fields)
    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    // Find an AttrSet by name (case-insensitive)
    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    // Find a Define by name
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    // File generators
    QString genEqualityMembers(const AttrSet& as, const QString& cn,
                               const SpectableFile& file, bool dncAware) const;
    QString genStringClass(const AttrSet& as, const QString& ns, const SpectableFile& file) const;
    QString genTypedClass(const AttrSet& as, const QString& ns, const SpectableFile& file) const;
    QString genTestFile(const SpectableFile& file, const QString& ns,
                        const QString& className, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& ns,
                        const QString& className) const;

    QStringList m_extraImports;
    QString     m_tagFilter;
    QString     m_commonNs;

    struct GlueSig { QString method; QString paramType; bool isList; };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubMethod(const GlueSig& sig);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs);

    // Helper: write a file and add a message on failure
    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
