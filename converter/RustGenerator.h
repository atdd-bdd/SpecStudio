#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class RustGenerator {
public:
    struct Options {
        QString     cratePrefix;    // reserved — always uses "crate::" paths
        QString     outputDir;
        QString     sourceRoot;     // project root; used to mirror the .spectable's subfolder in output
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QStringList extraUses;     // extra "use" lines injected at top of generated files
        QString     tagFilter;
        bool        createProductionClasses = false;
        QString     productionClassesDir;
        bool        failEveryTest = true;            // end every generated glue stub with a failure
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

    // Public so production-class free functions can call them
    static QString rustType(const QString& specType);
    static QString toIdentifier(const QString& name);  // snake_case
    static QString toTypeName(const QString& name);    // PascalCase

private:
    QStringList m_extraUses;
    QString     m_tagFilter;
    bool        m_failEveryTest = true;

    static bool    isAttrSetType(const QString& name, const SpectableFile& file);
    static QString rustNestedLiteral(const QString& cellValue, const QString& fieldType,
                                     const SpectableFile& file);
    static QString rustStringLiteral(const AttrSet& as, const QStringList& row,
                                     const SpectableFile& file);
    static QString rustCommonType(const Field& f, const SpectableFile& file);
    static QString parseExpr(const QString& field, const QString& specType);
    static QString toFnName(const QString& keyword, const QString& stepText);  // snake_case

    static bool        isDataType(const QString& name, const SpectableFile& file);
    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static bool    isCollectionType(const QString& name, const SpectableFile& file);
    static QString collectionElementType(const QString& name, const SpectableFile& file);
    static QString effectiveAttrSetName(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* as,
        const SpectableFile& file, QStringList& errors);
    static QVector<QStringList> resolveExamplesRows(
        const NamedBlock& nb, const AttrSet* as);

    QString genStringStruct(const AttrSet& as, const SpectableFile& file) const;
    QString genTypedStruct(const AttrSet& as, const SpectableFile& file) const;
    QString genCommonMod(const QVector<AttrSet>& attrSets,
                         const QString& existing,
                         const QString& dir) const;
    QString genTestFile(const SpectableFile& file, const QString& specSnake,
                        const QString& glueStruct, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& glueStruct) const;

    struct GlueSig {
        QString method;
        QString paramType;  // "" = void; struct name = &[Struct]; "grid" = &[Vec<String>]
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubFn(const GlueSig& sig,
                                   bool failEveryTest);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs,
                                   bool failEveryTest);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
