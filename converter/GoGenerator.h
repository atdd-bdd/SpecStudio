#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class GoGenerator {
public:
    struct Options {
        QString     outputDir;
        QString     sourceRoot;     // project root; used to mirror the .spectable's subfolder in output
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QStringList extraImports;
        QString     tagFilter;
        bool        createProductionClasses  = false;
        QString     productionClassesDir;
        QString     productionClassesPackage; // Go package name (default: "domain")
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

private:
    QStringList m_extraImports;
    QString     m_tagFilter;
    QString     m_modulePath;   // go.mod module name; prefixes the common import

    static QString goType(const QString& specType);
    static bool    isAttrSetType(const QString& name, const SpectableFile& file);
    static QString goCommonType(const Field& f, const SpectableFile& file);
    static QString goNestedLiteral(const QString& cellValue, const QString& fieldType,
                                   const SpectableFile& file, const QString& prefix);
    static QString goStringLiteral(const AttrSet& as, const QStringList& row,
                                   const SpectableFile& file, const QString& prefix);
    static QString toIdentifier(const QString& name);   // snake_case
    static QString toExported(const QString& name);     // PascalCase
    static QString toMethodName(const QString& keyword, const QString& stepText); // PascalCase
    static QString toPackageName(const QString& name);  // lowercase no-separator
    static QString goParamName(const QString& fieldName); // avoids Go keywords

    static bool    isDataType(const QString& name, const SpectableFile& file);
    static bool    isCollectionType(const QString& name, const SpectableFile& file);
    static QString collectionElementType(const QString& name, const SpectableFile& file);

    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    static QVector<QStringList> resolveExamplesRows(
        const NamedBlock& nb, const AttrSet* as);

    QString genStringStruct(const AttrSet& as, const QString& pkg,
                            const SpectableFile& file) const;
    QString genTypedStruct(const AttrSet& as, const QString& pkg,
                           const SpectableFile& file) const;
    QString genCommonGo(const QString& pkg) const;
    QString genTestFile(const SpectableFile& file, const QString& specPkg,
                        const QString& glueType, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& specPkg,
                        const QString& glueType) const;

    struct GlueSig {
        QString method;
        QString paramType; // "" = void, "docstring", "grid", or "{Name}String"
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubFn(const GlueSig& sig, const QString& glueType);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   const QString& glueType,
                                   QStringList& msgs);

    QString genProductionEntity(const AttrSet& as, const QString& pkg) const;
    QString genProductionCollection(const Collection& col, const QString& pkg) const;

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
