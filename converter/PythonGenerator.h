#pragma once

#include "SpectableModel.h"
#include "SpectableParser.h"   // CellShape
#include <QString>
#include <QStringList>

class PythonGenerator
{
public:
    struct Options {
        QString     packagePrefix;              // module prefix (currently unused but reserved)
        QString     outputDir;
        QString     sourceRoot;     // project root; used to mirror the .spectable's subfolder in output
        bool        overwriteGlue = false;
        bool        copySpectable = true;
        QStringList extraImports;               // injected as verbatim "import ..." lines
        QString     tagFilter;                  // boolean tag expression; empty = generate all
        bool        createProductionClasses = false;
        QString     productionClassesDir;
        QString     productionClassesPackage;   // reserved
        bool        failEveryTest = true;            // end every generated glue stub with a failure
        // Append the step's AttributeSet/Entity to its glue method name, so
        // that two steps reading alike but taking different tables become two
        // methods instead of one collision. Off unless a .specconfig asks for
        // it: turning it on renames every affected glue method, and
        // appendMissingStubs matches by name, so the old ones are left behind
        // holding their implementations.
        bool        stepNameIncludesAttrSet = false;
        // Only where leaving the set off would make two different steps share
        // one glue method. Ignored unless stepNameIncludesAttrSet is set.
        bool        stepNameAttrSetOnCollision = false;
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

    // Type / identifier helpers (public so free cell-expression functions can
    // call them)
    static QString pythonType(const QString& specType);
    static QString toTypeName(const QString& name);     // PascalCase
    static QString toModuleName(const QString& name);   // snake_case module

private:
    QStringList m_extraImports;
    QString     m_tagFilter;
    bool        m_failEveryTest = true;

    // Type / identifier helpers
    static QString parseExpr(const QString& field, const QString& specType);
    static QString toIdentifier(const QString& name);   // snake_case
    static QString toMethodName(const QString& keyword, const QString& stepText,
                                const QString& attrSetName = QString());
    static QString toMethodName(const Step& step);

    // Lookup helpers
    static bool        isAttrSetType(const QString& name, const SpectableFile& file);
    static QString     dncLiteral(const QString& fieldType, const SpectableFile& file);
    static QString     resolveAttrCellExpr(const QString& cellValue, const QString& fieldType,
                                            const SpectableFile& file);
    static bool        isDataType(const QString& name, const SpectableFile& file);
    static bool        isCollectionType(const QString& name, const SpectableFile& file);
    static QString     collectionElementType(const QString& name, const SpectableFile& file);
    static const AttrSet* findAttrSet(const QString& name, const SpectableFile& file);
    static const Define*  findDefine(const QString& name, const SpectableFile& file);

    // Table resolution
    static QVector<QStringList> resolveStepRows(
        const Step& step, const AttrSet* attrSet,
        const SpectableFile& file, QStringList& errors);

    // File generators
    QString genStringClass(const AttrSet& as, const SpectableFile& file) const;
    QString genTypedClass(const AttrSet& as, const SpectableFile& file) const;
    QString genEqualityMethods(const AttrSet& as, const QString& cn,
                               bool dncAware) const;
    QString genCommonInit(const QVector<AttrSet>& attrSets,
                          const QStringList& dataTypeNames,
                          const QString& existing,
                          const QString& dir) const;
    QString genTestFile(const SpectableFile& file, const QString& specSnake,
                        const QString& glueClass, QStringList& errors) const;
    QString genGlueFile(const SpectableFile& file, const QString& glueClass) const;
    static QString pyDefaultLiteral(const Field& f, const QString& pt);
    static QString genProductionDataTypeEnum(const NamedBlock& nb);
    static QString genProductionDataTypeClass(const NamedBlock& nb);
    static QString genProductionEntity(const AttrSet& as);
    static QString genProductionCollection(const Collection& col);

    struct GlueSig {
        QString method;
        QString paramType;  // "" = void; "docstring"; struct name; "grid"
        // The DataType, built-in or (for EveryCell) Entity a grid step
        // declared, and the shape its table arrives in. Empty name = not that
        // kind of step, and the glue keeps the grid of text it always had.
        QString gridDataType;
        QString gridElemType;          // what one cell becomes, spelled in Python
        CellShape gridShape = CellShape::Grid;
    };
    static QVector<GlueSig> collectGlueSigs(const SpectableFile& file,
                                        QStringList* conflicts = nullptr);
    static QString genStubMethod(const GlueSig& sig,
                                   bool failEveryTest);
    static bool appendMissingStubs(const QString& gluePath,
                                   const QVector<GlueSig>& sigs,
                                   QStringList& msgs,
                                   bool failEveryTest);

    static bool writeFile(const QString& path, const QString& content, QStringList& msgs);
};
