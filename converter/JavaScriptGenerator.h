#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class JavaScriptGenerator
{
public:
    struct Options {
        QString     outputDir;
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QStringList extraImports;              // extra import lines injected into generated files
        QString     tagFilter;                 // boolean tag expression; empty = generate all
        bool        createProductionClasses = false;
        QString     productionClassesDir;
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

private:
    QStringList m_extraImports;
    QString     m_tagFilter;

    // Type helpers
    static QString jsDefaultValue(const QString& specType);
    static QString parseExpr(const QString& field, const QString& specType);

    // Identifier helpers
    static QString toCamelCase(const QString& name);         // "Transfer Amount" → "transferAmount"
    static QString toPascalCase(const QString& name);        // "Transfer Amount" → "TransferAmount"
    static QString toMethodName(const QString& keyword, const QString& stepText);  // camelCase method
    static QString toFileName(const QString& name);          // "MySpec" → "mySpec"

    // Collection helpers
    static bool    isCollectionType(const QString& name, const SpectableFile& file);
    static QString collectionElementType(const QString& name, const SpectableFile& file);

    // DataType detection
    static bool isDataType(const QString& name, const SpectableFile& file);

    // Lookup
    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    // Row resolution
    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);
    static QVector<QStringList> resolveExamplesRows(
        const NamedBlock& nb, const AttrSet* as);

    // File generators
    QString genStringClass(const AttrSet& as) const;
    QString genTypedClass(const AttrSet& as) const;
    QString genCommonIndex(const QVector<AttrSet>& attrSets) const;
    QString genTestFile(const SpectableFile& file, const QString& specName,
                        const QString& glueClass, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& specName) const;

    // Production
    static QString genProductionEntity(const AttrSet& as);
    static QString genProductionCollection(const Collection& col);

    // Glue stub helpers
    struct GlueSig {
        QString method;
        QString paramType;  // "" = void, "docstring", "grid", or "{AttrSetName}String"
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubMethod(const GlueSig& sig);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
