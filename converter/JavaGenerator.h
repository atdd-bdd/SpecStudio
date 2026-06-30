#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class JavaGenerator
{
public:
    struct Options {
        QString packagePrefix = "gherkinexecutor";
        QString outputDir;
        QString sourceRoot;     // project root for computing package from subfolder path
        bool    overwriteGlue = false;
        QString framework     = "JUnit";  // "JUnit", "TestNG"
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

private:
    QString m_framework;

    static QString javaType(const QString& specType);
    static QString parseExpr(const QString& field, const QString& specType);

    static QString toClassName(const QString& name);
    static QString toMethodName(const QString& keyword, const QString& stepText);
    static QString toCamelCase(const QString& fieldName);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    QString genStringClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports) const;
    QString genTypedClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports) const;
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
