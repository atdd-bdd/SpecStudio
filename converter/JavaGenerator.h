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
    static QString paramName(const QString& fieldName);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    QString genStringClass(const AttrSet& as, const QString& pkg) const;
    QString genTypedClass(const AttrSet& as, const QString& pkg) const;
    QString genTestFile(const SpectableFile& file, const QString& pkg,
                        const QString& className, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& pkg,
                        const QString& className) const;

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
