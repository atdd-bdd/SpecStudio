#include "SpecConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

SpecConfig SpecConfig::load(const QString& filePath)
{
    SpecConfig cfg;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return cfg;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return cfg;

    QJsonObject o = doc.object();
    if (o.contains("outputDirectory")) cfg.outputDirectory  = o["outputDirectory"].toString();
    if (o.contains("language"))        cfg.language         = o["language"].toString();
    if (o.contains("framework"))       cfg.framework        = o["framework"].toString();
    if (o.contains("namespace"))       cfg.namespacePrefix  = o["namespace"].toString();
    if (o.contains("overwriteGlue"))   cfg.overwriteGlue    = o["overwriteGlue"].toBool();
    if (o.contains("converterPath"))   cfg.converterPath    = o["converterPath"].toString();
    if (o.contains("imports")) {
        for (const QJsonValue& v : o["imports"].toArray())
            cfg.imports << v.toString();
    }
    if (o.contains("tagFilter")) cfg.tagFilter = o["tagFilter"].toString();
    return cfg;
}

bool SpecConfig::save(const QString& filePath) const
{
    QJsonObject o;
    o["version"]         = version;
    o["outputDirectory"] = outputDirectory;
    o["language"]        = language;
    o["framework"]       = framework;
    if (!namespacePrefix.isEmpty())
        o["namespace"]   = namespacePrefix;
    o["overwriteGlue"]   = overwriteGlue;
    if (!converterPath.isEmpty())
        o["converterPath"] = converterPath;
    if (!imports.isEmpty()) {
        QJsonArray arr;
        for (const QString& imp : imports) arr << imp;
        o["imports"] = arr;
    }
    if (!tagFilter.isEmpty())
        o["tagFilter"] = tagFilter;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return true;
}

QStringList SpecConfig::frameworksFor(const QString& language)
{
    if (language == "Java")   return { "JUnit", "TestNG" };
    if (language == "Python") return { "pytest", "unittest" };
    if (language == "Rust")   return { "builtin" };
    return { "MSTest", "NUnit", "xUnit" };   // CSharp default
}
