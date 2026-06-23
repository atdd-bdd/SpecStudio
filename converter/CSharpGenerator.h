#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class CSharpGenerator
{
public:
    struct Options {
        QString nsPrefix     = "gherkinexecutor";  // namespace prefix
        QString outputDir;                          // destination directory
        bool    overwriteGlue = false;              // force-overwrite the glue file
    };

    // Generate all output files; returns list of "SEVERITY:LINE:message" strings
    QStringList generate(const SpectableFile& file, const Options& opts);

private:
    // Type helpers
    static QString csharpType(const QString& specType);
    static QString parseExpr(const QString& field, const QString& specType);

    // Identifier helpers
    static QString toClassName(const QString& name);
    static QString toMethodName(const QString& keyword, const QString& stepText);
    static QString paramName(const QString& fieldName);

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
    QString genStringClass(const AttrSet& as, const QString& ns) const;
    QString genTypedClass(const AttrSet& as, const QString& ns) const;
    QString genTestFile(const SpectableFile& file, const QString& ns,
                        const QString& className, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& ns,
                        const QString& className) const;

    struct GlueSig { QString method; QString paramType; bool isList; };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubMethod(const GlueSig& sig);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs);

    // Helper: write a file and add a message on failure
    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
