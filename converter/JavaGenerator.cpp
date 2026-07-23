#include "JavaGenerator.h"
#include "TagFilter.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>
#include <QSet>
#include <algorithm>

static QString toCamelCase(const QString& fieldName)
{
    const QStringList parts = fieldName.split(QRegularExpression(R"([\s_]+)"),
                                               Qt::SkipEmptyParts);
    if (parts.isEmpty()) return fieldName;
    QString result = parts[0][0].toLower() + parts[0].mid(1);
    for (int i = 1; i < parts.size(); ++i)
        result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

static QString resolveCell(const QString& cell)
{
    QString s = cell;
    return s.replace('~', ' ');
}

static QString resolveValue(const QString& cell, const SpectableFile& file)
{
    if (cell.startsWith('=')) {
        const QString name = cell.mid(1).trimmed();
        for (const Define& d : file.defines)
            if (d.name.compare(name, Qt::CaseInsensitive) == 0 && !d.isTable && !d.hasDocString) {
                QString v = d.scalarValue;
                return v.replace('~', ' ');
            }
    }
    return resolveCell(cell);
}

// Join a package prefix with a suffix; if prefix is empty, return suffix alone.
static QString joinPkg(const QString& prefix, const QString& suffix)
{
    return prefix.isEmpty() ? suffix : prefix + "." + suffix;
}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

QString JavaGenerator::javaType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")                          return "int";
    if (t == "float" || t == "scientific")                     return "double";
    if (t == "decimal")                                        return "java.math.BigDecimal";
    if (t == "yesno")                                          return "YesNo";
    if (t == "boolean" || t == "bool")         return "boolean";
    if (t == "date")                           return "LocalDate";
    if (t == "time")                           return "LocalTime";
    if (t == "datetime")                       return "LocalDateTime";
    if (t == "duration")                       return "Duration";
    if (t == "string" || t == "text")           return "String";
    if (t == "character" || t == "char")        return "char";
    return specType.trimmed();
}

static QString javaBoxedType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")                          return "Integer";
    if (t == "float" || t == "scientific")                     return "Double";
    if (t == "decimal")                                        return "java.math.BigDecimal";
    if (t == "yesno")                          return "YesNo";
    if (t == "boolean" || t == "bool")         return "Boolean";
    if (t == "date")                           return "LocalDate";
    if (t == "time")                           return "LocalTime";
    if (t == "datetime")                       return "LocalDateTime";
    if (t == "duration")                       return "Duration";
    if (t == "string" || t == "text")           return "String";
    if (t == "character" || t == "char")        return "Character";
    return specType.trimmed();
}

static bool isDataType(const QString& name, const SpectableFile& file)
{
    static const QStringList builtins = {
        "Character", "String", "Text", "Integer", "Float", "Scientific", "Decimal", "Boolean",
        "Date", "Time", "DateTime", "Duration", "YesNo"
    };
    for (const QString& b : builtins)
        if (b.compare(name, Qt::CaseInsensitive) == 0) return true;
    for (const QString& d : file.dataTypeNames)
        if (d.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

static bool isEnumType(const QString& name, const SpectableFile& file)
{
    for (const NamedBlock& nb : file.namedBlocks)
        if (nb.kind.compare("DataType", Qt::CaseInsensitive) == 0
         && nb.name.compare(name, Qt::CaseInsensitive) == 0
         && nb.examples.attrSetName.compare("EnumerationValues", Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

// True only for a user-declared DataType (has its own wrapper class with a
// "value" field) — excludes built-ins, which have no such class.
static bool isUserDataType(const QString& name, const SpectableFile& file)
{
    for (const QString& d : file.dataTypeNames)
        if (d.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

static bool isAttrSetType(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

static bool isCollectionType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

static QString collectionElementType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return c.elementType;
    return {};
}

QString JavaGenerator::parseExpr(const QString& field, const QString& specType,
                                  int line, QStringList& msgs,
                                  const SpectableFile* file,
                                  const QString& objectRef)
{
    const QString ref = objectRef + "." + field;
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int" || t == "long")
        return QString("Integer.parseInt(%1)").arg(ref);
    if (t == "float" || t == "scientific" || t == "double")
        return QString("Double.parseDouble(%1)").arg(ref);
    if (t == "decimal")
        return QString("new java.math.BigDecimal(%1)").arg(ref);
    if (t == "yesno")
        return QString("new YesNo(%1)").arg(ref);
    if (t == "boolean" || t == "bool")
        return QString("(%1.equalsIgnoreCase(\"true\") || %1.equalsIgnoreCase(\"t\") "
                       "|| %1.equalsIgnoreCase(\"yes\") || %1.equalsIgnoreCase(\"y\") "
                       "|| %1.equals(\"1\"))").arg(ref);
    if (t == "date")      return QString("LocalDate.parse(%1)").arg(ref);
    if (t == "time")      return QString("LocalTime.parse(%1)").arg(ref);
    if (t == "datetime")  return QString("LocalDateTime.parse(%1)").arg(ref);
    if (t == "duration")  return QString("Duration.parse(%1)").arg(ref);
    if (t == "string" || t == "text")
        return ref;
    if (t == "character" || t == "char")
        return QString("%1.isEmpty() ? '\\0' : %1.charAt(0)").arg(ref);
    if (file && isEnumType(specType.trimmed(), *file))
        return QString("%1.valueOf(%2)").arg(specType.trimmed()).arg(ref);
    if (file && isDataType(specType.trimmed(), *file))
        return QString("new %1(%2)").arg(specType.trimmed()).arg(ref);
    if (file && isAttrSetType(specType.trimmed(), *file))
        return QString("new %1Typed(%2)").arg(specType.trimmed()).arg(ref);
    msgs << QString("WARNING:%1:Unknown type '%2' for field '%3' — no parse conversion available")
                .arg(line).arg(specType.trimmed()).arg(field);
    return ref;
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString JavaGenerator::toClassName(const QString& name)
{
    // Underscore-separated (not merged PascalCase) so the generated name stays
    // visually close to the original Specification/Scenario/DataType name --
    // just guaranteed to start with a capital letter.
    QString s = name;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s = s.remove(QRegularExpression("^_+|_+$"));
    if (!s.isEmpty() && s[0].isDigit()) s.prepend("_");
    if (!s.isEmpty()) s[0] = s[0].toUpper();
    return s;
}

QString JavaGenerator::toMethodName(const QString& keyword, const QString& stepText)
{
    QString s = keyword + "_" + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s = s.remove(QRegularExpression("^_+|_+$"));
    return s;
}

QString JavaGenerator::toCamelCase(const QString& fieldName)
{
    return ::toCamelCase(fieldName);
}

// ---------------------------------------------------------------------------
// Convert a raw cell string to a typed Java literal for the given spec type
// ---------------------------------------------------------------------------

static QString cellLiteral(const QString& val, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "string" || t == "text" || t == "character" || t == "char")
        return "\"" + val + "\"";
    if (t == "integer" || t == "int" || t == "long")
        return val;
    if (t == "float" || t == "scientific" || t == "double")
        return val;
    if (t == "decimal")
        return "new java.math.BigDecimal(\"" + val + "\")";
    if (t == "boolean" || t == "yesno" || t == "bool") {
        const QString v = val.toLower();
        return (v == "true" || v == "t" || v == "yes" || v == "y" || v == "1")
               ? "true" : "false";
    }
    return "\"" + val + "\"";  // DataType or unknown — string fallback
}

// DataType detection (built-in names + user-declared)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Examples-table row resolution for NamedBlock (BusinessRule / Calc / DataType)
// ---------------------------------------------------------------------------

static QVector<QStringList> resolveExamplesRows(const NamedBlock& block, const AttrSet* as,
                                                 QStringList& warnings)
{
    QVector<QStringList> result;
    if (block.examples.header.isEmpty() && block.examples.rows.isEmpty())
        return result;

    if (!as) {
        result = block.examples.rows;
        return result;
    }

    const int fieldCount = as->fields.size();
    QMap<QString, int> fieldIdx;
    for (int i = 0; i < as->fields.size(); ++i)
        fieldIdx[as->fields[i].name.toLower()] = i;

    QVector<int> colMap;
    for (const QString& h : block.examples.header)
        colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);

    // Missing AttrSet field columns: error if the field has no default value to
    // fall back on; otherwise it's fine — the default is used silently.
    for (const Field& f : as->fields) {
        bool found = false;
        for (const QString& h : block.examples.header)
            if (h.compare(f.name, Qt::CaseInsensitive) == 0) { found = true; break; }
        if (!found && f.defaultValue.trimmed().isEmpty())
            warnings << QString("ERROR:%1:%2 '%3' Examples table is missing column '%4' "
                                 "and '%4' has no default value")
                        .arg(block.examples.line).arg(block.kind).arg(block.name).arg(f.name);
    }

    // Extra/unrecognized header columns: warn but keep compiling.
    for (const QString& h : block.examples.header) {
        if (!fieldIdx.contains(h.toLower()))
            warnings << QString("WARNING:%1:%2 '%3' Examples table has column '%4' which doesn't "
                                 "match any field on '%5' — it will be ignored")
                        .arg(block.examples.line).arg(block.kind).arg(block.name).arg(h, as->name);
    }

    for (const QStringList& dr : block.examples.rows) {
        QStringList row(fieldCount);
        for (int i = 0; i < as->fields.size(); ++i)
            row[i] = as->fields[i].defaultValue;
        for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
            if (colMap[ci] >= 0) row[colMap[ci]] = resolveCell(dr[ci]);
        result << row;
    }
    return result;
}

// ---------------------------------------------------------------------------
// AttrSet / Define lookup (identical logic to CSharpGenerator)
// ---------------------------------------------------------------------------

const AttrSet* JavaGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0)
            return &as;
    return nullptr;
}

const Define* JavaGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0)
            return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Table resolution (same logic as CSharpGenerator)
// ---------------------------------------------------------------------------

QVector<QStringList> JavaGenerator::resolveStepRows(
    const Step& step, const AttrSet* attrSet,
    const SpectableFile& file, QStringList& errors)
{
    QVector<QStringList> result;
    if (!attrSet) return result;

    const int fieldCount = attrSet->fields.size();

    if (!step.defineRef.isEmpty()) {
        const Define* def = findDefine(step.defineRef, file);
        if (!def) {
            errors << QString("WARNING:%1:Define '%2' not found")
                      .arg(step.line).arg(step.defineRef);
            return result;
        }
        if (!def->isTable) {
            QStringList row(fieldCount);
            if (fieldCount > 0) row[0] = def->scalarValue;
            result << row;
            return result;
        }

        QMap<QString, int> fieldIdx;
        for (int i = 0; i < attrSet->fields.size(); ++i)
            fieldIdx[attrSet->fields[i].name.toLower()] = i;

        const bool useKV = def->vertical || step.vertical;
        const QString fillVal = step.compareOnly ? "?DNC?" : QString();
        if (useKV) {
            const int startIdx = def->vertical ? 1 : 0;
            QStringList row(fieldCount);
            for (int i = 0; i < attrSet->fields.size(); ++i)
                row[i] = step.compareOnly ? fillVal : attrSet->fields[i].defaultValue;
            for (int ri = startIdx; ri < def->tableRows.size(); ++ri) {
                const QStringList& r = def->tableRows[ri];
                if (r.size() < 2) continue;
                QString key = r[0].toLower();
                if (fieldIdx.contains(key))
                    row[fieldIdx[key]] = resolveValue(r[1], file);
            }
            result << row;
        } else {
            if (def->tableRows.isEmpty()) return result;
            const QStringList& hdrs = def->tableRows[0];
            QVector<int> colMap;
            for (const QString& h : hdrs)
                colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);
            if (!step.compareOnly) {
                for (const Field& f : attrSet->fields) {
                    bool found = false;
                    for (const QString& h : hdrs)
                        if (h.compare(f.name, Qt::CaseInsensitive) == 0) { found = true; break; }
                    if (!found && f.defaultValue.trimmed().isEmpty())
                        errors << QString("ERROR:%1:Table is missing column '%2' and '%2' has no default value")
                                    .arg(step.line).arg(f.name);
                }
            }
            for (const QString& h : hdrs) {
                if (!fieldIdx.contains(h.toLower()))
                    errors << QString("WARNING:%1:Table has column '%2' which doesn't match any field on '%3' — it will be ignored")
                                .arg(step.line).arg(h, attrSet->name);
            }
            for (int ri = 1; ri < def->tableRows.size(); ++ri) {
                QStringList row(fieldCount);
                for (int i = 0; i < attrSet->fields.size(); ++i)
                    row[i] = step.compareOnly ? fillVal : attrSet->fields[i].defaultValue;
                const QStringList& dr = def->tableRows[ri];
                for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
                    if (colMap[ci] >= 0) row[colMap[ci]] = resolveValue(dr[ci], file);
                result << row;
            }
        }
        return result;
    }

    if (!step.hasTable || step.table.rows.isEmpty()) return result;

    QMap<QString, int> fieldIdx;
    for (int i = 0; i < attrSet->fields.size(); ++i)
        fieldIdx[attrSet->fields[i].name.toLower()] = i;

    const QString fillVal = step.compareOnly ? "?DNC?" : QString();
    if (step.table.vertical) {
        // Each row = [AttrName, Value [, Value2, ...]]
        // Extra columns are additional list items; each value column = one result row.
        int numCols = 0;
        for (const QStringList& r : step.table.rows)
            if (r.size() > numCols) numCols = r.size();
        for (int col = 1; col < numCols; ++col) {
            QStringList row(fieldCount);
            for (int i = 0; i < attrSet->fields.size(); ++i)
                row[i] = step.compareOnly ? fillVal : attrSet->fields[i].defaultValue;
            for (const QStringList& r : step.table.rows) {
                if (r.size() < 2) continue;
                const QString key = r[0].toLower();
                if (fieldIdx.contains(key) && col < r.size())
                    row[fieldIdx[key]] = resolveValue(r[col], file);
            }
            result << row;
        }
    } else {
        if (step.table.rows.size() < 2) return result;
        const QStringList& hdrs = step.table.rows[0];
        QVector<int> colMap;
        for (const QString& h : hdrs)
            colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);
        if (!step.compareOnly) {
            for (const Field& f : attrSet->fields) {
                bool found = false;
                for (const QString& h : hdrs)
                    if (h.compare(f.name, Qt::CaseInsensitive) == 0) { found = true; break; }
                if (!found && f.defaultValue.trimmed().isEmpty())
                    errors << QString("ERROR:%1:Table is missing column '%2' and '%2' has no default value")
                                .arg(step.line).arg(f.name);
            }
        }
        for (const QString& h : hdrs) {
            if (!fieldIdx.contains(h.toLower()))
                errors << QString("WARNING:%1:Table has column '%2' which doesn't match any field on '%3' — it will be ignored")
                            .arg(step.line).arg(h, attrSet->name);
        }
        for (int ri = 1; ri < step.table.rows.size(); ++ri) {
            QStringList row(fieldCount);
            for (int i = 0; i < attrSet->fields.size(); ++i)
                row[i] = step.compareOnly ? fillVal : attrSet->fields[i].defaultValue;
            const QStringList& dr = step.table.rows[ri];
            for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
                if (colMap[ci] >= 0) row[colMap[ci]] = resolveValue(dr[ci], file);
            result << row;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// String class
// ---------------------------------------------------------------------------

QString JavaGenerator::genStringClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports, QStringList& msgs, const SpectableFile& file) const
{
    const QString cn = as.name + "String";
    const QString tn = as.name + "Typed";
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << ";\n\n";

    s << "import java.util.Objects;\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    s << "\n";

    s << "public class " << cn << " {\n";
    s << "    private static final String DNCString = \"?DNC?\";\n\n";

    for (const Field& f : as.fields) {
        const QString fType = isAttrSetType(f.type, file) ? f.type + "String" : "String";
        s << "    public " << fType << " " << toCamelCase(f.name) << ";\n";
    }
    s << "\n";

    // Constructor
    s << "    public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString fType = isAttrSetType(as.fields[i].type, file) ? as.fields[i].type + "String" : "String";
        s << fType << " " << toCamelCase(as.fields[i].name);
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "        this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "    }\n\n";

    // equals() — DNCString on either side skips the field comparison
    s << "    @Override\n    public boolean equals(Object o) {\n";
    s << "        if (this == o) return true;\n";
    s << "        if (!(o instanceof " << cn << ")) return false;\n";
    s << "        " << cn << " that = (" << cn << ") o;\n";
    s << "        return ";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << "\n            && ";
        const QString field = toCamelCase(as.fields[i].name);
        if (isAttrSetType(as.fields[i].type, file)) {
            // sub-object field — delegate to its own equals (no DNC at this level)
            s << "Objects.equals(" << field << ", that." << field << ")";
        } else {
            s << "(DNCString.equals(" << field << ") || DNCString.equals(that." << field << ")"
              << " || Objects.equals(" << field << ", that." << field << "))";
        }
    }
    s << ";\n    }\n\n";

    // hashCode()
    s << "    @Override\n    public int hashCode() {\n        return Objects.hash(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << toCamelCase(as.fields[i].name);
    }
    s << ");\n    }\n\n";

    // toString() — label uses original name, value reference uses camelCase identifier
    s << "    @Override\n    public String toString() {\n        return ";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << " + \", \" + ";
        s << "\"" << as.fields[i].name << "=\" + " << toCamelCase(as.fields[i].name);
    }
    s << ";\n    }\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed class
// ---------------------------------------------------------------------------

QString JavaGenerator::genTypedClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports, QStringList& msgs, const SpectableFile& file) const
{
    const QString cn  = as.name + "Typed";
    const QString csn = as.name + "String";
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << ";\n\n";

    bool needsDate = false, needsTime = false, needsDt = false, needsDur = false;
    bool needsList = false, needsCollections = false;
    for (const Field& f : as.fields) {
        const QString t = f.type.toLower();
        if (t == "date")     needsDate = true;
        if (t == "time")     needsTime = true;
        if (t == "datetime") needsDt   = true;
        if (t == "duration") needsDur  = true;
        if (isCollectionType(f.type, file)) needsList = true, needsCollections = true;
    }
    if (needsDate)        s << "import java.time.LocalDate;\n";
    if (needsTime)        s << "import java.time.LocalTime;\n";
    if (needsDt)          s << "import java.time.LocalDateTime;\n";
    if (needsDur)         s << "import java.time.Duration;\n";
    s << "import java.util.ArrayList;\n";
    s << "import java.util.List;\n";
    if (needsCollections) s << "import java.util.Collections;\n";
    s << "import java.util.Objects;\n";
    s << "import org.json.JSONObject;\n";
    s << "import org.json.JSONArray;\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    s << "\n";

    s << "public class " << cn << " {\n";

    for (const Field& f : as.fields) {
        const bool isCol = isCollectionType(f.type, file);
        const QString elemT = isCol ? collectionElementType(f.type, file) : QString();
        const QString jt = isCol ? (isAttrSetType(elemT, file) ? elemT + "Typed" : javaBoxedType(elemT))
                         : isAttrSetType(f.type, file) ? f.type + "Typed"
                         : javaType(f.type);
        if (isCol)
            s << "    public List<" << jt << "> " << toCamelCase(f.name) << ";\n";
        else
            s << "    public " << jt << " " << toCamelCase(f.name) << ";\n";
    }
    s << "\n";

    // Constructor from typed values
    s << "    public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        const bool isCol = isCollectionType(f.type, file);
        const QString elemT = isCol ? collectionElementType(f.type, file) : QString();
        const QString jt = isCol ? (isAttrSetType(elemT, file) ? elemT + "Typed" : javaBoxedType(elemT))
                         : isAttrSetType(f.type, file) ? f.type + "Typed"
                         : javaType(f.type);
        if (isCol)
            s << "List<" << jt << "> " << toCamelCase(f.name);
        else
            s << jt << " " << toCamelCase(f.name);
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "        this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "    }\n\n";

    // Constructor from String class
    s << "    public " << cn << "(" << csn << " s) {\n";
    for (const Field& f : as.fields) {
        const QString fn = toCamelCase(f.name);
        if (isCollectionType(f.type, file)) {
            s << "        this." << fn << " = new ArrayList<>(); // Collection — populate from s." << fn << "\n";
        } else {
            const QString expr = parseExpr(fn, f.type, as.line, msgs, &file, "s");
            s << "        this." << fn << " = " << expr << ";\n";
        }
    }
    s << "    }\n\n";

    // equals()
    s << "    @Override\n    public boolean equals(Object o) {\n";
    s << "        if (this == o) return true;\n";
    s << "        if (!(o instanceof " << cn << ")) return false;\n";
    s << "        " << cn << " that = (" << cn << ") o;\n";
    s << "        return ";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << "\n            && ";
        const QString field = toCamelCase(as.fields[i].name);
        s << "Objects.equals(" << field << ", that." << field << ")";
    }
    s << ";\n    }\n\n";

    // hashCode()
    s << "    @Override\n    public int hashCode() {\n        return Objects.hash(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << toCamelCase(as.fields[i].name);
    }
    s << ");\n    }\n\n";

    // toXxxString() — convert this Typed instance back to its String equivalent
    s << "    public " << csn << " to" << csn << "() {\n";
    s << "        return new " << csn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString field = toCamelCase(as.fields[i].name);
        if (isAttrSetType(as.fields[i].type, file))
            s << field << ".to" << as.fields[i].type << "String()";
        else
            s << "String.valueOf(" << field << ")";
    }
    s << ");\n";
    s << "    }\n\n";

    // Static conversion helpers
    s << "    public static List<" << cn << "> fromStringList(List<" << csn << "> list) {\n";
    s << "        List<" << cn << "> result = new ArrayList<>();\n";
    s << "        for (" << csn << " s : list) result.add(new " << cn << "(s));\n";
    s << "        return result;\n";
    s << "    }\n\n";

    s << "    public static List<" << csn << "> toStringList(List<" << cn << "> list) {\n";
    s << "        List<" << csn << "> result = new ArrayList<>();\n";
    s << "        for (" << cn << " t : list) result.add(t.to" << csn << "());\n";
    s << "        return result;\n";
    s << "    }\n\n";

    // toJSON()
    s << "    public JSONObject toJSON() {\n";
    s << "        JSONObject obj = new JSONObject();\n";
    for (const Field& f : as.fields) {
        const QString fname = toCamelCase(f.name);
        if (isAttrSetType(f.type, file)) {
            s << "        obj.put(\"" << fname << "\", " << fname << ".toJSON());\n";
        } else {
            const QString t = f.type.toLower();
            if (t == "character" || t == "char")
                s << "        obj.put(\"" << fname << "\", String.valueOf(" << fname << "));\n";
            else if (t == "date" || t == "time" || t == "datetime" || t == "duration")
                s << "        obj.put(\"" << fname << "\", " << fname << ".toString());\n";
            else if (t == "yesno")
                s << "        obj.put(\"" << fname << "\", " << fname << ".value);\n";
            else if (isEnumType(f.type, file))
                s << "        obj.put(\"" << fname << "\", " << fname << ".name());\n";
            else if (isUserDataType(f.type, file))
                s << "        obj.put(\"" << fname << "\", " << fname << ".value);\n";
            else
                s << "        obj.put(\"" << fname << "\", " << fname << ");\n";
        }
    }
    s << "        return obj;\n";
    s << "    }\n\n";

    // fromJSON()
    s << "    public static " << cn << " fromJSON(JSONObject obj) {\n";
    s << "        return new " << cn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ",\n";
        const Field& f = as.fields[i];
        const QString fname = toCamelCase(f.name);
        if (isAttrSetType(f.type, file)) {
            s << "            " << f.type << "Typed.fromJSON(obj.getJSONObject(\"" << fname << "\"))";
        } else {
            const QString t = f.type.toLower();
            if (t == "integer" || t == "int")
                s << "            obj.getInt(\"" << fname << "\")";
            else if (t == "float" || t == "scientific")
                s << "            obj.getDouble(\"" << fname << "\")";
            else if (t == "decimal")
                s << "            obj.getBigDecimal(\"" << fname << "\")";
            else if (t == "boolean" || t == "bool")
                s << "            obj.getBoolean(\"" << fname << "\")";
            else if (t == "yesno")
                s << "            new YesNo(obj.getString(\"" << fname << "\"))";
            else if (t == "character" || t == "char")
                s << "            (char) obj.getString(\"" << fname << "\").charAt(0)";
            else if (t == "date")
                s << "            java.time.LocalDate.parse(obj.getString(\"" << fname << "\"))";
            else if (t == "time")
                s << "            java.time.LocalTime.parse(obj.getString(\"" << fname << "\"))";
            else if (t == "datetime")
                s << "            java.time.LocalDateTime.parse(obj.getString(\"" << fname << "\"))";
            else if (t == "duration")
                s << "            java.time.Duration.parse(obj.getString(\"" << fname << "\"))";
            else if (isEnumType(f.type, file))
                s << "            " << f.type << ".valueOf(obj.getString(\"" << fname << "\"))";
            else if (isUserDataType(f.type, file))
                s << "            new " << f.type << "(obj.getString(\"" << fname << "\"))";
            else
                s << "            obj.getString(\"" << fname << "\")";
        }
    }
    s << ");\n    }\n\n";

    // toJSONList()
    s << "    public static JSONArray toJSONList(List<" << cn << "> list) {\n";
    s << "        JSONArray arr = new JSONArray();\n";
    s << "        for (" << cn << " item : list) arr.put(item.toJSON());\n";
    s << "        return arr;\n";
    s << "    }\n\n";

    // fromJSONList()
    s << "    public static List<" << cn << "> fromJSONList(JSONArray arr) {\n";
    s << "        List<" << cn << "> result = new ArrayList<>();\n";
    s << "        for (int i = 0; i < arr.length(); i++) result.add(fromJSON(arr.getJSONObject(i)));\n";
    s << "        return result;\n";
    s << "    }\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Resolve a step table cell for an AttrSet-typed field.
// If the cell is "=DefineRef" and the define is a table, resolves it against
// the sub-AttrSet and returns "new XxxString(\"v1\", \"v2\", ...)".
// ---------------------------------------------------------------------------

static QString resolveAttrCellExpr(const QString& cellValue, const QString& fieldType,
                                    const SpectableFile& file, QStringList& /*errors*/)
{
    if (cellValue.startsWith('=')) {
        const QString defineName = cellValue.mid(1).trimmed();
        for (const Define& d : file.defines) {
            if (d.name.compare(defineName, Qt::CaseInsensitive) != 0 || !d.isTable)
                continue;
            for (const AttrSet& subAs : file.attrSets) {
                if (subAs.name.compare(fieldType, Qt::CaseInsensitive) != 0)
                    continue;
                QMap<QString, int> fieldIdx;
                for (int i = 0; i < subAs.fields.size(); ++i)
                    fieldIdx[subAs.fields[i].name.toLower()] = i;

                QStringList row(subAs.fields.size());
                for (int i = 0; i < subAs.fields.size(); ++i)
                    row[i] = subAs.fields[i].defaultValue;

                if (d.vertical) {
                    for (const QStringList& r : d.tableRows) {
                        if (r.size() < 2) continue;
                        if (fieldIdx.contains(r[0].toLower()))
                            row[fieldIdx[r[0].toLower()]] = r[1];
                    }
                } else if (d.tableRows.size() >= 2) {
                    const QStringList& hdrs = d.tableRows[0];
                    QVector<int> colMap;
                    for (const QString& h : hdrs)
                        colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);
                    const QStringList& dr = d.tableRows[1];
                    for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
                        if (colMap[ci] >= 0) row[colMap[ci]] = dr[ci];
                }
                QString expr = "new " + fieldType + "String(";
                for (int i = 0; i < row.size(); ++i) {
                    if (i) expr += ", ";
                    expr += "\"" + row[i] + "\"";
                }
                expr += ")";
                return expr;
            }
        }
    }
    return "\"" + cellValue + "\"";
}

// ---------------------------------------------------------------------------
// Test file
// ---------------------------------------------------------------------------

QString JavaGenerator::genTestFile(const SpectableFile& file, const QString& testPkg,
                                    const QString& specPkg, const QString& domainPkg,
                                    const QString& className, QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    if (!testPkg.isEmpty()) s << "package " << testPkg << ";\n\n";
    s << "import java.util.List;\n";
    s << "import java.util.ArrayList;\n";
    s << "import " << domainPkg << ".*;\n";
    if (!specPkg.isEmpty()) s << "import " << specPkg << "." << className << "_glue;\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";

    const bool isJUnit5 = m_framework.compare("TestNG", Qt::CaseInsensitive) != 0
                       && (m_framework.compare("JUnit", Qt::CaseInsensitive) == 0
                           || m_framework.toLower().startsWith("junit5")
                           || m_framework.toLower() == "junit 5");
    const bool isTestNG = m_framework.compare("TestNG", Qt::CaseInsensitive) == 0;

    if (isTestNG) {
        s << "import org.testng.annotations.Test;\n";
    } else if (isJUnit5) {
        s << "import org.junit.jupiter.api.Test;\n";
        // Emit Tag import only if any block has tags
        bool hasTags = false;
        for (const Scenario& sc : file.scenarios) if (!sc.tags.isEmpty()) { hasTags = true; break; }
        if (!hasTags)
            for (const NamedBlock& nb : file.namedBlocks) if (!nb.tags.isEmpty()) { hasTags = true; break; }
        if (hasTags) s << "import org.junit.jupiter.api.Tag;\n";
    } else {
        // JUnit 4
        s << "import org.junit.Test;\n";
    }

    if (m_failEveryTest) {
        if (isTestNG)      s << "import static org.testng.Assert.fail;\n";
        else if (isJUnit5) s << "import static org.junit.jupiter.api.Assertions.fail;\n";
        else               s << "import static org.junit.Assert.fail;\n";
    }

    // Helper: emit tag annotations before @Test
    auto emitTags = [&](const QStringList& tags) {
        if (isJUnit5)
            for (const QString& t : tags) s << "    @Tag(\"" << t << "\")\n";
        else if (!tags.isEmpty())
            s << "    // Tags: " << tags.join(", ") << "\n";
    };

    s << "\npublic class " << className << "_Test {\n\n";

    int objectCounter = 0;

    // Strip the common leading whitespace from docstring lines and emit as a Java text block.
    // Each content line is prefixed with `lineIndent` in the output; Java then strips that indent.
    // Issues a warning if any non-blank line lacks the common prefix.
    auto emitTextBlock = [&](const QString& docString, const QString& lineIndent, int srcLine) {
        const QStringList lines = docString.split('\n');

        // Compute common leading whitespace across non-blank lines
        QString prefix;
        bool first = true;
        for (const QString& ln : lines) {
            if (ln.trimmed().isEmpty()) continue;
            if (first) {
                int i = 0;
                while (i < ln.size() && (ln[i] == ' ' || ln[i] == '\t')) ++i;
                prefix = ln.left(i);
                first = false;
            } else {
                int maxLen = qMin(prefix.size(), ln.size());
                int i = 0;
                while (i < maxLen && prefix[i] == ln[i]) ++i;
                prefix = prefix.left(i);
            }
        }

        s << "\"\"\"\n";
        for (const QString& ln : lines) {
            if (ln.trimmed().isEmpty()) {
                s << "\n";
            } else if (ln.startsWith(prefix)) {
                s << lineIndent << ln.mid(prefix.size()) << "\n";
            } else {
                errors << QStringLiteral("WARNING:%1: Text does not align with the opening \"\"\"").arg(srcLine);
                s << lineIndent << ln << "\n";
            }
        }
        s << lineIndent << "\"\"\"";
    };

    auto emitSteps = [&](const QVector<Step>& steps, const QString& glueVar) {
        for (const Step& step : steps) {
            if (step.hasDocString) {
                const QString meth = toMethodName(step.keyword, step.text);
                s << "        " << glueVar << "." << meth << "(";
                emitTextBlock(step.docString, "        ", step.line);
                s << ");\n\n";
                continue;
            }
            if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                if (def && def->hasDocString) {
                    const QString meth = toMethodName(step.keyword, step.text);
                    s << "        " << glueVar << "." << meth << "(";
                    emitTextBlock(def->docString, "        ", step.line);
                    s << ");\n\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                // Bare step — call with no arguments
                const QString meth = toMethodName(step.keyword, step.text);
                s << "        " << glueVar << "." << meth << "();\n\n";
                continue;
            }

            // If attrSetName is a Collection, resolve to its element type's AttrSet
            const QString effectiveAttrSetName = (!step.attrSetName.isEmpty() && isCollectionType(step.attrSetName, file))
                ? collectionElementType(step.attrSetName, file)
                : step.attrSetName;
            const AttrSet* as = findAttrSet(effectiveAttrSetName, file);

            if (!step.attrSetName.isEmpty() && as == nullptr) {
                if (!isDataType(effectiveAttrSetName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined — add an 'Attributes %2' block")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
                // DataType grid step — fall through to the List<List<String>> branch below
            }

            if (!step.attrSetName.isEmpty() && as) {
                ++objectCounter;
                const QString listType = effectiveAttrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);

                QStringList localErrs;
                QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;

                s << "        List<" << listType << "> " << listVar
                  << " = new ArrayList<>();\n";
                bool hasSubObject = false;
                for (const Field& f : as->fields)
                    if (isAttrSetType(f.type, file)) { hasSubObject = true; break; }

                for (const QStringList& row : rows) {
                    if (hasSubObject) {
                        s << "        " << listVar << ".add(new " << listType << "(\n";
                        for (int ci = 0; ci < as->fields.size() && ci < row.size(); ++ci) {
                            const Field& f = as->fields[ci];
                            s << "                ";
                            if (isAttrSetType(f.type, file))
                                s << resolveAttrCellExpr(row[ci], f.type, file, errors);
                            else
                                s << "\"" << row[ci] << "\"";
                            s << (ci + 1 < as->fields.size() && ci + 1 < row.size() ? ",\n" : "));\n");
                        }
                    } else {
                        s << "        " << listVar << ".add(new " << listType << "(";
                        for (int ci = 0; ci < as->fields.size() && ci < row.size(); ++ci) {
                            if (ci) s << ", ";
                            s << "\"" << row[ci] << "\"";
                        }
                        s << "));\n";
                    }
                }
                const QString meth = toMethodName(step.keyword, step.text);
                s << "        " << glueVar << "." << meth << "(" << listVar << ");\n\n";

            } else if (step.hasTable && as == nullptr) {
                ++objectCounter;
                const QString listVar = QString("objectList%1").arg(objectCounter);
                const StepTable& tbl = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;
                const QString meth = toMethodName(step.keyword, step.text);

                s << "        List<List<String>> " << listVar << " = new ArrayList<>();\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "        " << listVar << ".add(List.of(";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << resolveValue(r[ci], file) << "\"";
                    }
                    s << "));\n";
                }
                s << "        " << glueVar << "." << meth << "(" << listVar << ");\n\n";
            }
        }
    };

    // ── Scenario tests ──────────────────────────────────────────────────────
    if (!file.scenarios.isEmpty()) {
        s << "    // -------------------------\n";
        s << "    // Scenario Tests\n";
        s << "    // -------------------------\n";
    }
    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;
        const QString meth      = "Scenario_" + toClassName(sc.name);
        const QString glueClass = className + "_glue";

        emitTags(file.tags + sc.tags);
        s << "    @Test\n";
        s << "    public void " << meth << "() {\n";
        if (m_failEveryTest) s << "        fail(\"Not yet implemented: " << meth << "\");\n";
        s << "        " << glueClass << " glue = new " << glueClass << "();\n\n";

        emitSteps(file.backgroundSteps, "glue");
        emitSteps(sc.steps, "glue");

        s << "    }\n\n";
    }

    // ── BusinessRule / Calculation / DataType tests ──────────────────────────
    static const QStringList namedKinds = { "BusinessRule", "Calculation", "DataType" };
    QSet<QString> seenNamedBlocks;  // kind:name — first definition wins; extras get a warning
    for (const QString& kind : namedKinds) {
        bool hasKind = false;
        for (const NamedBlock& nb : file.namedBlocks)
            if (nb.hasExamples && nb.kind == kind && !nb.isContext
                && !seenNamedBlocks.contains(kind + ":" + nb.name.toLower()))
                { hasKind = true; break; }
        if (!hasKind) continue;

        s << "    // -------------------------\n";
        s << "    // " << kind << " Tests\n";
        s << "    // -------------------------\n";

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind || nb.isContext) continue;
            const QString blockKey = kind + ":" + nb.name.toLower();
            if (seenNamedBlocks.contains(blockKey)) {
                errors << QString("WARNING:%1:%2 '%3' is declared in multiple files — only the first definition is tested")
                              .arg(nb.line).arg(kind).arg(nb.name);
                continue;
            }
            seenNamedBlocks.insert(blockKey);
            const QStringList effectiveGenTags = file.generatorTags + nb.generatorTags;
            if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;

            const QString meth      = kind + "_" + toClassName(nb.name);
            const QString glueMeth  = "Examples_" + kind + "_" + toClassName(nb.name);
            const QString glueClass = className + "_glue";
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            emitTags(nb.tags);
            s << "    @Test\n";
            s << "    public void " << meth << "() {\n";
            if (m_failEveryTest) s << "        fail(\"Not yet implemented: " << meth << "\");\n";
            s << "        " << glueClass << " glue = new " << glueClass << "();\n";

            if (as) {
                ++objectCounter;
                const QString listType = nb.examples.attrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, as, errors);
                s << "        List<" << listType << "> " << listVar
                  << " = new ArrayList<>();\n";
                for (const QStringList& row : rows) {
                    s << "        " << listVar << ".add(new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << row[ci] << "\"";
                    }
                    s << "));\n";
                }
                s << "        glue." << glueMeth << "(" << listVar << ");\n";
            } else {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr, errors);
                s << "        List<List<String>> " << listVar << " = new ArrayList<>();\n";
                for (const QStringList& row : rows) {
                    s << "        " << listVar << ".add(List.of(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << resolveValue(row[ci], file) << "\"";
                    }
                    s << "));\n";
                }
                s << "        glue." << glueMeth << "(" << listVar << ");\n";
            }
            s << "    }\n\n";
        }
    }

    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Glue file
// ---------------------------------------------------------------------------

QVector<JavaGenerator::GlueSig> JavaGenerator::collectGlueSigs(const SpectableFile& file, QStringList* conflicts)
{
    QVector<GlueSig> sigs;
    QMap<QString, GlueSig> seen;   // method name -> first-seen signature

    // Human-readable description of what a glue method expects, for conflict messages.
    auto describe = [](const GlueSig& s) -> QString {
        if (s.paramType.isEmpty())     return "no parameter";
        if (s.paramType == "docstring") return "a docstring";
        if (s.isAttrSet)               return QStringLiteral("a %1 table").arg(s.paramType.left(s.paramType.length() - 6));
        if (!s.dataTypeName.isEmpty()) return QStringLiteral("a %1 grid").arg(s.dataTypeName);
        return s.paramType;
    };

    auto recordSig = [&](const QString& meth, GlueSig sig, int line) {
        auto it = seen.find(meth);
        if (it == seen.end()) {
            seen.insert(meth, sig);
            sigs.push_back(sig);
            return;
        }
        const GlueSig& prior = it.value();
        const bool sameSig = prior.paramType == sig.paramType
                           && prior.isAttrSet == sig.isAttrSet
                           && prior.gridDataType == sig.gridDataType
                           && prior.dataTypeName == sig.dataTypeName;
        if (!sameSig && conflicts) {
            *conflicts << QString(
                "WARNING:%1:Step '%2' expects %3 here, but the same glue method elsewhere "
                "in this project expects %4 — rename one of the steps so they don't collide")
                .arg(line).arg(meth, describe(sig), describe(prior));
        }
        // Keep the first-seen signature; later occurrences don't get a second entry.
    };

    auto collectSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            const QString meth = toMethodName(step.keyword, step.text);
            if (step.hasDocString) {
                recordSig(meth, { meth, "docstring" }, step.line);
            } else if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                recordSig(meth, { meth, (def && def->hasDocString) ? "docstring" : "" }, step.line);
            } else if (step.attrSetName.isEmpty() && !step.hasTable) {
                recordSig(meth, { meth, "" }, step.line);                  // void / no parameter
            } else if (!step.attrSetName.isEmpty() && !isDataType(step.attrSetName, file)) {
                const QString effectiveName = isCollectionType(step.attrSetName, file)
                    ? collectionElementType(step.attrSetName, file)
                    : step.attrSetName;
                recordSig(meth, { meth, effectiveName + "String", "", true }, step.line);
            } else if (!step.attrSetName.isEmpty() && isDataType(step.attrSetName, file)) {
                recordSig(meth, { meth, "List<List<String>>", step.attrSetName }, step.line);
            } else {
                recordSig(meth, { meth, "List<List<String>>" }, step.line);
            }
        }
    };

    collectSteps(file.backgroundSteps);
    collectSteps(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios)
        collectSteps(sc.steps);

    // Named blocks — emit ExamplesBusinessRule_*, ExamplesCalculation_*, ExamplesDataType_*
    for (const NamedBlock& nb : file.namedBlocks) {
        if (!nb.hasExamples || nb.isContext) continue;
        const QString meth = "Examples_" + nb.kind + "_" + toClassName(nb.name);
        if (nb.kind == "DataType" &&
            nb.examples.attrSetName.compare("ValidValues", Qt::CaseInsensitive) == 0) {
            recordSig(meth, { meth, "ValidValuesString", "", false, nb.name }, nb.line);
        } else if (nb.kind == "DataType" &&
                   nb.examples.attrSetName.compare("EnumerationValues", Qt::CaseInsensitive) == 0) {
            recordSig(meth, { meth, "EnumerationValuesString", "", false, nb.name }, nb.line);
        } else {
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);
            if (as)
                recordSig(meth, { meth, nb.examples.attrSetName + "String", "", true }, nb.line);
            else
                recordSig(meth, { meth, "List<List<String>>" }, nb.line);
        }
    }

    return sigs;
}

static QString cellConvertExpr(const QString& specType, const SpectableFile& file)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int" || t == "long")
        return "Integer.parseInt(cell)";
    if (t == "float" || t == "scientific" || t == "double")
        return "Double.parseDouble(cell)";
    if (t == "decimal")
        return "new java.math.BigDecimal(cell)";
    if (t == "yesno")
        return "new YesNo(cell)";
    if (t == "boolean" || t == "bool")
        return "Boolean.parseBoolean(cell)";
    if (t == "string" || t == "text" || t == "character" || t == "char")
        return "cell";
    if (isEnumType(specType.trimmed(), file))
        return specType.trimmed() + ".valueOf(cell)";
    if (isDataType(specType.trimmed(), file))
        return "new " + specType.trimmed() + "(cell)";
    return "cell";
}

static QString genGridConverter(const QString& dataType, const SpectableFile& file)
{
    const QString boxed  = javaBoxedType(dataType);
    const QString cellEx = cellConvertExpr(dataType, file);
    QString out;
    QTextStream s(&out);
    s << "    public static List<List<" << boxed << ">> toListList" << boxed
      << "(List<List<String>> values) {\n";
    s << "        List<List<" << boxed << ">> result = new ArrayList<>();\n";
    s << "        for (List<String> row : values) {\n";
    s << "            List<" << boxed << "> typedRow = new ArrayList<>();\n";
    s << "            for (String cell : row) { typedRow.add(" << cellEx << "); }\n";
    s << "            result.add(typedRow);\n";
    s << "        }\n";
    s << "        return result;\n";
    s << "    }\n";
    return out;
}

static QString genTableHelperClass(const QVector<JavaGenerator::GlueSig>& sigs,
                                    const QString& pkg, const SpectableFile& file)
{
    QSet<QString> seen;
    QStringList methods;
    for (const JavaGenerator::GlueSig& sig : sigs) {
        if (sig.gridDataType.isEmpty()) continue;
        const QString boxed = javaBoxedType(sig.gridDataType);
        if (seen.contains(boxed)) continue;
        seen.insert(boxed);
        methods << genGridConverter(sig.gridDataType, file);
    }
    if (methods.isEmpty()) return {};

    QString out;
    QTextStream s(&out);
    s << "package " << pkg << ";\n\n";
    s << "import java.util.ArrayList;\n";
    s << "import java.util.List;\n\n";
    s << "public class TableHelper {\n";
    for (const QString& m : methods)
        s << "\n" << m;
    s << "}\n";
    return out;
}

// Builds a framework-correct assertEquals(...) call. JUnit4's org.junit.Assert only has a
// message-FIRST 3-arg overload (String, Object, Object); JUnit5's Assertions and TestNG's Assert
// both take the message LAST, and TestNG additionally swaps to (actual, expected, message).
static QString buildAssertEquals(const QString& framework, const QString& expectedExpr,
                                  const QString& actualExpr, const QString& messageExpr)
{
    const bool isTestNG = framework.compare("TestNG", Qt::CaseInsensitive) == 0;
    const bool isJUnit5 = !isTestNG
                       && (framework.compare("JUnit", Qt::CaseInsensitive) == 0
                           || framework.toLower().startsWith("junit5")
                           || framework.toLower() == "junit 5");
    if (isTestNG)  return QString("assertEquals(%1, %2, %3)").arg(actualExpr, expectedExpr, messageExpr);
    if (isJUnit5)  return QString("assertEquals(%1, %2, %3)").arg(expectedExpr, actualExpr, messageExpr);
    return QString("assertEquals(%1, %2, %3)").arg(messageExpr, expectedExpr, actualExpr);  // JUnit4
}

QString JavaGenerator::genStubMethod(const GlueSig& sig, const QString& framework)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "    public void " << sig.method << "() {\n";
        s << "        fail(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
        return out;
    }
    if (sig.paramType == "docstring") {
        s << "    public void " << sig.method << "(String value) {\n";
        s << "        System.out.println(value);\n";
        s << "        fail(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
        return out;
    }
    if (!sig.gridDataType.isEmpty()) {
        const QString boxed = javaBoxedType(sig.gridDataType);
        s << "    public void " << sig.method << "(List<List<String>> values) {\n";
        s << "        List<List<" << boxed << ">> typedValues = TableHelper.toListList" << boxed << "(values);\n";
        s << "        for (List<" << boxed << "> value : typedValues) {\n";
        s << "            System.out.println(value);\n";
        s << "        }\n";
        s << "        fail(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
        return out;
    }
    if (!sig.dataTypeName.isEmpty() && sig.paramType == "ValidValuesString") {
        s << "    public void " << sig.method << "(List<ValidValuesString> values) {\n";
        s << "        for (ValidValuesString value : values) {\n";
        s << "            boolean error = false;\n";
        s << "            System.out.println(value);\n";
        s << "            ValidValuesTyped vvt = new ValidValuesTyped(value);\n";
        s << "            try {\n";
        s << "                new " << sig.dataTypeName << "(vvt.value);\n";
        s << "            }\n";
        s << "            catch(NumberFormatException e){\n";
        s << "                error = true;\n";
        s << "            }\n";
        s << "            " << buildAssertEquals(framework, "vvt.isValid.toBoolean()", "!error",
                                                   "\" Value \" + vvt.value") << ";\n";
        s << "        }\n";
        s << "    }\n";
        return out;
    }
    if (!sig.dataTypeName.isEmpty() && sig.paramType == "EnumerationValuesString") {
        const QString dt = sig.dataTypeName;
        s << "    public void " << sig.method << "(List<EnumerationValuesString> values) {\n";
        s << "        for (EnumerationValuesString value : values) {\n";
        s << "            System.out.println(value);\n";
        s << "            try {\n";
        s << "                " << dt << ".valueOf(value.value);\n";
        s << "            } catch (IllegalArgumentException e) {\n";
        s << "                fail(\"Value \" + value.value + \" not in " << dt << "\");\n";
        s << "            }\n";
        s << "        }\n";
        s << "        int count = " << dt << ".values().length;\n";
        s << "        " << buildAssertEquals(framework, "values.size()", "count",
                                              "\"Number of " + dt + "s \"") << ";\n";
        s << "    }\n";
        return out;
    }
    if (sig.isAttrSet) {
        const QString strType   = sig.paramType;   // e.g. ShoppingCartString
        const QString typedType = strType.left(strType.length() - 6) + "Typed";
        s << "    public void " << sig.method << "(List<" << strType << "> values) {\n";
        s << "        for (" << strType << " value : values) {\n";
        s << "            " << typedType << " typed = new " << typedType << "(value);\n";
        s << "            System.out.println(typed);\n";
        s << "        }\n";
        s << "        fail(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
        return out;
    }
    const QString paramType = sig.paramType.contains('<')
        ? sig.paramType
        : QString("List<%1>").arg(sig.paramType);
    QString iterType;
    if (sig.paramType.startsWith("List<List<") && sig.paramType.endsWith(">>")) {
        iterType = "List<" + sig.paramType.mid(10, sig.paramType.length() - 12) + ">";
    } else if (sig.paramType.contains('<')) {
        iterType = "List<String>";
    } else {
        iterType = sig.paramType;
    }
    s << "    public void " << sig.method << "(" << paramType << " values) {\n";
    s << "        for (" << iterType << " value : values) {\n";
    s << "            System.out.println(value);\n";
    s << "        }\n";
    s << "        fail(\"Not implemented: " << sig.method << "\");\n";
    s << "    }\n";
    return out;
}

bool JavaGenerator::appendMissingStubs(const QString& gluePath,
                                        const QVector<GlueSig>& sigs,
                                        const SpectableFile& file,
                                        QStringList& msgs,
                                        const QString& framework)
{
    QFile f(gluePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString content = QTextStream(&f).readAll();
    f.close();

    QString stubs;
    for (const GlueSig& sig : sigs) {
        const QString signature = QStringLiteral("public void %1(").arg(sig.method);
        if (!content.contains(signature))
            stubs += "\n" + genStubMethod(sig, framework);
    }
    if (stubs.isEmpty()) return false;

    // Inject assertEquals import if a ValidValues DataType stub was added
    bool addedValidValues = false;
    for (const GlueSig& sig : sigs) {
        const QString signature = QStringLiteral("public void %1(").arg(sig.method);
        if (!sig.dataTypeName.isEmpty() && !content.contains(signature)) {
            addedValidValues = true; break;
        }
    }
    if (addedValidValues && !content.contains("assertEquals")) {
        const int failImport = content.indexOf("import static");
        if (failImport >= 0) {
            const int lineEnd = content.indexOf('\n', failImport);
            // Insert assertEquals import on the next line after the fail import
            const int nextLine = content.indexOf('\n', lineEnd + 1);
            const QString assertLine = content.mid(lineEnd + 1,
                content.indexOf('\n', lineEnd + 1) - lineEnd - 1);
            // Derive assertEquals import from the existing fail import line
            QString eqImport = content.mid(failImport,
                content.indexOf('\n', failImport) - failImport);
            eqImport.replace("fail", "assertEquals");
            content.insert(lineEnd + 1, eqImport + "\n");
        }
    }

    // Insert before the final closing "}\n" of the class
    const int closingClass = content.lastIndexOf("\n}");
    if (closingClass < 0) {
        msgs << QString("WARNING:0:Could not locate class closing brace in %1 — stubs not added")
                .arg(gluePath);
        return false;
    }

    content.insert(closingClass, "\n" + stubs);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

QString JavaGenerator::genGlueFile(const SpectableFile& file, const QString& specPkg,
                                    const QString& domainPkg, const QString& className) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    const QString glueClass = className + "_glue";
    QString out;
    QTextStream s(&out);

    if (!specPkg.isEmpty()) s << "package " << specPkg << ";\n\n";
    s << "import java.util.List;\n";
    s << "import java.util.ArrayList;\n";
    s << "import " << domainPkg << ".*;\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    const bool glueJUnit5 = m_framework.compare("TestNG", Qt::CaseInsensitive) != 0
                         && (m_framework.compare("JUnit", Qt::CaseInsensitive) == 0
                             || m_framework.toLower().startsWith("junit5")
                             || m_framework.toLower() == "junit 5");
    bool hasValidValuesStub = false;
    for (const GlueSig& g : sigs)
        if (!g.dataTypeName.isEmpty()) { hasValidValuesStub = true; break; }
    if (m_framework.compare("TestNG", Qt::CaseInsensitive) == 0) {
        s << "import static org.testng.Assert.fail;\n";
        if (hasValidValuesStub) s << "import static org.testng.Assert.assertEquals;\n";
        s << "\n";
    } else if (glueJUnit5) {
        s << "import static org.junit.jupiter.api.Assertions.fail;\n";
        if (hasValidValuesStub) s << "import static org.junit.jupiter.api.Assertions.assertEquals;\n";
        s << "\n";
    } else {
        s << "import static org.junit.Assert.fail;\n";
        if (hasValidValuesStub) s << "import static org.junit.Assert.assertEquals;\n";
        s << "\n";
    }
    s << "public class " << glueClass << " {\n";
    s << "    private static final String DNCString = \"?DNC?\";\n\n";

    for (const GlueSig& sig : sigs)
        s << genStubMethod(sig, m_framework) << "\n";

    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

static QString genYesNoClass(const QString& pkg, const QStringList& extraImports)
{
    QString out;
    QTextStream s(&out);
    s << "package " << pkg << ";\n\n";
    s << "import java.util.Objects;\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    s << "\n";
    s << "public class YesNo {\n";
    s << "    public final String value;\n\n";
    s << "    public YesNo(String value) {\n";
    s << "        this.value = value != null ? value : \"\";\n";
    s << "    }\n\n";
    s << "    public boolean toBoolean() {\n";
    s << "        return value.equalsIgnoreCase(\"true\")\n";
    s << "            || value.equalsIgnoreCase(\"t\")\n";
    s << "            || value.equalsIgnoreCase(\"yes\")\n";
    s << "            || value.equalsIgnoreCase(\"y\")\n";
    s << "            || value.equals(\"1\");\n";
    s << "    }\n\n";
    s << "    @Override\n";
    s << "    public String toString() { return value; }\n\n";
    s << "    @Override\n";
    s << "    public boolean equals(Object o) {\n";
    s << "        if (this == o) return true;\n";
    s << "        if (!(o instanceof YesNo)) return false;\n";
    s << "        return toBoolean() == ((YesNo) o).toBoolean();\n";
    s << "    }\n\n";
    s << "    @Override\n";
    s << "    public int hashCode() { return Objects.hash(toBoolean()); }\n";
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Production class generation helpers
// ---------------------------------------------------------------------------

// Infer a Java primitive type from ValidValues example values.
// Returns "long", "double", or "" (String-only, no second constructor).
static QString inferJavaType(const NamedBlock& nb)
{
    int valueCol = -1;
    for (int i = 0; i < nb.examples.header.size(); ++i)
        if (nb.examples.header[i].trimmed().compare("value", Qt::CaseInsensitive) == 0)
            { valueCol = i; break; }
    if (valueCol < 0 || nb.examples.rows.isEmpty()) return {};

    bool allLong = true, allDouble = true;
    for (const QStringList& row : nb.examples.rows) {
        if (valueCol >= row.size()) continue;
        const QString v = row[valueCol].trimmed();
        if (v.isEmpty()) continue;
        bool ok;
        v.toLongLong(&ok);  if (!ok) allLong = false;
        v.toDouble(&ok);    if (!ok) allDouble = false;
    }
    if (allLong)   return "long";
    if (allDouble) return "double";
    return {};
}

static QString genProductionClass(const NamedBlock& nb, const QString& pkg,
                                   const QStringList& extraImports)
{
    const QString name      = nb.name;
    const QString javaType  = inferJavaType(nb);
    QString out;
    QTextStream s(&out);
    s << "package " << pkg << ";\n\n";
    s << "import java.util.Objects;\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    s << "\npublic class " << name << " {\n";
    s << "    public final String value;\n\n";
    s << "    public " << name << "(String value) {\n";
    s << "        this.value = value != null ? value : \"\";\n";
    s << "    }\n";
    if (!javaType.isEmpty()) {
        const QString boxed = (javaType == "long") ? "Long" : "Double";
        s << "\n    public " << name << "(" << javaType << " numericValue) {\n";
        s << "        this.value = " << boxed << ".toString(numericValue);\n";
        s << "    }\n";
    }
    s << "\n    @Override\n";
    s << "    public boolean equals(Object o) {\n";
    s << "        if (this == o) return true;\n";
    s << "        if (!(o instanceof " << name << ")) return false;\n";
    s << "        return Objects.equals(value, ((" << name << ") o).value);\n";
    s << "    }\n\n";
    s << "    @Override\n";
    s << "    public int hashCode() { return Objects.hash(value); }\n\n";
    s << "    @Override\n";
    s << "    public String toString() { return \"" << name << "{\" + value + \"}\"; }\n";
    s << "}\n";
    return out;
}

static QString genProductionEnum(const NamedBlock& nb, const QString& pkg,
                                  const QStringList& extraImports)
{
    // Locate Value and optional Notes columns
    int valueCol = -1, notesCol = -1;
    for (int i = 0; i < nb.examples.header.size(); ++i) {
        const QString h = nb.examples.header[i].trimmed().toLower();
        if (h == "value") valueCol = i;
        if (h == "notes") notesCol = i;
    }

    // Collect constant name + optional description pairs
    struct Constant { QString name; QString note; };
    QList<Constant> constants;
    if (valueCol >= 0) {
        for (const QStringList& row : nb.examples.rows) {
            if (valueCol >= row.size()) continue;
            const QString v = row[valueCol].trimmed();
            if (v.isEmpty()) continue;
            const QString note = (notesCol >= 0 && notesCol < row.size())
                                 ? row[notesCol].trimmed() : QString();
            constants << Constant{v, note};
        }
    }

    const bool hasNotes = notesCol >= 0 && std::any_of(
        constants.begin(), constants.end(), [](const Constant& c){ return !c.note.isEmpty(); });

    const QString name = nb.name;
    QString out;
    QTextStream s(&out);
    s << "package " << pkg << ";\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    s << "\npublic enum " << name << " {\n";

    if (hasNotes) {
        for (int i = 0; i < constants.size(); ++i) {
            // Escape any double-quotes in the note
            QString escaped = constants[i].note;
            escaped.replace("\\", "\\\\").replace("\"", "\\\"");
            s << "    " << constants[i].name << "(\"" << escaped << "\")"
              << (i + 1 < constants.size() ? "," : ";") << "\n";
        }
        s << "\n";
        s << "    public final String description;\n\n";
        s << "    " << name << "(String description) { this.description = description; }\n";
    } else {
        for (int i = 0; i < constants.size(); ++i)
            s << "    " << constants[i].name
              << (i + 1 < constants.size() ? "," : "") << "\n";
    }

    s << "}\n";
    return out;
}

// Production field type: DataType/Entity/Collection names pass through; built-ins use javaType()
static QString prodFieldType(const Field& f, const SpectableFile& file)
{
    if (isCollectionType(f.type, file)) return f.type;      // Collection class → same name
    if (isAttrSetType(f.type, file))    return f.type;      // sub-entity → same name
    for (const QString& d : file.dataTypeNames)
        if (d.compare(f.type, Qt::CaseInsensitive) == 0) return f.type; // user DataType
    return JavaGenerator::javaType(f.type);                 // built-in
}

static QString genProductionCollection(const Collection& col, const QString& pkg)
{
    const QString className = col.name;
    const QString elemType  = col.elementType;
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << ";\n\n";
    s << "import java.util.ArrayList;\n";
    s << "import java.util.Collections;\n";
    s << "import java.util.List;\n";
    s << "\n";
    s << "public class " << className << " {\n";

    if (!col.minimum.isEmpty())
        s << "    public static final int MINIMUM = " << col.minimum << ";\n";
    if (!col.maximum.isEmpty())
        s << "    public static final int MAXIMUM = " << col.maximum << ";\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty())
        s << "\n";

    s << "    private final List<" << elemType << "> items = new ArrayList<>();\n\n";

    s << "    public void add(" << elemType << " item) {\n";
    s << "        items.add(item);\n";
    s << "    }\n\n";

    s << "    public boolean delete(" << elemType << " item) {\n";
    s << "        return items.remove(item);\n";
    s << "    }\n\n";

    s << "    public List<" << elemType << "> read() {\n";
    s << "        return Collections.unmodifiableList(items);\n";
    s << "    }\n\n";

    s << "    public boolean update(" << elemType << " oldItem, " << elemType << " newItem) {\n";
    s << "        int index = items.indexOf(oldItem);\n";
    s << "        if (index < 0) return false;\n";
    s << "        items.set(index, newItem);\n";
    s << "        return true;\n";
    s << "    }\n\n";

    s << "    public int size() {\n";
    s << "        return items.size();\n";
    s << "    }\n";

    s << "}\n";
    return out;
}

static QString genProductionEntity(const AttrSet& as, const QString& pkg,
                                    const SpectableFile& file)
{
    const QString name = as.name;
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << ";\n\n";
    s << "import java.util.Objects;\n\n";

    s << "public class " << name << " {\n";

    // Fields
    for (const Field& f : as.fields)
        s << "    public final " << prodFieldType(f, file) << " " << toCamelCase(f.name) << ";\n";
    s << "\n";

    // Constructor
    s << "    public " << name << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << prodFieldType(as.fields[i], file) << " " << toCamelCase(as.fields[i].name);
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "        this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "    }\n\n";

    // Builder inner class
    s << "    public static class Builder {\n";
    for (const Field& f : as.fields)
        s << "        private " << prodFieldType(f, file) << " " << toCamelCase(f.name) << ";\n";
    s << "\n";
    for (const Field& f : as.fields) {
        const QString fn = toCamelCase(f.name);
        const QString ft = prodFieldType(f, file);
        s << "        public Builder " << fn << "(" << ft << " " << fn << ") "
          << "{ this." << fn << " = " << fn << "; return this; }\n";
    }
    s << "\n        public " << name << " build() {\n";
    s << "            return new " << name << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << toCamelCase(as.fields[i].name);
    }
    s << ");\n        }\n    }\n\n";

    // equals()
    s << "    @Override\n    public boolean equals(Object o) {\n";
    s << "        if (this == o) return true;\n";
    s << "        if (!(o instanceof " << name << ")) return false;\n";
    s << "        " << name << " that = (" << name << ") o;\n";
    s << "        return ";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << "\n            && ";
        const QString fn = toCamelCase(as.fields[i].name);
        s << "Objects.equals(" << fn << ", that." << fn << ")";
    }
    s << ";\n    }\n\n";

    // hashCode()
    s << "    @Override\n    public int hashCode() {\n        return Objects.hash(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << toCamelCase(as.fields[i].name);
    }
    s << ");\n    }\n\n";

    // toString()
    s << "    @Override\n    public String toString() {\n        return \"" << name << "{\"";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << " + \", \"";
        const QString fn = toCamelCase(as.fields[i].name);
        s << " + \"" << as.fields[i].name << "=\" + " << fn;
    }
    s << " + \"}\";\n    }\n}\n";

    return out;
}

static QString genProductionHelper(const QVector<AttrSet>& entities, const QString& prodPkg,
                                    const QString& commonPkg, const SpectableFile& file)
{
    QString out;
    QTextStream s(&out);

    s << "package " << commonPkg << ";\n\n";
    s << "import " << prodPkg << ".*;\n";
    s << "import java.util.ArrayList;\n";
    s << "import java.util.List;\n\n";

    s << "public class ProductionHelper {\n\n";

    for (const AttrSet& as : entities) {
        const QString en = as.name;
        const QString tn = en + "Typed";

        // Determine if any field is a Collection (requires multi-statement setup)
        bool hasCollection = false;
        for (const Field& f : as.fields)
            if (isCollectionType(f.type, file)) { hasCollection = true; break; }

        // TypedToProduction
        s << "    public static " << en << " " << en << "TypedToProduction(" << tn << " t) {\n";
        if (hasCollection) {
            // Emit local Collection variables, then construct
            for (const Field& f : as.fields) {
                if (!isCollectionType(f.type, file)) continue;
                const QString fn  = toCamelCase(f.name);
                const QString elem = collectionElementType(f.type, file);
                s << "        " << f.type << " " << fn << " = new " << f.type << "();\n";
                s << "        for (" << elem << " _item : " << elem << "TypedListToProduction(t." << fn << "))"
                  << " " << fn << ".add(_item);\n";
            }
            s << "        return new " << en << "(";
            for (int i = 0; i < as.fields.size(); ++i) {
                if (i) s << ",\n            ";
                const QString fn = toCamelCase(as.fields[i].name);
                if (isCollectionType(as.fields[i].type, file))
                    s << fn;  // local variable already built above
                else if (isAttrSetType(as.fields[i].type, file))
                    s << as.fields[i].type << "TypedToProduction(t." << fn << ")";
                else
                    s << "t." << fn;
            }
            s << ");\n";
        } else {
            s << "        return new " << en << "(";
            for (int i = 0; i < as.fields.size(); ++i) {
                if (i) s << ",\n            ";
                const QString fn = toCamelCase(as.fields[i].name);
                if (isAttrSetType(as.fields[i].type, file))
                    s << as.fields[i].type << "TypedToProduction(t." << fn << ")";
                else
                    s << "t." << fn;
            }
            s << ");\n";
        }
        s << "    }\n\n";

        // ProductionToTyped
        s << "    public static " << tn << " " << en << "ProductionToTyped(" << en << " p) {\n";
        s << "        return new " << tn << "(";
        for (int i = 0; i < as.fields.size(); ++i) {
            if (i) s << ",\n            ";
            const QString fn = toCamelCase(as.fields[i].name);
            if (isCollectionType(as.fields[i].type, file)) {
                const QString elem = collectionElementType(as.fields[i].type, file);
                s << elem << "ProductionToTypedList(p." << fn << ".read())";
            } else if (isAttrSetType(as.fields[i].type, file)) {
                s << as.fields[i].type << "ProductionToTyped(p." << fn << ")";
            } else {
                s << "p." << fn;
            }
        }
        s << ");\n    }\n\n";

        // List TypedToProduction
        s << "    public static List<" << en << "> " << en << "TypedListToProduction(List<" << tn << "> list) {\n";
        s << "        List<" << en << "> result = new ArrayList<>();\n";
        s << "        for (" << tn << " t : list) result.add(" << en << "TypedToProduction(t));\n";
        s << "        return result;\n    }\n\n";

        // List ProductionToTyped
        s << "    public static List<" << tn << "> " << en << "ProductionToTypedList(List<" << en << "> list) {\n";
        s << "        List<" << tn << "> result = new ArrayList<>();\n";
        s << "        for (" << en << " p : list) result.add(" << en << "ProductionToTyped(p));\n";
        s << "        return result;\n    }\n\n";
    }

    s << "}\n";
    return out;
}

bool JavaGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        msgs << QString("ERROR:0:Cannot write file: %1").arg(path);
        return false;
    }
    QTextStream out(&f);
    out << content;
    return true;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

QStringList JavaGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_framework     = opts.framework;
    m_extraImports  = opts.extraImports;
    m_tagFilter     = opts.tagFilter;
    m_failEveryTest = opts.failEveryTest;

    // Inject production package import into Typed/glue files whenever the package is configured
    if (!opts.productionClassesPackage.isEmpty()) {
        const QString prodImport = "import " + opts.productionClassesPackage + ".*;";
        if (!m_extraImports.contains(prodImport))
            m_extraImports.prepend(prodImport);
    }

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    // Item 6: validate field types in every Attributes/Entity declaration
    for (const AttrSet& as : file.attrSets) {
        if (as.isContext) continue;
        for (const Field& f : as.fields) {
            if (f.type.isEmpty())
                msgs << QString("WARNING:%1:Field '%2' in '%3' has no type specified")
                        .arg(as.line).arg(f.name).arg(as.name);
            else if (!isDataType(f.type, file) && !isAttrSetType(f.type, file) && !isCollectionType(f.type, file))
                msgs << QString("WARNING:%1:Field '%2' in '%3' has unknown type '%4'")
                        .arg(as.line).arg(f.name).arg(as.name).arg(f.type);
        }
    }

    // Item 11: warn if Specification name doesn't match the filename
    if (!file.specName.isEmpty() && !file.filePath.isEmpty()) {
        const QString baseName  = QFileInfo(file.filePath).completeBaseName();
        const QString specFlat  = file.specName.simplified().remove(' ').toLower();
        const QString fileFlat  = baseName.simplified().remove(' ').toLower();
        if (specFlat != fileFlat)
            msgs << QString("INFO:0:Specification name '%1' does not match file name '%2'")
                    .arg(file.specName, baseName);
    }

    const QString className   = toClassName(file.specName);
    const QString domainPkg   = joinPkg(opts.packagePrefix, "common");

    QString specPkg;
    QString specSubDir;
    if (!opts.sourceRoot.isEmpty() && !file.filePath.isEmpty()) {
        const QDir    srcDir(QFileInfo(opts.sourceRoot).absoluteFilePath());
        const QString fileAbsDir = QFileInfo(file.filePath).absoluteDir().absolutePath();
        QString relPath = srcDir.relativeFilePath(fileAbsDir);
        if (relPath == "." || relPath.isEmpty()) {
            specPkg = opts.packagePrefix;
        } else {
            QStringList parts;
            for (const QString& p : relPath.split('/'))
                if (!p.isEmpty() && p != "..") parts << p.toLower().replace('-', '_');
            specPkg = parts.isEmpty() ? opts.packagePrefix
                                      : joinPkg(opts.packagePrefix, parts.join('.'));
            specSubDir = parts.join('/');
        }
    } else {
        specPkg = opts.packagePrefix;
    }
    const QString testPkg = specPkg.isEmpty() ? QString() : joinPkg(specPkg, "tests");

    QDir dir(specSubDir.isEmpty() ? opts.outputDir : opts.outputDir + "/" + specSubDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create output directory: %1").arg(dir.path());
        return msgs;
    }

    // Domain classes go into outputDir/common/
    QDir domainDir(opts.outputDir + "/common");
    if (!domainDir.exists() && !domainDir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create common directory: %1").arg(domainDir.path());
        return msgs;
    }

    writeFile(domainDir.filePath("YesNo.java"), genYesNoClass(domainPkg, m_extraImports), msgs);

    // Copy the source .spectable file into the output folder (if enabled)
    if (opts.copySpectable && !file.filePath.isEmpty()) {
        const QString destPath = dir.filePath(QFileInfo(file.filePath).fileName());
        QFile::remove(destPath);
        if (!QFile::copy(file.filePath, destPath))
            msgs << QString("WARNING:0:Could not copy %1 to %2").arg(file.filePath, destPath);
    }

    // Synthesize AttrSets for built-in Examples names (EnumerationValues, ValidValues, etc.)
    // referenced in NamedBlocks but not declared with an explicit Attributes block.
    SpectableFile augmented = file;
    {
        QSet<QString> knownNames;
        for (const AttrSet& as : file.attrSets)
            knownNames.insert(as.name.toLower());

        for (const NamedBlock& nb : file.namedBlocks) {
            const QString asName = nb.examples.attrSetName.trimmed();
            if (asName.isEmpty() || nb.examples.header.isEmpty()) continue;
            if (isDataType(asName, file)) continue;
            if (knownNames.contains(asName.toLower())) continue;
            knownNames.insert(asName.toLower());

            AttrSet sa;
            sa.name = asName;
            const bool isValidValues = asName.compare("ValidValues", Qt::CaseInsensitive) == 0;
            const bool isEnumValues  = asName.compare("EnumerationValues", Qt::CaseInsensitive) == 0;
            for (const QString& col : nb.examples.header) {
                const QString c = col.trimmed();
                if (!c.isEmpty()) {
                    Field f; f.name = c;
                    f.type = (isValidValues && c.compare("isvalid", Qt::CaseInsensitive) == 0)
                             ? "YesNo" : "String";
                    sa.fields.push_back(f);
                }
            }
            // Always include Notes for ValidValues / EnumerationValues so the
            // constructor signature is stable even when the column is omitted.
            if (isValidValues || isEnumValues) {
                bool hasNotes = false;
                for (const Field& f : sa.fields)
                    if (f.name.compare("notes", Qt::CaseInsensitive) == 0) { hasNotes = true; break; }
                if (!hasNotes) {
                    Field nf; nf.name = "Notes"; nf.type = "String"; nf.defaultValue = "";
                    sa.fields.push_back(nf);
                }
            }
            if (!sa.fields.isEmpty())
                augmented.attrSets.push_back(sa);
        }
    }

    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        writeFile(domainDir.filePath(as.name + "String.java"), genStringClass(as, domainPkg, m_extraImports, msgs, file), msgs);
        writeFile(domainDir.filePath(as.name + "Typed.java"),  genTypedClass(as, domainPkg, m_extraImports, msgs, file),  msgs);
    }

    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, testPkg, specPkg, domainPkg, className, testErrs);
        msgs << testErrs;
        const bool testHasErrors = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!testHasErrors)
            writeFile(dir.filePath(className + "_Test.java"), testContent, msgs);
    }

    {
        const QString gluePath = dir.filePath(className + "_glue.java");
        QStringList sigConflicts;
        const QVector<GlueSig> sigs = collectGlueSigs(augmented, &sigConflicts);
        msgs << sigConflicts;
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, specPkg, domainPkg, className), msgs);
        } else {
            if (appendMissingStubs(gluePath, sigs, augmented, msgs, m_framework))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // Generate TableHelper.java in common/ with toListListXxx static converters
    {
        const QVector<GlueSig> sigs = collectGlueSigs(augmented);
        const QString helperContent = genTableHelperClass(sigs, domainPkg, augmented);
        if (!helperContent.isEmpty())
            writeFile(domainDir.filePath("TableHelper.java"), helperContent, msgs);
    }

    // Production class generation
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        const QString prodPkg = opts.productionClassesPackage;
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists()) prodDir.mkpath(".");

        // DataType → production class / enum. Covers EnumerationValues (enum),
        // ValidValues (wrapper class + inferred numeric ctor), and bare DataTypes
        // with no Examples table at all (plain String-constructor wrapper) — the
        // last case previously generated nothing, even though Typed-class codegen
        // assumes a class with that name exists (parseExpr's "new TypeName(ref)").
        for (const NamedBlock& nb : file.namedBlocks) {
            if (nb.isContext || nb.kind.compare("DataType", Qt::CaseInsensitive) != 0) continue;
            const bool isEnum = nb.hasExamples &&
                nb.examples.attrSetName.compare("EnumerationValues", Qt::CaseInsensitive) == 0;

            const QString prodPath = prodDir.filePath(nb.name + ".java");
            if (QFile::exists(prodPath)) continue;   // never overwrite existing production classes

            if (isEnum)
                writeFile(prodPath, genProductionEnum(nb, prodPkg, {}), msgs);
            else
                writeFile(prodPath, genProductionClass(nb, prodPkg, {}), msgs);
        }

        // Entity → production class with Builder (only if file does not exist)
        QVector<AttrSet> prodEntities;
        for (const AttrSet& as : file.attrSets) {
            if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
            prodEntities << as;
            const QString prodPath = prodDir.filePath(as.name + ".java");
            if (QFile::exists(prodPath)) continue;
            writeFile(prodPath, genProductionEntity(as, prodPkg, file), msgs);
        }

        // Collection → production class with add/delete/read/update (only if file does not exist)
        for (const Collection& col : file.collections) {
            if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
            const QString prodPath = prodDir.filePath(col.name + ".java");
            if (QFile::exists(prodPath)) continue;
            writeFile(prodPath, genProductionCollection(col, prodPkg), msgs);
        }

        // ProductionHelper in common/ (only if file does not exist)
        if (!prodEntities.isEmpty() && !prodPkg.isEmpty()) {
            const QString helperPath = domainDir.filePath("ProductionHelper.java");
            if (!QFile::exists(helperPath))
                writeFile(helperPath, genProductionHelper(prodEntities, prodPkg, domainPkg, file), msgs);
        }
    }

    return msgs;
}
