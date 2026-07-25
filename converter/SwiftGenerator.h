#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class SwiftGenerator {
public:
    struct Options {
        QString     outputDir;
        QString     sourceRoot;     // project root; used to mirror the .spectable's subfolder in output
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QStringList extraImports;   // extra "import" lines injected at top of generated files
        QString     tagFilter;
        bool        createProductionClasses = false;
        QString     productionClassesDir;
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

    // Public so production-class free functions can call them
    static QString swiftType(const QString& specType);
    static QString toIdentifier(const QString& name);  // lowerCamelCase — properties, functions
    static QString toTypeName(const QString& name);    // UpperCamelCase — struct/class/enum names

private:
    QStringList m_extraImports;
    QString     m_tagFilter;

    static QString parseExpr(const QString& field, const QString& specType);
    static QString toFnName(const QString& keyword, const QString& stepText);  // lowerCamelCase

    static bool        isDataType(const QString& name, const SpectableFile& file);
    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* as,
        const SpectableFile& file, QStringList& errors);
    static QVector<QStringList> resolveExamplesRows(
        const NamedBlock& nb, const AttrSet* as);

    static bool    isAttrSetType(const QString& name, const SpectableFile& file);
    static QString swiftCommonType(const Field& f, const SpectableFile& file);
    static QString nestedLiteral(const QString& cellValue, const QString& fieldType,
                                 const SpectableFile& file);
    static QString stringLiteral(const AttrSet& as, const QStringList& row,
                                 const SpectableFile& file);
    QString genStringStruct(const AttrSet& as, const SpectableFile& file) const;
    QString genTypedStruct(const AttrSet& as, const SpectableFile& file) const;
    QString genTestFile(const SpectableFile& file, const QString& className,
                        const QString& glueClass, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& glueClass) const;

    struct GlueSig {
        QString method;
        QString paramType;  // "" = void; struct name = [Struct]; "grid" = [[String]]
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubFn(const GlueSig& sig);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
