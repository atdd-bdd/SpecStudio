#pragma once

#include "SpectableModel.h"
#include "SpectableParser.h"   // CellShape
#include <QString>
#include <QStringList>

class GoGenerator {
public:
    struct Options {
        QString     outputDir;
        QString     sourceRoot;     // project root; used to mirror the .spectable's subfolder in output
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QStringList extraImports;
        QString     tagFilter;
        bool        createProductionClasses  = false;
        QString     productionClassesDir;
        QString     productionClassesPackage; // Go package name (default: "domain")
        bool        failEveryTest = true;            // end every generated glue stub with a failure
        // Append the step's AttributeSet/Entity to its glue method name, so
        // that two steps reading alike but taking different tables become two
        // methods instead of one collision. Off unless a .specconfig asks for
        // it: turning it on renames every affected glue method, and
        // appendMissingStubs matches by name, so the old ones are left behind
        // holding their implementations.
        bool        stepNameIncludesAttrSet = false;
        // How a table of a DataType or a built-in reaches the glue. The values
        // are of the declared type either way; these say what shape they come
        // in. Flattening a single row is safe by default -- row count is a
        // property of the table's shape and every occurrence of a step agrees
        // on it. Flattening a single cell to a bare value is not: cell count is
        // cardinality and varies between scenarios of the same step, so it is
        // off unless a project asks.
        bool        flattenSingleRowToList   = true;
        bool        flattenSingleCellToScalar = false;
    };

    QStringList generate(const SpectableFile& file, const Options& opts);

    // Public so the free cell-expression functions can call them.
    static QString goType(const QString& specType);
    static QString toExported(const QString& name);     // PascalCase

private:
    QStringList m_extraImports;
    QString     m_tagFilter;
    bool        m_failEveryTest = true;
    QString     m_modulePath;   // go.mod module name; prefixes the common import

    static bool    isAttrSetType(const QString& name, const SpectableFile& file);
    static QString goCommonType(const Field& f, const SpectableFile& file);
    static QString goNestedLiteral(const QString& cellValue, const QString& fieldType,
                                   const SpectableFile& file, const QString& prefix);
    static QString goStringLiteral(const AttrSet& as, const QStringList& row,
                                   const SpectableFile& file, const QString& prefix);
    static QString toIdentifier(const QString& name);   // snake_case
    static QString toMethodName(const QString& keyword, const QString& stepText,
                                const QString& attrSetName = QString());
    static QString toMethodName(const Step& step); // PascalCase
    static QString toPackageName(const QString& name);  // lowercase no-separator
    static QString goParamName(const QString& fieldName); // avoids Go keywords

    static bool    isDataType(const QString& name, const SpectableFile& file);
    static bool    isCollectionType(const QString& name, const SpectableFile& file);
    static QString collectionElementType(const QString& name, const SpectableFile& file);

    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    static QVector<QStringList> resolveExamplesRows(
        const NamedBlock& nb, const AttrSet* as);

    QString genStringStruct(const AttrSet& as, const QString& pkg,
                            const SpectableFile& file) const;
    QString genTypedStruct(const AttrSet& as, const QString& pkg,
                           const SpectableFile& file) const;
    QString genCommonGo(const QString& pkg) const;
    QString genTestFile(const SpectableFile& file, const QString& specPkg,
                        const QString& glueType, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& specPkg,
                        const QString& glueType) const;

    struct GlueSig {
        QString method;
        QString paramType; // "" = void, "docstring", "grid", or "{Name}String"
        // The built-in, or (for EveryCell) the Entity, a grid step declared,
        // and the shape its table arrives in. Empty name = not that kind of
        // step, and the glue keeps the grid of text it always had -- which is
        // what a *user* DataType does here, because Go's constructor for one
        // may or may not return an error and the generator cannot know which.
        QString gridDataType;
        QString gridElemType;      // what one cell becomes, spelled in Go
        CellShape gridShape = CellShape::Grid;
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file,
                                        QStringList* conflicts = nullptr);
    static QString genStubFn(const GlueSig& sig, const QString& glueType,
                                   bool failEveryTest);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   const QString& glueType,
                                   const QString& modulePath,
                                   QStringList& msgs,
                                   bool failEveryTest);

    static QString genProductionDataTypeEnum(const NamedBlock& nb, const QString& pkg);
    static QString genProductionDataTypeStruct(const NamedBlock& nb, const QString& pkg);
    QString genProductionEntity(const AttrSet& as, const QString& pkg) const;
    QString genProductionCollection(const Collection& col, const QString& pkg) const;

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
