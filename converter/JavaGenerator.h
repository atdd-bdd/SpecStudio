#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>
#include <QMap>

class JavaGenerator
{
public:
    struct Options {
        QString     packagePrefix = "gherkinexecutor";
        QString     outputDir;
        QString     sourceRoot;     // project root for computing package from subfolder path
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QString     framework     = "JUnit";  // "JUnit", "TestNG"
        QStringList extraImports;             // injected after auto-imports in every generated file
        QString     tagFilter;               // boolean tag expression; empty = generate all
        bool        createProductionClasses = false; // generate production stubs for DataTypes
        QString     productionClassesDir;            // output folder for production classes
        QString     productionClassesPackage;        // Java package for production classes
        bool        failEveryTest = true;            // end every generated glue stub with fail()
        // Append the step's AttributeSet/Entity to its glue method name, so
        // that two steps reading alike but taking different tables become two
        // methods instead of one collision. Off unless a .specconfig asks for
        // it: turning it on renames every affected glue method, and
        // appendMissingStubs matches by name, so the old ones are left behind
        // holding their implementations.
        bool        stepNameIncludesAttrSet = false;
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

    static QString javaType(const QString& specType);

    struct GlueSig { QString method; QString paramType; QString gridDataType; bool isAttrSet = false; QString dataTypeName; };

private:
    QString     m_framework;
    QStringList m_extraImports;
    QString     m_tagFilter;
    bool        m_failEveryTest = true;
    static QString parseExpr(const QString& field, const QString& specType,
                              int line, QStringList& msgs,
                              const SpectableFile* file = nullptr,
                              const QString& objectRef = "this");

    static QString toClassName(const QString& name);
    static QString toMethodName(const QString& keyword, const QString& stepText,
                                const QString& attrSetName = QString());
    static QString toMethodName(const Step& step);
    static QString toCamelCase(const QString& fieldName);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    QString genStringClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports, QStringList& msgs, const SpectableFile& file) const;
    QString genTypedClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports, QStringList& msgs, const SpectableFile& file) const;
    QString genTestFile(const SpectableFile& file, const QString& testPkg,
                        const QString& specPkg, const QString& domainPkg,
                        const QString& className, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& specPkg,
                        const QString& domainPkg, const QString& className) const;

    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file, QStringList* conflicts = nullptr);
    static QString genStubMethod(const GlueSig& sig, const QString& framework,
                                 bool failEveryTest);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   const SpectableFile& file,
                                   QStringList& msgs,
                                   const QString& framework,
                                   bool failEveryTest);

    // Adds this specification's grid converters to common/TableHelper.java,
    // keeping any that other specifications put there. Several specs share the
    // file, so it is merged under a lock rather than replaced.
    static bool mergeTableHelper(const QString& path,
                                 const QMap<QString, QString>& fresh,
                                 const QString& pkg,
                                 const QStringList& extraImports,
                                 QStringList& msgs);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
