#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class PythonGenerator
{
public:
    struct Options {
        QString     packagePrefix;              // module prefix (currently unused but reserved)
        QString     outputDir;
        QString     sourceRoot;     // project root; used to mirror the .spectable's subfolder in output
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QStringList extraImports;               // injected as verbatim "import ..." lines
        QString     tagFilter;                  // boolean tag expression; empty = generate all
        bool        createProductionClasses = false;
        QString     productionClassesDir;
        QString     productionClassesPackage;   // reserved
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

private:
    QStringList m_extraImports;
    QString     m_tagFilter;

    // Type / identifier helpers
    static QString pythonType(const QString& specType);
    static QString parseExpr(const QString& field, const QString& specType);
    static QString toIdentifier(const QString& name);   // snake_case
    static QString toTypeName(const QString& name);     // PascalCase
    static QString toMethodName(const QString& keyword, const QString& stepText);
    static QString toModuleName(const QString& name);   // snake_case module

    // Lookup helpers
    static bool        isAttrSetType(const QString& name, const SpectableFile& file);
    static QString     resolveAttrCellExpr(const QString& cellValue, const QString& fieldType,
                                            const SpectableFile& file);
    static bool        isDataType(const QString& name, const SpectableFile& file);
    static bool        isCollectionType(const QString& name, const SpectableFile& file);
    static QString     collectionElementType(const QString& name, const SpectableFile& file);
    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    // Table resolution
    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    // File generators
    QString genStringClass(const AttrSet& as, const SpectableFile& file) const;
    QString genTypedClass(const AttrSet& as, const SpectableFile& file) const;
    QString genEqualityMethods(const AttrSet& as, const QString& cn,
                               bool dncAware) const;
    QString genCommonInit(const QVector<AttrSet>& attrSets, const QString& existing,
                          const QString& dir) const;
    QString genTestFile(const SpectableFile& file, const QString& specSnake,
                        const QString& glueClass, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& glueClass) const;
    static QString pyDefaultLiteral(const Field& f, const QString& pt);
    static QString genProductionEntity(const AttrSet& as);
    static QString genProductionCollection(const Collection& col);

    struct GlueSig {
        QString method;
        QString paramType;  // "" = void; "docstring"; struct name; "grid"
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubMethod(const GlueSig& sig);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
