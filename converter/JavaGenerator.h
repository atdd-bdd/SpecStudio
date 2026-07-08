#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

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
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

private:
    QString     m_framework;
    QStringList m_extraImports;
    QString     m_tagFilter;

    static QString javaType(const QString& specType);
    static QString parseExpr(const QString& field, const QString& specType,
                              int line, QStringList& msgs,
                              const SpectableFile* file = nullptr);

    static QString toClassName(const QString& name);
    static QString toMethodName(const QString& keyword, const QString& stepText);
    static QString toCamelCase(const QString& fieldName);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    QString genStringClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports, QStringList& msgs, const SpectableFile& file) const;
    QString genTypedClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports, QStringList& msgs) const;
    QString genTestFile(const SpectableFile& file, const QString& testPkg,
                        const QString& specPkg, const QString& domainPkg,
                        const QString& className, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& specPkg,
                        const QString& domainPkg, const QString& className) const;

    struct GlueSig { QString method; QString paramType; };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubMethod(const GlueSig& sig);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
