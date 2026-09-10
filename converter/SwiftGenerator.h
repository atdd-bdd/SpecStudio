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
        bool        failEveryTest = true;            // end every generated glue stub with a failure
        // Append the step's AttributeSet/Entity to its glue method name, so
        // that two steps reading alike but taking different tables become two
        // methods instead of one collision. Off unless a .specconfig asks for
        // it: turning it on renames every affected glue method, and
        // appendMissingStubs matches by name, so the old ones are left behind
        // holding their implementations.
        bool        stepNameIncludesAttrSet = false;
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

    // Public so production-class free functions can call them
    static QString swiftType(const QString& specType);
    static QString toIdentifier(const QString& name);  // lowerCamelCase — properties, functions
    static QString toArgLabel(const QString& name);    // same, without the keyword backticks
    static QString toTypeName(const QString& name);    // UpperCamelCase — struct/class/enum names

private:
    QStringList m_extraImports;
    QString     m_tagFilter;
    bool        m_failEveryTest = true;

    static QString parseExpr(const QString& field, const QString& specType);
    static QString toFnName(const QString& keyword, const QString& stepText,
                            const QString& attrSetName = QString());
    static QString toFnName(const Step& step);  // lowerCamelCase

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
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file,
                                        QStringList* conflicts = nullptr);
    static QString genStubFn(const GlueSig& sig,
                                   bool failEveryTest);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs,
                                   bool failEveryTest);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
