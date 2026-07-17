#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QStringList>

class CppGenerator {
public:
    struct Options {
        QString     outputDir;
        QString     sourceRoot;     // project root; used to mirror the .spectable's subfolder in output
        bool        overwriteGlue          = false;
        bool        copySpectable          = true;
        QStringList extraIncludes;          // extra #include lines injected into generated files
        QString     tagFilter;
        bool        createProductionClasses = false;
        QString     productionClassesDir;
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

    // Type / identifier helpers (public so free production functions can call them)
    static QString cppType(const QString& specType);
    static QString parseExpr(const QString& field, const QString& specType);
    static QString toIdentifier(const QString& name);   // snake_case
    static QString toTypeName(const QString& name);     // PascalCase
    static QString toFnName(const QString& keyword, const QString& stepText);

private:
    QStringList m_extraIncludes;
    QString     m_tagFilter;

    // Lookup helpers
    static bool        isDataType(const QString& name, const SpectableFile& file);
    static bool        isCollectionType(const QString& name, const SpectableFile& file);
    static QString     collectionElementType(const QString& name, const SpectableFile& file);
    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    // Row resolution
    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);
    static QVector<QStringList> resolveExamplesRows(
        const NamedBlock& nb, const AttrSet* as);

    // File generators
    QString genStringHeader(const AttrSet& as) const;
    QString genTypedHeader(const AttrSet& as) const;
    QString genCommonHeader(const QVector<AttrSet>& attrSets) const;
    QString genTestFile(const SpectableFile& file, const QString& specSnake,
                        const QString& glueClass, const QString& commonRelPath,
                        QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& glueClass,
                        const QString& commonRelPath) const;

    struct GlueSig {
        QString method;
        QString paramType;  // "" = void; "docstring"; "grid"; or "{AttrSetName}String"
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file);
    static QString genStubMethod(const GlueSig& sig);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
