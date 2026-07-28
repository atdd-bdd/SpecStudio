#include "JavaScriptGenerator.h"
#include "TagFilter.h"
#include "SourceScan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <functional>

// ---------------------------------------------------------------------------
// Cell value helpers
// ---------------------------------------------------------------------------

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

static QString jsEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('`',  "\\`");
    r.replace('$',  "\\$");
    return r;
}

static QString jsStringEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('"',  "\\\"");
    return r;
}

// ---------------------------------------------------------------------------
// Type helpers
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::jsDefaultValue(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")        return "0";
    if (t == "float"   || t == "decimal" || t == "scientific")    return "0.0";
    if (t == "boolean" || t == "yesno"
     || t == "bool")                         return "false";
    // strings, date/time, user-defined
    return "\"\"";
}

bool JavaScriptGenerator::isAttrSetType(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

// A cell for a nested-object field is written "=SomeDefine"; expand that define
// against the nested Attributes block and build its String constructor call.
QString JavaScriptGenerator::nestedLiteral(const QString& cellValue, const QString& fieldType,
                                           const SpectableFile& file)
{
    for (const AttrSet& subAs : file.attrSets) {
        if (subAs.name.compare(fieldType, Qt::CaseInsensitive) != 0) continue;

        QStringList row(subAs.fields.size());
        for (int i = 0; i < subAs.fields.size(); ++i)
            row[i] = subAs.fields[i].defaultValue;

        if (cellValue.startsWith('=')) {
            const QString defineName = cellValue.mid(1).trimmed();
            for (const Define& d : file.defines) {
                if (d.name.compare(defineName, Qt::CaseInsensitive) != 0 || !d.isTable)
                    continue;
                QMap<QString, int> fieldIdx;
                for (int i = 0; i < subAs.fields.size(); ++i)
                    fieldIdx[subAs.fields[i].name.toLower()] = i;
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
                break;
            }
        }
        return stringLiteral(subAs, row, file);
    }
    return "\"" + jsStringEscape(cellValue) + "\"";
}

// A constructor call for one row, used when the block has a nested-object field.
QString JavaScriptGenerator::stringLiteral(const AttrSet& as, const QStringList& row,
                                           const SpectableFile& file)
{
    QString expr = "new " + as.name + "String(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) expr += ", ";
        const QString cell = (i < row.size()) ? row[i] : QString();
        if (isAttrSetType(as.fields[i].type, file))
            expr += nestedLiteral(cell, as.fields[i].type, file);
        else
            expr += "\"" + jsStringEscape(cell) + "\"";
    }
    return expr + ")";
}

QString JavaScriptGenerator::parseExpr(const QString& field, const QString& specType,
                                       const SpectableFile* file)
{
    if (file && isAttrSetType(specType.trimmed(), *file)) {
        // Nested Attributes block — build its own Typed object.
        for (const AttrSet& as : file->attrSets)
            if (as.name.compare(specType.trimmed(), Qt::CaseInsensitive) == 0)
                return QString("%1Typed.fromStringObj(s.%2)").arg(as.name, field);
    }
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("s.%1 !== \"\" ? Number(s.%1) : 0").arg(field);
    if (t == "float" || t == "decimal" || t == "scientific")
        return QString("s.%1 !== \"\" ? Number(s.%1) : 0.0").arg(field);
    if (t == "boolean" || t == "yesno" || t == "bool")
        return QString("[\"true\",\"t\",\"yes\",\"y\",\"1\"].includes(s.%1.toLowerCase())").arg(field);
    // A user DataType lives in the production folder, which common must not
    // depend on, so its value is carried as text — the glue converts it when it
    // needs the production object.
    return QString("s.%1").arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::toCamelCase(const QString& name)
{
    const QStringList parts = name.split(QRegularExpression(R"([\s_]+)"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return name;
    QString result = parts[0][0].toLower() + parts[0].mid(1);
    for (int i = 1; i < parts.size(); ++i)
        if (!parts[i].isEmpty())
            result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

QString JavaScriptGenerator::toPascalCase(const QString& name)
{
    QString s = name.trimmed();
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), " ");
    const QStringList parts = s.split(' ', Qt::SkipEmptyParts);
    QString result;
    for (const QString& p : parts)
        if (!p.isEmpty()) result += p[0].toUpper() + p.mid(1);
    if (!result.isEmpty() && result[0].isDigit()) result.prepend('_');
    return result;
}

QString JavaScriptGenerator::toMethodName(const QString& keyword, const QString& stepText)
{
    // "Given" + "check account balance" → "givenCheckAccountBalance"
    QString combined = keyword + "_" + stepText;
    combined.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    combined.remove(QRegularExpression("^_+|_+$"));
    const QStringList parts = combined.split('_', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return combined.toLower();
    QString result = parts[0].toLower();
    for (int i = 1; i < parts.size(); ++i)
        if (!parts[i].isEmpty())
            result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

QString JavaScriptGenerator::toFileName(const QString& name)
{
    // "MySpecName" → "mySpecName"
    if (name.isEmpty()) return name;
    return name[0].toLower() + name.mid(1);
}

// ---------------------------------------------------------------------------
// Collection helpers
// ---------------------------------------------------------------------------

bool JavaScriptGenerator::isCollectionType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

QString JavaScriptGenerator::collectionElementType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return c.elementType;
    return {};
}

// ---------------------------------------------------------------------------
// DataType detection
// ---------------------------------------------------------------------------

bool JavaScriptGenerator::isDataType(const QString& name, const SpectableFile& file)
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

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

const AttrSet* JavaScriptGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return &as;
    return nullptr;
}

const Define* JavaScriptGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Row resolution
// ---------------------------------------------------------------------------

QVector<QStringList> JavaScriptGenerator::resolveStepRows(
    const Step& step, const AttrSet* attrSet,
    const SpectableFile& file, QStringList& errors)
{
    QVector<QStringList> result;
    if (!attrSet) return result;

    const int fieldCount = attrSet->fields.size();

    // A column the table leaves out takes the field's Default, or the
    // Do-Not-Care marker when the step is CompareOnly.
    auto initRow = [&]() {
        QStringList row(fieldCount);
        for (int i = 0; i < fieldCount; ++i)
            row[i] = step.compareOnly ? QStringLiteral("?DNC?")
                                      : attrSet->fields[i].defaultValue;
        return row;
    };

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

        if (def->vertical || step.vertical) {
            QStringList row = initRow();
            const int start = def->vertical ? 1 : 0;
            for (int ri = start; ri < def->tableRows.size(); ++ri) {
                const QStringList& r = def->tableRows[ri];
                if (r.size() < 2) continue;
                const QString key = r[0].toLower();
                if (fieldIdx.contains(key)) row[fieldIdx[key]] = resolveValue(r[1], file);
            }
            result << row;
        } else {
            if (def->tableRows.isEmpty()) return result;
            const QStringList& hdrs = def->tableRows[0];
            QVector<int> colMap;
            for (const QString& h : hdrs)
                colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);
            for (int ri = 1; ri < def->tableRows.size(); ++ri) {
                QStringList row = initRow();
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

    if (step.table.vertical) {
        int numCols = 0;
        for (const QStringList& r : step.table.rows)
            if (r.size() > numCols) numCols = r.size();
        for (int col = 1; col < numCols; ++col) {
            QStringList row = initRow();
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
        for (int ri = 1; ri < step.table.rows.size(); ++ri) {
            QStringList row = initRow();
            const QStringList& dr = step.table.rows[ri];
            for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
                if (colMap[ci] >= 0) row[colMap[ci]] = resolveValue(dr[ci], file);
            result << row;
        }
    }
    return result;
}

QVector<QStringList> JavaScriptGenerator::resolveExamplesRows(
    const NamedBlock& nb, const AttrSet* as)
{
    QVector<QStringList> result;
    if (nb.examples.header.isEmpty() && nb.examples.rows.isEmpty())
        return result;
    if (!as) {
        result = nb.examples.rows;
        return result;
    }
    const int fieldCount = as->fields.size();
    QMap<QString, int> fieldIdx;
    for (int i = 0; i < as->fields.size(); ++i)
        fieldIdx[as->fields[i].name.toLower()] = i;
    QVector<int> colMap;
    for (const QString& h : nb.examples.header)
        colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);
    for (const QStringList& dr : nb.examples.rows) {
        QStringList row(fieldCount);
        for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
            if (colMap[ci] >= 0) row[colMap[ci]] = resolveCell(dr[ci]);
        result << row;
    }
    return result;
}

// ---------------------------------------------------------------------------
// String class generator
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genStringClass(const AttrSet& as, const SpectableFile& file) const
{
    const QString cn = as.name + "String";
    QString out;
    QTextStream s(&out);

    // A nested Attributes block is held as that block's own String object.
    QSet<QString> seenImports;
    for (const Field& f : as.fields) {
        if (!isAttrSetType(f.type, file)) continue;
        if (seenImports.contains(f.type.trimmed().toLower())) continue;
        seenImports.insert(f.type.trimmed().toLower());
        for (const AttrSet& sub : file.attrSets)
            if (sub.name.compare(f.type, Qt::CaseInsensitive) == 0)
                s << "import { " << sub.name << "String } from \"./"
                  << sub.name << "String.js\";\n";
    }
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";
    s << "export class " << cn << " {\n";
    s << "  static DNC_STRING = \"?DNC?\";\n\n";
    s << "  constructor(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << toCamelCase(as.fields[i].name) << " = \"\"";
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "    this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "  }\n\n";

    s << "  static fromList(values) {\n";
    s << "    const v = Array.from(values);\n";
    s << "    return new " << cn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        s << "      v[" << i << "] ?? \"\"";
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "    );\n  }\n\n";

    s << "  toString() {\n";
    s << "    return `";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString fn = toCamelCase(as.fields[i].name);
        s << as.fields[i].name << "=${this." << fn << "}";
    }
    s << "`;\n  }\n\n";

    s << genEqualsMethod(as, cn, file, /*dncAware=*/true);
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Value equality. On the String class a field holding the Do-Not-Care marker on
// either side matches whatever the other side holds — that is what lets a
// CompareOnly step name only the columns it cares about.
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genEqualsMethod(const AttrSet& as, const QString& cn,
                                              const SpectableFile& file,
                                              bool dncAware) const
{
    QString out;
    QTextStream s(&out);

    s << "  equals(other) {\n";
    s << "    if (!(other instanceof " << cn << ")) return false;\n";
    if (as.fields.isEmpty()) {
        s << "    return true;\n";
    } else {
        s << "    return ";
        for (int i = 0; i < as.fields.size(); ++i) {
            const Field& f  = as.fields[i];
            const QString fn = toCamelCase(f.name);
            if (i) s << "\n      && ";
            if (isAttrSetType(f.type, file))
                s << "this." << fn << ".equals(other." << fn << ")";
            else if (dncAware)
                s << "(this." << fn << " === " << cn << ".DNC_STRING"
                  << " || other." << fn << " === " << cn << ".DNC_STRING"
                  << " || this." << fn << " === other." << fn << ")";
            else
                s << "this." << fn << " === other." << fn;
        }
        s << ";\n";
    }
    s << "  }\n";

    if (dncAware) {
        s << "\n  static equalLists(a, b) {\n";
        s << "    if (a.length !== b.length) return false;\n";
        s << "    return a.every((row, i) => row.equals(b[i]));\n";
        s << "  }\n";
    }
    return out;
}

// ---------------------------------------------------------------------------
// Typed class generator
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genTypedClass(const AttrSet& as, const SpectableFile& file) const
{
    const QString cn   = as.name + "Typed";
    const QString scn  = as.name + "String";
    const QString sFile = as.name + "String.js";
    QString out;
    QTextStream s(&out);

    s << "import { " << scn << " } from \"./" << sFile << "\";\n";
    s << "import * as _json from \"./json.js\";\n";
    // A nested Attributes block is held as that block's own Typed object.
    QSet<QString> seenTypedImports;
    for (const Field& f : as.fields) {
        if (!isAttrSetType(f.type, file)) continue;
        if (seenTypedImports.contains(f.type.trimmed().toLower())) continue;
        seenTypedImports.insert(f.type.trimmed().toLower());
        for (const AttrSet& sub : file.attrSets)
            if (sub.name.compare(f.type, Qt::CaseInsensitive) == 0)
                s << "import { " << sub.name << "Typed } from \"./"
                  << sub.name << "Typed.js\";\n";
    }
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";
    s << "export class " << cn << " {\n";
    s << "  constructor(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        const QString fn = toCamelCase(f.name);
        s << fn << " = " << jsDefaultValue(f.type);
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "    this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "  }\n\n";

    s << "  static fromStringObj(s) {\n";
    s << "    return new " << cn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f  = as.fields[i];
        const QString fn = toCamelCase(f.name);
        s << "      " << parseExpr(fn, f.type, &file);
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "    );\n  }\n\n";

    // ---- JSON (built-in JSON object; no package dependency) ----

    // toJsonValue() builds a graph of primitives only, so the toJSON() name
    // below can never cause JSON.stringify to double-encode a nested object.
    s << "  toJsonValue() {\n";
    s << "    return {\n";
    for (const Field& f : as.fields) {
        const QString fn = toCamelCase(f.name);
        const QString t  = f.type.trimmed().toLower();
        const bool isNum  = (t == "integer" || t == "int" || t == "float"
                          || t == "decimal" || t == "scientific");
        const bool isBool = (t == "boolean" || t == "yesno" || t == "bool");
        const bool isStr  = (t == "string" || t == "text" || t == "character" || t == "char"
                          || t == "date"   || t == "time" || t == "datetime"  || t == "duration");
        if (isNum || isBool || isStr)
            s << "      " << fn << ": this." << fn << ",\n";
        else    // user-defined type — same string convention fromStringObj uses
            s << "      " << fn << ": this." << fn << " == null ? null : String(this." << fn << "),\n";
    }
    s << "    };\n";
    s << "  }\n\n";

    s << "  toJSON() { return _json.stringify(this.toJsonValue()); }\n\n";

    s << "  static fromJsonValue(m) {\n";
    s << "    return new " << cn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f   = as.fields[i];
        const QString fn = toCamelCase(f.name);
        const QString t  = f.type.trimmed().toLower();
        const QString src = QString("_json.require(m, \"%1\")").arg(fn);
        QString expr;
        if (t == "integer" || t == "int")
            expr = QString("_json.asInt(%1, \"%2\")").arg(src, fn);
        else if (t == "float" || t == "decimal" || t == "scientific")
            expr = QString("_json.asNumber(%1, \"%2\")").arg(src, fn);
        else if (t == "boolean" || t == "yesno" || t == "bool")
            expr = QString("_json.asBool(%1, \"%2\")").arg(src, fn);
        else if (t == "string" || t == "text" || t == "character" || t == "char"
              || t == "date"   || t == "time" || t == "datetime"  || t == "duration")
            expr = QString("_json.asString(%1, \"%2\")").arg(src, fn);
        else
            expr = QString("new %1(_json.asString(%2, \"%3\"))").arg(f.type.trimmed(), src, fn);
        s << "      " << expr;
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "    );\n  }\n\n";

    s << "  static fromJSON(text) { return " << cn << ".fromJsonValue(_json.parse(text)); }\n\n";

    s << "  static toJSONList(items) {\n";
    s << "    return _json.stringify(items.map((item) => item.toJsonValue()));\n";
    s << "  }\n\n";

    s << "  static fromJSONList(text) {\n";
    s << "    const raw = _json.asArray(_json.parse(text), \"" << cn << "\");\n";
    s << "    return raw.map((e) => " << cn << ".fromJsonValue(e));\n";
    s << "  }\n\n";

    s << "  toString() {\n";
    s << "    return `";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << as.fields[i].name << "=${this." << toCamelCase(as.fields[i].name) << "}";
    }
    s << "`;\n  }\n\n";

    s << genEqualsMethod(as, cn, file, /*dncAware=*/false);
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Common index.js
// ---------------------------------------------------------------------------

// Every .spectable in a project generates into the same common folder, and this
// index is rewritten on each one. Emitting only the current file's AttrSets meant
// the last file processed erased every class contributed by the others, so the
// re-export barrel resolved almost nothing. The existing index is therefore
// merged with the new entries instead of replaced.
QString JavaScriptGenerator::genCommonIndex(const QVector<AttrSet>& attrSets,
                                             const QString& existing,
                                             const QString& dir) const
{
    QStringList lines;
    for (const QString& line : existing.split('\n')) {
        const QString t = line.trimmed();
        // json.js is re-exported by the fixed header below; carrying it over
        // too emitted the line twice from the second generation onward.
        if (t == "export * from \"./json.js\";") continue;
        if (t.startsWith("export * from") && !lines.contains(t)) lines << t;
    }
    lines = sourcescan::dropEntriesForMissingFiles(
                lines, dir, QRegularExpression(R"(from "\./([A-Za-z0-9_]+)\.js")"), ".js");
    for (const AttrSet& as : attrSets) {
        for (const QString& l : { QString("export * from \"./%1String.js\";").arg(as.name),
                                  QString("export * from \"./%1Typed.js\";").arg(as.name) })
            if (!lines.contains(l)) lines << l;
    }
    lines.sort();

    QString out;
    QTextStream s(&out);
    s << "export * from \"./json.js\";\n";
    for (const QString& l : lines) s << l << "\n";
    return out;
}

// ---------------------------------------------------------------------------
// common/json.js — typed field accessors over the language's built-in JSON.
// No package dependency: JSON.parse / JSON.stringify are part of ECMAScript.
// ---------------------------------------------------------------------------

static QString genJsonModule()
{
    return QString::fromLatin1(R"JS(// Typed field accessors layered over the built-in JSON object.
//
// A missing key or a value of the wrong type throws a TypeError.  An explicit
// JSON null is passed through as null rather than treated as an error.

export function parse(text) {
  try {
    return JSON.parse(text);
  } catch (e) {
    throw new TypeError(`Invalid JSON: ${e.message}`);
  }
}

export function stringify(value) {
  return JSON.stringify(value);
}

function describe(value) {
  if (value === null || value === undefined) return "null";
  if (Array.isArray(value)) return "an array";
  switch (typeof value) {
    case "boolean": return "a boolean";
    case "number":  return "a number";
    case "string":  return "a string";
    case "object":  return "an object";
    default:        return typeof value;
  }
}

function typeError(ctx, expected, value) {
  return new TypeError(`JSON field '${ctx}' is not ${expected} (got ${describe(value)})`);
}

export function require(obj, key) {
  if (obj === null || typeof obj !== "object" || Array.isArray(obj))
    throw new TypeError(`Expected an object holding field '${key}'`);
  if (!Object.prototype.hasOwnProperty.call(obj, key))
    throw new TypeError(`Missing JSON field '${key}'`);
  return obj[key];
}

export function asString(value, ctx) {
  if (value === null || value === undefined) return null;
  if (typeof value === "string") return value;
  if (typeof value === "number" || typeof value === "boolean") return String(value);
  throw typeError(ctx, "a string", value);
}

export function asNumber(value, ctx) {
  if (typeof value === "number" && Number.isFinite(value)) return value;
  if (typeof value === "string" && value.trim() !== "") {
    const n = Number(value);
    if (Number.isFinite(n)) return n;
  }
  throw typeError(ctx, "a number", value);
}

export function asInt(value, ctx) {
  const n = asNumber(value, ctx);
  if (!Number.isInteger(n)) throw typeError(ctx, "an integer", value);
  return n;
}

export function asBool(value, ctx) {
  if (typeof value === "boolean") return value;
  if (typeof value === "string") {
    const low = value.trim().toLowerCase();
    if (["true", "t", "yes", "y", "1"].includes(low)) return true;
    if (["false", "f", "no", "n", "0"].includes(low)) return false;
  }
  throw typeError(ctx, "a boolean", value);
}

export function asArray(value, ctx) {
  if (value === null || value === undefined) return null;
  if (Array.isArray(value)) return value;
  throw typeError(ctx, "an array", value);
}
)JS");
}

// ---------------------------------------------------------------------------
// Test file generator
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genTestFile(const SpectableFile& file, const QString& specName,
                                          const QString& glueClass, const QString& commonRelPath,
                                          QStringList& errors) const
{
    const QString glueFile = toFileName(specName) + "_glue.js";
    QString out;
    QTextStream s(&out);

    s << "import { ";
    // collect all String types used
    QSet<QString> usedTypes;
    // A block with a nested-object field is emitted as a literal that names the
    // nested String class too, so the import list has to reach those as well.
    std::function<void(const QString&)> addWithNested = [&](const QString& name) {
        if (name.isEmpty() || isDataType(name, file)) return;
        if (usedTypes.contains(name + "String")) return;
        usedTypes.insert(name + "String");
        for (const AttrSet& as : file.attrSets) {
            if (as.name.compare(name, Qt::CaseInsensitive) != 0) continue;
            for (const Field& f : as.fields)
                if (isAttrSetType(f.type, file)) addWithNested(f.type.trimmed());
        }
    };

    auto collectUsedTypes = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            if (step.attrSetName.isEmpty()) continue;
            const QString effectiveName = isCollectionType(step.attrSetName, file)
                ? collectionElementType(step.attrSetName, file)
                : step.attrSetName;
            addWithNested(effectiveName);
        }
    };
    collectUsedTypes(file.backgroundSteps);
    collectUsedTypes(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios) collectUsedTypes(sc.steps);
    for (const NamedBlock& nb : file.namedBlocks)
        addWithNested(nb.examples.attrSetName);

    QStringList typeList = usedTypes.values();
    std::sort(typeList.begin(), typeList.end());
    s << typeList.join(", ");
    s << " } from \"" << commonRelPath << "/index.js\";\n";
    s << "import { " << glueClass << " } from \"./" << glueFile << "\";\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";

    int objectCounter = 0;

    auto emitSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            const QString meth = toMethodName(step.keyword, step.text);

            if (step.hasDocString) {
                QString esc = step.docString;
                esc.replace("\\", "\\\\");
                esc.replace("\"", "\\\"");
                esc.replace("\n", "\\n");
                s << "    glue." << meth << "(\"" << esc << "\");\n";
                continue;
            }
            if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                if (def && def->hasDocString) {
                    QString esc = def->docString;
                    esc.replace("\\", "\\\\");
                    esc.replace("\"", "\\\"");
                    esc.replace("\n", "\\n");
                    s << "    glue." << meth << "(\"" << esc << "\");\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                s << "    glue." << meth << "();\n";
                continue;
            }

            const QString effectiveAttrSetName = (!step.attrSetName.isEmpty() && isCollectionType(step.attrSetName, file))
                ? collectionElementType(step.attrSetName, file)
                : step.attrSetName;
            const AttrSet* as = findAttrSet(effectiveAttrSetName, file);

            if (!step.attrSetName.isEmpty() && as == nullptr) {
                if (!isDataType(effectiveAttrSetName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
            }

            if (!step.attrSetName.isEmpty() && as) {
                ++objectCounter;
                const QString listType = effectiveAttrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                QStringList localErrs;
                QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;

                s << "    const " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "      new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        const QString fType = (ci < as->fields.size())
                                            ? as->fields[ci].type : QString();
                        if (!fType.isEmpty() && isAttrSetType(fType, file)) {
                            s << nestedLiteral(row[ci], fType, file);
                            continue;
                        }
                        s << "\"" << jsStringEscape(row[ci]) << "\"";
                    }
                    s << "),\n";
                }
                s << "    ];\n";
                s << "    glue." << meth << "(" << listVar << ");\n";

            } else if (step.hasTable && as == nullptr) {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const StepTable& tbl  = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;

                s << "    const " << listVar << " = [\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "      [";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << jsStringEscape(resolveValue(r[ci], file)) << "\"";
                    }
                    s << "],\n";
                }
                s << "    ];\n";
                s << "    glue." << meth << "(" << listVar << ");\n";
            }
        }
    };

    s << "describe(\"" << jsStringEscape(specName) << "\", () => {\n\n";

    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;

        const QStringList allTags = file.tags + sc.tags;
        for (const QString& tag : allTags) s << "  // @tag: " << tag << "\n";

        s << "  test(\"Scenario " << jsStringEscape(sc.name) << "\", () => {\n";
        s << "    const glue = new " << glueClass << "();\n";
        emitSteps(file.backgroundSteps);
        emitSteps(sc.steps);
        s << "  });\n\n";
    }

    // ── BusinessRule / Calculation / DataType tests ──────────────────────────
    static const QStringList namedKinds = { "BusinessRule", "Calculation", "DataType" };
    QSet<QString> seenNamedBlocks;
    for (const QString& kind : namedKinds) {
        bool hasKind = false;
        for (const NamedBlock& nb : file.namedBlocks)
            if (nb.hasExamples && nb.kind == kind && !nb.isContext
                && !seenNamedBlocks.contains(kind + ":" + nb.name.toLower()))
                { hasKind = true; break; }
        if (!hasKind) continue;

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind || nb.isContext) continue;
            const QString blockKey = kind + ":" + nb.name.toLower();
            if (seenNamedBlocks.contains(blockKey)) {
                errors << QString("WARNING:%1:%2 '%3' declared in multiple files — only first is tested")
                              .arg(nb.line).arg(kind).arg(nb.name);
                continue;
            }
            seenNamedBlocks.insert(blockKey);
            const QStringList effectiveGenTags = file.generatorTags + nb.generatorTags;
            if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;

            const QString glueMeth = toMethodName("Examples_" + kind, nb.name);
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            for (const QString& tag : nb.tags) s << "  // @tag: " << tag << "\n";
            s << "  test(\"" << kind << " " << jsStringEscape(nb.name) << "\", () => {\n";
            s << "    const glue = new " << glueClass << "();\n";

            if (as) {
                ++objectCounter;
                const QString listType = nb.examples.attrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                s << "    const " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "      new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        const QString fType = (ci < as->fields.size())
                                            ? as->fields[ci].type : QString();
                        if (!fType.isEmpty() && isAttrSetType(fType, file)) {
                            s << nestedLiteral(row[ci], fType, file);
                            continue;
                        }
                        s << "\"" << jsStringEscape(row[ci]) << "\"";
                    }
                    s << "),\n";
                }
                s << "    ];\n";
                s << "    glue." << glueMeth << "(" << listVar << ");\n";
            } else {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "    const " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "      [";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << jsStringEscape(resolveValue(row.size() > ci ? row[ci] : QString(), file)) << "\"";
                    }
                    s << "],\n";
                }
                s << "    ];\n";
                s << "    glue." << glueMeth << "(" << listVar << ");\n";
            }
            s << "  });\n\n";
        }
    }

    s << "});\n";
    return out;
}

// ---------------------------------------------------------------------------
// Glue file generator
// ---------------------------------------------------------------------------

QVector<JavaScriptGenerator::GlueSig> JavaScriptGenerator::collectGlueSigs(const SpectableFile& file)
{
    QVector<GlueSig> sigs;
    QSet<QString> seen;

    auto collectSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            const QString meth = toMethodName(step.keyword, step.text);
            if (seen.contains(meth)) continue;
            seen.insert(meth);
            if (step.hasDocString) {
                sigs.push_back({ meth, "docstring" });
            } else if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                sigs.push_back({ meth, (def && def->hasDocString) ? "docstring" : "" });
            } else if (step.attrSetName.isEmpty() && !step.hasTable) {
                sigs.push_back({ meth, "" });
            } else if (!step.attrSetName.isEmpty() && !isDataType(step.attrSetName, file)) {
                const QString effectiveName = isCollectionType(step.attrSetName, file)
                    ? collectionElementType(step.attrSetName, file)
                    : step.attrSetName;
                sigs.push_back({ meth, effectiveName + "String" });
            } else if (!step.attrSetName.isEmpty() && isDataType(step.attrSetName, file)) {
                sigs.push_back({ meth, "grid" });
            } else {
                sigs.push_back({ meth, "list" });
            }
        }
    };

    collectSteps(file.backgroundSteps);
    collectSteps(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios) collectSteps(sc.steps);

    for (const NamedBlock& nb : file.namedBlocks) {
        // A context block belongs to another .spectable and is tested there —
        // generating a stub for it here produces a method no test ever calls.
        if (!nb.hasExamples || nb.isContext) continue;
        const QString meth = toMethodName("Examples_" + nb.kind, nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr
            : findAttrSet(nb.examples.attrSetName, file);
        sigs.push_back({ meth, as ? (nb.examples.attrSetName + "String") : "grid" });
    }
    return sigs;
}

QString JavaScriptGenerator::genStubMethod(const GlueSig& sig, bool failEveryTest)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "\n  " << sig.method << "() {\n";
        if (failEveryTest)
            s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    } else if (sig.paramType == "docstring") {
        s << "\n  " << sig.method << "(value) {\n";
        s << "    console.log(value);\n";
        if (failEveryTest)
            s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    } else if (sig.paramType == "grid" || sig.paramType == "list") {
        s << "\n  " << sig.method << "(values) {\n";
        s << "    values.forEach(row => console.log(Array.isArray(row) ? row.join(\", \") : String(row)));\n";
        if (failEveryTest)
            s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    } else {
        s << "\n  " << sig.method << "(values) {\n";
        s << "    values.forEach(v => console.log(v.toString()));\n";
        if (failEveryTest)
            s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    }
    return out;
}

bool JavaScriptGenerator::appendMissingStubs(const QString& gluePath,
                                              const QVector<GlueSig>& sigs,
                                              QStringList& msgs,
                                       bool failEveryTest)
{
    QFile f(gluePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString content = QTextStream(&f).readAll();
    f.close();
    // A commented-out method has been removed as far as the compiler is
    // concerned, so search a copy with comments blanked out.
    const QString scan = sourcescan::stripCStyleComments(content);

    QString stubs;
    for (const GlueSig& sig : sigs) {
        // Check for "methodName(" in the file
        if (!scan.contains(sig.method + "("))
            stubs += "\n" + genStubMethod(sig, failEveryTest);
    }
    if (stubs.isEmpty()) return false;

    // Insert before the last closing "}" of the class
    const int closingBrace = content.lastIndexOf("\n}");
    if (closingBrace < 0) {
        msgs << QString("WARNING:0:Could not locate class closing brace in %1 — stubs not added")
                .arg(gluePath);
        return false;
    }
    content.insert(closingBrace, stubs);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

QString JavaScriptGenerator::genGlueFile(const SpectableFile& file,
                                          const QString& specName,
                                          const QString& commonRelPath) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    const QString glueClass = toPascalCase(specName) + "Glue";
    QString out;
    QTextStream s(&out);

    s << "import { } from \"" << commonRelPath << "/index.js\";\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";
    s << "export class " << glueClass << " {\n";
    s << "  static DNC_STRING = \"?DNC?\";\n";

    for (const GlueSig& sig : sigs)
        s << genStubMethod(sig, m_failEveryTest) << "\n";

    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Production class generators
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genProductionEntity(const AttrSet& as)
{
    QString out;
    QTextStream s(&out);

    s << "export class " << as.name << " {\n";
    s << "  constructor(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        const QString fn = toCamelCase(f.name);
        if (!f.defaultValue.isEmpty())
            s << fn << " = " << jsStringEscape(f.defaultValue);
        else
            s << fn;
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "    this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "  }\n";
    s << "}\n";
    return out;
}

QString JavaScriptGenerator::genProductionCollection(const Collection& col)
{
    const QString elem = col.elementType;
    QString out;
    QTextStream s(&out);

    s << "import { " << elem << " } from \"./" << elem << ".js\";\n\n";
    s << "export class " << col.name << " {\n";
    if (!col.minimum.isEmpty())
        s << "  static MINIMUM = " << col.minimum << ";\n";
    if (!col.maximum.isEmpty())
        s << "  static MAXIMUM = " << col.maximum << ";\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty()) s << "\n";
    s << "  #items = [];\n\n";
    s << "  add(item) { this.#items.push(item); }\n\n";
    s << "  delete(item) {\n";
    s << "    const idx = this.#items.indexOf(item);\n";
    s << "    if (idx < 0) return false;\n";
    s << "    this.#items.splice(idx, 1);\n";
    s << "    return true;\n";
    s << "  }\n\n";
    s << "  read() { return [...this.#items]; }\n\n";
    s << "  update(oldItem, newItem) {\n";
    s << "    const idx = this.#items.indexOf(oldItem);\n";
    s << "    if (idx < 0) return false;\n";
    s << "    this.#items[idx] = newItem;\n";
    s << "    return true;\n";
    s << "  }\n\n";
    s << "  size() { return this.#items.length; }\n";
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool JavaScriptGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        msgs << QString("ERROR:0:Cannot write file: %1").arg(path);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

QStringList JavaScriptGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraImports = opts.extraImports;
    m_tagFilter    = opts.tagFilter;
    m_failEveryTest = opts.failEveryTest;

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString specName  = file.specName;
    const QString glueClass = toPascalCase(specName) + "Glue";
    const QString specSnake = toFileName(specName);

    // Derive subfolder from the .spectable file's path relative to sourceRoot
    QString specSubDir;
    if (!opts.sourceRoot.isEmpty() && !file.filePath.isEmpty()) {
        const QDir    srcDir(QFileInfo(opts.sourceRoot).absoluteFilePath());
        const QString fileAbsDir = QFileInfo(file.filePath).absoluteDir().absolutePath();
        const QString relPath = srcDir.relativeFilePath(fileAbsDir);
        if (relPath != "." && !relPath.isEmpty()) {
            QStringList parts;
            for (const QString& p : relPath.split('/'))
                if (!p.isEmpty() && p != "..") parts << p;
            specSubDir = parts.join('/');
        }
    }

    QDir dir(specSubDir.isEmpty() ? opts.outputDir : opts.outputDir + "/" + specSubDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create output directory: %1").arg(dir.path());
        return msgs;
    }

    QDir commonDir(opts.outputDir + "/common");
    if (!commonDir.exists() && !commonDir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create common directory: %1").arg(commonDir.path());
        return msgs;
    }

    // Relative import path from the (possibly mirrored) output dir back to common/
    QString commonRelPath = dir.relativeFilePath(commonDir.path());
    if (!commonRelPath.startsWith('.')) commonRelPath.prepend("./");

    // Copy source .spectable
    if (opts.copySpectable && !file.filePath.isEmpty()) {
        const QString dest = dir.filePath(QFileInfo(file.filePath).fileName());
        QFile::remove(dest);
        if (!QFile::copy(file.filePath, dest))
            msgs << QString("WARNING:0:Could not copy %1 to %2").arg(file.filePath, dest);
    }

    // Synthesize implicit AttrSets for NamedBlock examples
    SpectableFile augmented = file;
    {
        QSet<QString> known;
        for (const AttrSet& as : file.attrSets) known.insert(as.name.toLower());
        for (const NamedBlock& nb : file.namedBlocks) {
            const QString asName = nb.examples.attrSetName.trimmed();
            if (asName.isEmpty() || nb.examples.header.isEmpty()) continue;
            if (isDataType(asName, file)) continue;
            if (known.contains(asName.toLower())) continue;
            known.insert(asName.toLower());
            AttrSet sa;
            sa.name = asName;
            const bool isVV = asName.compare("ValidValues", Qt::CaseInsensitive) == 0;
            for (const QString& col : nb.examples.header) {
                const QString c = col.trimmed();
                if (c.isEmpty()) continue;
                Field f; f.name = c;
                f.type = (isVV && c.compare("isvalid", Qt::CaseInsensitive) == 0)
                         ? "YesNo" : "String";
                sa.fields.push_back(f);
            }
            if (!sa.fields.isEmpty()) augmented.attrSets.push_back(sa);
        }
    }

    // 1. Common String + Typed classes
    QVector<AttrSet> domainSets;
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        writeFile(commonDir.filePath(as.name + "String.js"), genStringClass(as, augmented), msgs);
        writeFile(commonDir.filePath(as.name + "Typed.js"),  genTypedClass(as, augmented),  msgs);
        domainSets.push_back(as);
    }
    writeFile(commonDir.filePath("json.js"),  genJsonModule(),             msgs);
    {
        // Read the existing barrel so classes from the other .spectable files survive.
        QString existingIndex;
        QFile xf(commonDir.filePath("index.js"));
        if (xf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            existingIndex = QTextStream(&xf).readAll();
            xf.close();
        }
        writeFile(commonDir.filePath("index.js"),
                  genCommonIndex(domainSets, existingIndex, commonDir.path()), msgs);
    }

    // 2. Test file (always overwritten)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specName, glueClass, commonRelPath, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(dir.filePath("test_" + specSnake + ".test.js"), testContent, msgs);
    }

    // 3. Glue file
    {
        const QString gluePath = dir.filePath(specSnake + "_glue.js");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, specName, commonRelPath), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs, m_failEveryTest))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // 4. Production classes
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists() && !prodDir.mkpath(".")) {
            msgs << QString("ERROR:0:Cannot create production directory: %1").arg(prodDir.path());
        } else {

            // A production file is only ever created, never overwritten. Look for a
            // declaration of the type anywhere in the folder rather than only for the
            // filename we would write, so a developer who groups several classes in one
            // file does not get a duplicate declaration emitted beside their own.
            const sourcescan::ProductionScan prodScan(prodDir.path(), {"*.js"});
            auto alreadyImplemented = [&](const QString& prodPath, const QString& typeName) {
                if (QFile::exists(prodPath)) return true;
                const QString other = prodScan.declaredIn(typeName);
                if (other.isEmpty()) return false;
                msgs << QString("INFO:0:Production type '%1' is already implemented in %2 "
                                "- no template written").arg(typeName, other);
                return true;
            };
            for (const AttrSet& as : file.attrSets) {
                if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
                const QString prodPath = prodDir.filePath(as.name + ".js");
                if (!alreadyImplemented(prodPath, as.name))
                    writeFile(prodPath, genProductionEntity(as), msgs);
            }
            for (const Collection& col : file.collections) {
                if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
                const QString prodPath = prodDir.filePath(col.name + ".js");
                if (!alreadyImplemented(prodPath, col.name))
                    writeFile(prodPath, genProductionCollection(col), msgs);
            }
        }
    }

    return msgs;
}
