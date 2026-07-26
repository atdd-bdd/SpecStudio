#include "RustGenerator.h"
#include "TagFilter.h"
#include "SourceScan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>
#include <QSet>
#include <algorithm>

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

static QString rustEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('"',  "\\\"");
    return r;
}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

QString RustGenerator::rustType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")                          return "i32";
    if (t == "float"   || t == "decimal" || t == "scientific") return "f64";
    if (t == "boolean" || t == "yesno" || t == "bool")        return "bool";
    if (t == "string" || t == "text"
     || t == "character" || t == "char")                       return "String";
    // date/time/duration — returned as String without external crate dependencies
    if (t == "date" || t == "time" || t == "datetime"
     || t == "duration")                                       return "String";
    return specType.trimmed();
}

// A cell for a nested-object field is written "=SomeDefine"; expand that define
// against the nested Attributes block and build its String struct literal.
QString RustGenerator::rustNestedLiteral(const QString& cellValue, const QString& fieldType,
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
        return rustStringLiteral(subAs, row, file);
    }
    return "\"" + rustEscape(cellValue) + "\".to_string()";
}

// A struct literal for one row, used when the block has a nested-object field.
QString RustGenerator::rustStringLiteral(const AttrSet& as, const QStringList& row,
                                         const SpectableFile& file)
{
    QString expr = toTypeName(as.name) + "String { ";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) expr += ", ";
        const QString cell = (i < row.size()) ? row[i] : QString();
        expr += toIdentifier(as.fields[i].name) + ": ";
        if (isAttrSetType(as.fields[i].type, file))
            expr += rustNestedLiteral(cell, as.fields[i].type, file);
        else
            expr += "\"" + rustEscape(cell) + "\".to_string()";
    }
    return expr + " }";
}

bool RustGenerator::isAttrSetType(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

// The type a field takes inside the common module. A nested Attributes block
// becomes that block's own Typed struct. A user DataType lives in the
// production crate, which common must not depend on, so its value is carried
// as text — the glue converts it when it needs the production object.
QString RustGenerator::rustCommonType(const Field& f, const SpectableFile& file)
{
    if (isAttrSetType(f.type, file)) return toTypeName(f.type) + "Typed";
    static const QSet<QString> builtin = {
        "integer", "int", "float", "decimal", "scientific", "boolean", "yesno",
        "bool", "string", "text", "character", "char", "date", "time",
        "datetime", "duration"
    };
    if (!builtin.contains(f.type.trimmed().toLower())) return "String";
    return rustType(f.type);
}

QString RustGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("s.%1.parse::<i32>().unwrap_or_default()").arg(field);
    if (t == "float" || t == "decimal" || t == "scientific")
        return QString("s.%1.parse::<f64>().unwrap_or_default()").arg(field);
    if (t == "boolean" || t == "yesno" || t == "bool")
        return QString(
            "matches!(s.%1.to_lowercase().as_str(), \"true\" | \"t\" | \"yes\" | \"y\" | \"1\")")
            .arg(field);
    // String-backed built-ins (date/time/datetime/duration/string/text/char)
    if (t == "string" || t == "text" || t == "character" || t == "char"
     || t == "date"   || t == "time" || t == "datetime"  || t == "duration")
        return QString("s.%1.clone()").arg(field);
    // User-defined type — use From<String> convention, same intent as Java's new Type(this.field)
    return QString("%1::from(s.%2.clone())").arg(specType.trimmed()).arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString RustGenerator::toIdentifier(const QString& name)
{
    QString s = name.trimmed();
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    if (!s.isEmpty() && s[0].isDigit()) s.prepend('_');
    s = s.toLower();

    // A spec field can be named "Type" or "Match", which lowercase to Rust
    // keywords and will not parse as an identifier.
    static const QSet<QString> keywords = {
        "as", "async", "await", "box", "break", "const", "continue", "crate",
        "dyn", "else", "enum", "extern", "false", "fn", "for", "if", "impl",
        "in", "let", "loop", "match", "mod", "move", "mut", "pub", "ref",
        "return", "self", "static", "struct", "super", "trait", "true", "try",
        "type", "union", "unsafe", "use", "where", "while", "yield"
    };
    if (keywords.contains(s)) s += "_";
    return s;
}

QString RustGenerator::toTypeName(const QString& name)
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

QString RustGenerator::toFnName(const QString& keyword, const QString& stepText)
{
    QString s = keyword + "_" + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    return s.toLower();
}

static QString kindToSnake(const QString& kind)
{
    if (kind == "BusinessRule") return "business_rule";
    if (kind == "Calculation")  return "calculation";
    if (kind == "DataType")     return "data_type";
    QString s = kind.toLower();
    s.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    return s;
}

// ---------------------------------------------------------------------------
// DataType detection
// ---------------------------------------------------------------------------

bool RustGenerator::isDataType(const QString& name, const SpectableFile& file)
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

const AttrSet* RustGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return &as;
    return nullptr;
}

bool RustGenerator::isCollectionType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

QString RustGenerator::collectionElementType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return c.elementType;
    return {};
}

// A step naming a Collection is really a step over that collection's element
// type — `Given item collection is : OrderItemCollection` carries OrderItem
// rows. Resolving to the element is what every other target already does.
QString RustGenerator::effectiveAttrSetName(const QString& name, const SpectableFile& file)
{
    if (!name.isEmpty() && isCollectionType(name, file))
        return collectionElementType(name, file);
    return name;
}

const Define* RustGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Row resolution (same logic as JavaGenerator)
// ---------------------------------------------------------------------------

QVector<QStringList> RustGenerator::resolveStepRows(
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
        // Each row = [AttrName, Value [, Value2, ...]]
        // Extra columns are additional list items; each value column = one result row.
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

QVector<QStringList> RustGenerator::resolveExamplesRows(
    const NamedBlock& block, const AttrSet* as)
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

    for (const QStringList& dr : block.examples.rows) {
        QStringList row(fieldCount);
        for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
            if (colMap[ci] >= 0) row[colMap[ci]] = resolveCell(dr[ci]);
        result << row;
    }
    return result;
}

// ---------------------------------------------------------------------------
// String struct
// ---------------------------------------------------------------------------

QString RustGenerator::genStringStruct(const AttrSet& as, const SpectableFile& file) const
{
    const QString typeName = toTypeName(as.name) + "String";
    QString out;
    QTextStream s(&out);

    s << "#![allow(dead_code, unused_imports, unused_variables)]\n\n";
    // A field whose type names another Attributes block holds that block's
    // String struct, so its module has to be brought in.
    QSet<QString> seenUses;
    for (const Field& f : as.fields) {
        if (!isAttrSetType(f.type, file)) continue;
        // Two fields can share a nested type; import it once.
        if (seenUses.contains(f.type.trimmed().toLower())) continue;
        seenUses.insert(f.type.trimmed().toLower());
        s << "use super::" << toIdentifier(f.type) << "_string::"
          << toTypeName(f.type) << "String;\n";
    }
    for (const QString& u : m_extraUses) s << u << "\n";
    if (!m_extraUses.isEmpty()) s << "\n";

    auto fieldType = [&](const Field& f) {
        return isAttrSetType(f.type, file) ? toTypeName(f.type) + "String"
                                          : QString("String");
    };

    s << "#[derive(Debug, Clone, Default)]\n";
    s << "pub struct " << typeName << " {\n";
    for (const Field& f : as.fields)
        s << "    pub " << toIdentifier(f.name) << ": " << fieldType(f) << ",\n";
    s << "}\n\n";

    s << "impl " << typeName << " {\n";
    s << "    pub fn from_vec(v: &[&str]) -> Self {\n";
    s << "        Self {\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f = as.fields[i];
        // A nested block cannot come from a flat slice; those rows are built
        // with a struct literal instead.
        if (isAttrSetType(f.type, file)) {
            s << "            " << toIdentifier(f.name) << ": Default::default(),\n";
            continue;
        }
        s << "            " << toIdentifier(f.name)
          << ": v.get(" << i << ").copied().unwrap_or(\"\").to_string(),\n";
    }
    s << "        }\n    }\n}\n\n";

    s << "impl std::fmt::Display for " << typeName << " {\n";
    s << "    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {\n";
    s << "        write!(f,\n";

    QStringList placeholders, args;
    for (const Field& field : as.fields) {
        placeholders << (field.name + (isAttrSetType(field.type, file) ? "={:?}" : "={}"));
        args << "self." + toIdentifier(field.name);
    }
    s << "            \"" << placeholders.join(", ") << "\",\n";
    for (int i = 0; i < args.size(); ++i) {
        s << "            " << args[i];
        if (i < args.size() - 1) s << ",";
        s << "\n";
    }
    s << "        )\n    }\n}\n\n";

    // A field holding the Do-Not-Care marker on either side matches whatever
    // the other side holds — that is what lets a CompareOnly step name only the
    // columns it cares about. dnc_equal lives in common/mod.rs.
    s << "impl PartialEq for " << typeName << " {\n";
    s << "    fn eq(&self, other: &Self) -> bool {\n";
    if (as.fields.isEmpty()) {
        s << "        true\n";
    } else {
        s << "        ";
        for (int i = 0; i < as.fields.size(); ++i) {
            if (i) s << "\n            && ";
            const QString fid = toIdentifier(as.fields[i].name);
            // A nested block delegates to its own PartialEq.
            if (isAttrSetType(as.fields[i].type, file))
                s << "self." << fid << " == other." << fid;
            else
                s << "crate::common::dnc_equal(&self." << fid << ", &other." << fid << ")";
        }
        s << "\n";
    }
    s << "    }\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed struct
// ---------------------------------------------------------------------------

QString RustGenerator::genTypedStruct(const AttrSet& as, const SpectableFile& file) const
{
    const QString strTypeName = toTypeName(as.name) + "String";
    const QString typedName   = toTypeName(as.name) + "Typed";
    const QString strMod      = toIdentifier(as.name) + "_string";
    QString out;
    QTextStream s(&out);

    s << "#![allow(dead_code, unused_imports, unused_variables)]\n\n";
    s << "use super::json;\n";
    s << "use super::" << strMod << "::" << strTypeName << ";\n";
    QSet<QString> seenTypedUses;
    for (const Field& f : as.fields) {
        if (!isAttrSetType(f.type, file)) continue;
        if (seenTypedUses.contains(f.type.trimmed().toLower())) continue;
        seenTypedUses.insert(f.type.trimmed().toLower());
        s << "use super::" << toIdentifier(f.type) << "_typed::"
          << toTypeName(f.type) << "Typed;\n";
    }
    for (const QString& u : m_extraUses) s << u << "\n";
    s << "\n";

    s << "#[derive(Debug, Clone, Default, PartialEq)]\n";
    s << "pub struct " << typedName << " {\n";
    for (const Field& f : as.fields) {
        const QString rt = rustCommonType(f, file);
        s << "    pub " << toIdentifier(f.name) << ": " << rt << ",\n";
    }
    s << "}\n\n";

    s << "impl " << typedName << " {\n";
    s << "    pub fn from_str_struct(s: &" << strTypeName << ") -> Self {\n";
    s << "        Self {\n";
    for (const Field& f : as.fields) {
        const QString fid  = toIdentifier(f.name);
        const QString rt   = rustCommonType(f, file);
        // A nested Attributes block builds its own Typed struct; a user DataType
        // is carried as text, so only the built-ins go through parseExpr.
        const QString expr = isAttrSetType(f.type, file)
            ? QString("%1::from_str_struct(&s.%2)").arg(rt, fid)
            : (rt == "String" ? QString("s.%1.clone()").arg(fid)
                              : parseExpr(fid, f.type));
        s << "            " << fid << ": " << expr << ",\n";
    }
    s << "        }\n    }\n\n";

    // ---- JSON (see common/json.rs — no serde dependency) ----

    s << "    pub fn to_json_value(&self) -> json::Value {\n";
    s << "        json::Value::Object(vec![\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        const QString rt  = rustCommonType(f, file);
        QString expr;
        if (rt == "i32")
            expr = QString("json::Value::number_from_i64(self.%1 as i64)").arg(fid);
        else if (rt == "f64")
            expr = QString("json::Value::number_from_f64(self.%1)").arg(fid);
        else if (rt == "bool")
            expr = QString("json::Value::Bool(self.%1)").arg(fid);
        else if (rt == "String")
            expr = QString("json::Value::Str(self.%1.clone())").arg(fid);
        else    // nested Attributes block — written as a nested object
            expr = QString("self.%1.to_json_value()").arg(fid);
        s << "            (\"" << fid << "\".to_string(), " << expr << "),\n";
    }
    s << "        ])\n";
    s << "    }\n\n";

    s << "    pub fn to_json(&self) -> String {\n";
    s << "        json::write(&self.to_json_value())\n";
    s << "    }\n\n";

    s << "    pub fn from_json_value(v: &json::Value) -> json::JsonResult<Self> {\n";
    s << "        Ok(Self {\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        const QString rt  = rustCommonType(f, file);
        const QString src = QString("json::require(v, \"%1\")?").arg(fid);
        QString expr;
        if (rt == "i32")
            expr = QString("json::as_i32(%1, \"%2\")?").arg(src, fid);
        else if (rt == "f64")
            expr = QString("json::as_f64(%1, \"%2\")?").arg(src, fid);
        else if (rt == "bool")
            expr = QString("json::as_bool(%1, \"%2\")?").arg(src, fid);
        else if (rt == "String")
            expr = QString("json::as_string(%1, \"%2\")?").arg(src, fid);
        else    // nested Attributes block — read as its own Typed struct
            expr = QString("%1::from_json_value(%2)?").arg(rt, src);
        s << "            " << fid << ": " << expr << ",\n";
    }
    s << "        })\n";
    s << "    }\n\n";

    s << "    pub fn from_json(text: &str) -> json::JsonResult<Self> {\n";
    s << "        Self::from_json_value(&json::parse(text)?)\n";
    s << "    }\n\n";

    s << "    pub fn to_json_list(list: &[" << typedName << "]) -> String {\n";
    s << "        json::write(&json::Value::Array(\n";
    s << "            list.iter().map(|item| item.to_json_value()).collect(),\n";
    s << "        ))\n";
    s << "    }\n\n";

    s << "    pub fn from_json_list(text: &str) -> json::JsonResult<Vec<" << typedName << ">> {\n";
    s << "        let root = json::parse(text)?;\n";
    s << "        let items = json::as_array(&root, \"" << typedName << "\")?;\n";
    s << "        let mut result = Vec::with_capacity(items.len());\n";
    s << "        for e in items {\n";
    s << "            result.push(Self::from_json_value(e)?);\n";
    s << "        }\n";
    s << "        Ok(result)\n";
    s << "    }\n";
    s << "}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Common mod.rs
// ---------------------------------------------------------------------------

// Every .spectable in a project generates into the same common module, and this
// index is rewritten on each one. Emitting only the current file's AttrSets meant
// the last file processed erased every struct contributed by the others, so
// `use crate::common::*` resolved almost nothing. The existing index is therefore
// merged with the new entries instead of replaced.
QString RustGenerator::genCommonMod(const QVector<AttrSet>& attrSets,
                                     const QString& existing,
                                     const QString& dir) const
{
    QStringList mods;
    for (const QString& line : existing.split('\n')) {
        const QString t = line.trimmed();
        if (t.startsWith("pub mod ") || t.startsWith("pub use ")) {
            if (t == "pub mod json;" || t == "pub use json::*;") continue;
            if (!mods.contains(t)) mods << t;
        }
    }
    mods = sourcescan::dropEntriesForMissingFiles(
               mods, dir, QRegularExpression("^pub (?:mod|use) ([A-Za-z0-9_]+)"), ".rs");
    for (const AttrSet& as : attrSets) {
        const QString mod = toIdentifier(as.name);
        for (const QString& l : { QString("pub mod %1_string;").arg(mod),
                                  QString("pub mod %1_typed;").arg(mod),
                                  QString("pub use %1_string::*;").arg(mod),
                                  QString("pub use %1_typed::*;").arg(mod) })
            if (!mods.contains(l)) mods << l;
    }
    mods.sort();

    QString out;
    QTextStream s(&out);
    // The re-exports below are a convenience surface; a crate that uses only
    // some of them would otherwise get an unused_imports warning per line.
    s << "#![allow(unused_imports, dead_code)]\n\n";
    s << "pub mod json;\n";
    s << "pub use json::*;\n";
    for (const QString& l : mods) s << l << "\n";

    s << "\n/// The Do-Not-Care marker a CompareOnly step puts in every column\n";
    s << "/// it does not name.\n";
    s << "pub const DNC_STRING: &str = \"?DNC?\";\n\n";
    s << "/// Compares two cells, treating the marker as a wildcard.\n";
    s << "pub fn dnc_equal(a: &str, b: &str) -> bool {\n";
    s << "    a == b || a == DNC_STRING || b == DNC_STRING\n}\n\n";
    s << "/// Reads the Yes/No/True/False text a spec cell may hold, in any casing.\n";
    s << "pub fn parse_bool_cell(v: &str) -> bool {\n";
    s << "    matches!(v.trim().to_ascii_lowercase().as_str(),\n";
    s << "             \"true\" | \"t\" | \"yes\" | \"y\" | \"1\")\n}\n";
    return out;
}

// ---------------------------------------------------------------------------
// common/json.rs — dependency-free JSON reader/writer (no serde).
// Numbers keep their original text so nothing is lost on a round trip.
// Emitted in chunks: MSVC caps a single string literal at 16380 bytes.
// ---------------------------------------------------------------------------

static QString genRustJsonMod()
{
    QString out;
    out += QString::fromLatin1(R"RS(//! Minimal dependency-free JSON reader/writer used by the generated Typed structs.
//!
//! No external crate is required. A missing field or a value of the wrong type
//! produces `Err(JsonError)`; an explicit JSON null is passed through.

#![allow(dead_code)]

use std::collections::BTreeMap;
use std::fmt;

#[derive(Debug, Clone, PartialEq)]
pub struct JsonError {
    pub message: String,
}

impl JsonError {
    pub fn new(message: impl Into<String>) -> Self {
        Self { message: message.into() }
    }
}

impl fmt::Display for JsonError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for JsonError {}

pub type JsonResult<T> = Result<T, JsonError>;

/// A parsed JSON value. Numbers keep their source text so that decimal and
/// long integer fields survive a round trip exactly.
#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Null,
    Bool(bool),
    Number(String),
    Str(String),
    Array(Vec<Value>),
    Object(Vec<(String, Value)>),
}

impl Value {
    pub fn number_from_i64(v: i64) -> Value { Value::Number(v.to_string()) }

    pub fn number_from_f64(v: f64) -> Value {
        if v.is_finite() {
            // {:?} gives the shortest representation that round-trips.
            Value::Number(format!("{:?}", v))
        } else {
            Value::Null
        }
    }

    pub fn get(&self, key: &str) -> Option<&Value> {
        match self {
            Value::Object(members) => members.iter().find(|(k, _)| k == key).map(|(_, v)| v),
            _ => None,
        }
    }

    pub fn describe(&self) -> &'static str {
        match self {
            Value::Null      => "null",
            Value::Bool(_)   => "a boolean",
            Value::Number(_) => "a number",
            Value::Str(_)    => "a string",
            Value::Array(_)  => "an array",
            Value::Object(_) => "an object",
        }
    }
}

/// Kept so callers that prefer a map view have one; the parser preserves order.
pub fn to_map(value: &Value) -> BTreeMap<String, Value> {
    match value {
        Value::Object(members) => members.iter().cloned().collect(),
        _ => BTreeMap::new(),
    }
}
)RS");

    out += QString::fromLatin1(R"RS(
// ---------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------

pub fn escape_into(out: &mut String, s: &str) {
    out.push('"');
    for c in s.chars() {
        match c {
            '"'  => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\u{08}' => out.push_str("\\b"),
            '\u{0c}' => out.push_str("\\f"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out.push('"');
}

pub fn write_into(out: &mut String, value: &Value) {
    match value {
        Value::Null      => out.push_str("null"),
        Value::Bool(b)   => out.push_str(if *b { "true" } else { "false" }),
        Value::Number(n) => out.push_str(n),
        Value::Str(s)    => escape_into(out, s),
        Value::Array(items) => {
            out.push('[');
            for (i, item) in items.iter().enumerate() {
                if i > 0 { out.push(','); }
                write_into(out, item);
            }
            out.push(']');
        }
        Value::Object(members) => {
            out.push('{');
            for (i, (k, v)) in members.iter().enumerate() {
                if i > 0 { out.push(','); }
                escape_into(out, k);
                out.push(':');
                write_into(out, v);
            }
            out.push('}');
        }
    }
}

pub fn write(value: &Value) -> String {
    let mut out = String::new();
    write_into(&mut out, value);
    out
}
)RS");

    out += QString::fromLatin1(R"RS(
// ---------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------

struct Parser<'a> {
    src: &'a [u8],
    text: &'a str,
    i: usize,
}

impl<'a> Parser<'a> {
    fn new(text: &'a str) -> Self {
        Self { src: text.as_bytes(), text, i: 0 }
    }

    fn err<T>(&self, msg: &str) -> JsonResult<T> {
        Err(JsonError::new(format!("{} at offset {}", msg, self.i)))
    }

    fn at_end(&self) -> bool { self.i >= self.src.len() }

    fn skip_ws(&mut self) {
        while self.i < self.src.len() {
            match self.src[self.i] {
                b' ' | b'\t' | b'\n' | b'\r' => self.i += 1,
                _ => break,
            }
        }
    }

    fn expect_word(&mut self, word: &str) -> JsonResult<()> {
        if self.text[self.i..].starts_with(word) {
            self.i += word.len();
            Ok(())
        } else {
            self.err(&format!("Expected '{}'", word))
        }
    }

    fn read_value(&mut self) -> JsonResult<Value> {
        self.skip_ws();
        if self.at_end() {
            return self.err("Unexpected end of JSON input");
        }
        match self.src[self.i] {
            b'{' => self.read_object(),
            b'[' => self.read_array(),
            b'"' => Ok(Value::Str(self.read_string()?)),
            b't' => { self.expect_word("true")?;  Ok(Value::Bool(true)) }
            b'f' => { self.expect_word("false")?; Ok(Value::Bool(false)) }
            b'n' => { self.expect_word("null")?;  Ok(Value::Null) }
            _    => self.read_number(),
        }
    }

    fn read_object(&mut self) -> JsonResult<Value> {
        let mut members: Vec<(String, Value)> = Vec::new();
        self.i += 1; // consume '{'
        self.skip_ws();
        if !self.at_end() && self.src[self.i] == b'}' {
            self.i += 1;
            return Ok(Value::Object(members));
        }
        loop {
            self.skip_ws();
            if self.at_end() || self.src[self.i] != b'"' {
                return self.err("Expected a string key");
            }
            let key = self.read_string()?;
            self.skip_ws();
            if self.at_end() || self.src[self.i] != b':' {
                return self.err(&format!("Expected ':' after key '{}'", key));
            }
            self.i += 1;
            let value = self.read_value()?;
            members.push((key, value));
            self.skip_ws();
            if self.at_end() {
                return self.err("Unterminated object");
            }
            match self.src[self.i] {
                b',' => { self.i += 1; }
                b'}' => { self.i += 1; return Ok(Value::Object(members)); }
                _ => return self.err("Expected ',' or '}'"),
            }
        }
    }

    fn read_array(&mut self) -> JsonResult<Value> {
        let mut items: Vec<Value> = Vec::new();
        self.i += 1; // consume '['
        self.skip_ws();
        if !self.at_end() && self.src[self.i] == b']' {
            self.i += 1;
            return Ok(Value::Array(items));
        }
        loop {
            items.push(self.read_value()?);
            self.skip_ws();
            if self.at_end() {
                return self.err("Unterminated array");
            }
            match self.src[self.i] {
                b',' => { self.i += 1; }
                b']' => { self.i += 1; return Ok(Value::Array(items)); }
                _ => return self.err("Expected ',' or ']'"),
            }
        }
    }
)RS");

    out += QString::fromLatin1(R"RS(
    fn read_hex4(&mut self) -> JsonResult<u32> {
        if self.i + 4 > self.src.len() {
            return self.err("Truncated \\u escape");
        }
        let mut cp: u32 = 0;
        for k in 0..4 {
            let c = self.src[self.i + k];
            let d = match c {
                b'0'..=b'9' => (c - b'0') as u32,
                b'a'..=b'f' => (c - b'a' + 10) as u32,
                b'A'..=b'F' => (c - b'A' + 10) as u32,
                _ => return self.err("Invalid \\u escape"),
            };
            cp = (cp << 4) | d;
        }
        self.i += 4;
        Ok(cp)
    }

    fn read_string(&mut self) -> JsonResult<String> {
        self.i += 1; // consume opening quote
        let mut out = String::new();
        loop {
            if self.at_end() {
                return self.err("Unterminated string");
            }
            let c = self.src[self.i];
            if c == b'"' {
                self.i += 1;
                return Ok(out);
            }
            if c != b'\\' {
                // Copy one whole UTF-8 character.
                let rest = &self.text[self.i..];
                let ch = match rest.chars().next() {
                    Some(ch) => ch,
                    None => return self.err("Unterminated string"),
                };
                out.push(ch);
                self.i += ch.len_utf8();
                continue;
            }
            self.i += 1; // consume backslash
            if self.at_end() {
                return self.err("Unterminated escape");
            }
            let e = self.src[self.i];
            self.i += 1;
            match e {
                b'"'  => out.push('"'),
                b'\\' => out.push('\\'),
                b'/'  => out.push('/'),
                b'b'  => out.push('\u{08}'),
                b'f'  => out.push('\u{0c}'),
                b'n'  => out.push('\n'),
                b'r'  => out.push('\r'),
                b't'  => out.push('\t'),
                b'u'  => {
                    let mut cp = self.read_hex4()?;
                    // Combine a surrogate pair when both halves are present.
                    if (0xD800..=0xDBFF).contains(&cp)
                        && self.i + 1 < self.src.len()
                        && self.src[self.i] == b'\\'
                        && self.src[self.i + 1] == b'u'
                    {
                        let save = self.i;
                        self.i += 2;
                        let lo = self.read_hex4()?;
                        if (0xDC00..=0xDFFF).contains(&lo) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            self.i = save;
                        }
                    }
                    match char::from_u32(cp) {
                        Some(ch) => out.push(ch),
                        None => return self.err("Invalid code point in \\u escape"),
                    }
                }
                _ => return self.err("Invalid escape"),
            }
        }
    }

    fn read_number(&mut self) -> JsonResult<Value> {
        let start = self.i;
        if !self.at_end() && self.src[self.i] == b'-' {
            self.i += 1;
        }
        while !self.at_end() {
            match self.src[self.i] {
                b'0'..=b'9' | b'.' | b'e' | b'E' | b'+' | b'-' => self.i += 1,
                _ => break,
            }
        }
        if start == self.i {
            return self.err("Expected a value");
        }
        let text = &self.text[start..self.i];
        if text.parse::<f64>().is_err() {
            return Err(JsonError::new(format!(
                "Invalid number '{}' at offset {}", text, start)));
        }
        Ok(Value::Number(text.to_string()))
    }
}

pub fn parse(text: &str) -> JsonResult<Value> {
    let mut p = Parser::new(text);
    let v = p.read_value()?;
    p.skip_ws();
    if !p.at_end() {
        return Err(JsonError::new(format!("Trailing content at offset {}", p.i)));
    }
    Ok(v)
}
)RS");

    out += QString::fromLatin1(R"RS(
// ---------------------------------------------------------------------
// Field accessors
// ---------------------------------------------------------------------

fn type_error<T>(ctx: &str, expected: &str, actual: &Value) -> JsonResult<T> {
    Err(JsonError::new(format!(
        "JSON field '{}' is not {} (got {})", ctx, expected, actual.describe())))
}

pub fn require<'a>(obj: &'a Value, key: &str) -> JsonResult<&'a Value> {
    match obj {
        Value::Object(_) => obj
            .get(key)
            .ok_or_else(|| JsonError::new(format!("Missing JSON field '{}'", key))),
        _ => Err(JsonError::new(format!(
            "Expected an object holding field '{}'", key))),
    }
}

pub fn as_string(v: &Value, ctx: &str) -> JsonResult<String> {
    match v {
        Value::Str(s)    => Ok(s.clone()),
        Value::Number(n) => Ok(n.clone()),
        Value::Bool(b)   => Ok(if *b { "true".to_string() } else { "false".to_string() }),
        Value::Null      => Ok(String::new()),
        _ => type_error(ctx, "a string", v),
    }
}

pub fn as_f64(v: &Value, ctx: &str) -> JsonResult<f64> {
    match v {
        Value::Number(n) => n.parse::<f64>().map_err(|_| {
            JsonError::new(format!("JSON field '{}' is not a number", ctx))
        }),
        Value::Str(s) => s.trim().parse::<f64>().map_err(|_| {
            JsonError::new(format!("JSON field '{}' is not a number", ctx))
        }),
        _ => type_error(ctx, "a number", v),
    }
}

pub fn as_i32(v: &Value, ctx: &str) -> JsonResult<i32> {
    let d = as_f64(v, ctx)?;
    if d.fract() != 0.0 || d < i32::MIN as f64 || d > i32::MAX as f64 {
        return type_error(ctx, "an integer", v);
    }
    Ok(d as i32)
}

pub fn as_bool(v: &Value, ctx: &str) -> JsonResult<bool> {
    match v {
        Value::Bool(b) => Ok(*b),
        Value::Str(s) => {
            let low = s.trim().to_lowercase();
            match low.as_str() {
                "true"  | "t" | "yes" | "y" | "1" => Ok(true),
                "false" | "f" | "no"  | "n" | "0" => Ok(false),
                _ => type_error(ctx, "a boolean", v),
            }
        }
        _ => type_error(ctx, "a boolean", v),
    }
}

pub fn as_array<'a>(v: &'a Value, ctx: &str) -> JsonResult<&'a Vec<Value>> {
    match v {
        Value::Array(items) => Ok(items),
        _ => type_error(ctx, "an array", v),
    }
}
)RS");
    return out;
}

// ---------------------------------------------------------------------------
// Test file
// ---------------------------------------------------------------------------

QString RustGenerator::genTestFile(const SpectableFile& file, const QString& specSnake,
                                    const QString& glueStruct, QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    s << "#![allow(unused_mut, unused_variables, unused_imports)]\n\n";
    s << "use crate::common::*;\n";
    // The glue module is a sibling of this test module, so `super::` resolves
    // whether the specification sits at the crate root or in a subfolder.
    s << "use super::" << specSnake << "_glue::" << glueStruct << ";\n";
    for (const QString& u : m_extraUses) s << u << "\n";
    s << "\n";

    // Helper: emit a slice of AttrSetString rows inline. A block with a
    // nested-object field cannot be built from a flat &[&str], so those rows use
    // a struct literal instead.
    auto emitStrSlice = [&](const QString& listType, const QVector<QStringList>& rows,
                            const AttrSet* as) {
        bool hasNested = false;
        if (as)
            for (const Field& f : as->fields)
                if (isAttrSetType(f.type, file)) { hasNested = true; break; }

        auto emitRow = [&](const QStringList& row) {
            if (hasNested) {
                s << rustStringLiteral(*as, row, file);
                return;
            }
            s << listType << "::from_vec(&[";
            for (int ci = 0; ci < row.size(); ++ci) {
                if (ci) s << ", ";
                s << "\"" << rustEscape(row[ci]) << "\"";
            }
            s << "])";
        };

        if (rows.size() == 1) {
            s << "&[";
            emitRow(rows[0]);
            s << "]";
        } else {
            s << "&[\n";
            for (const QStringList& row : rows) {
                s << "        ";
                emitRow(row);
                s << ",\n";
            }
            s << "    ]";
        }
    };

    // Helper: emit a slice of Vec<String> rows inline
    auto emitGridSlice = [&](const QVector<QStringList>& rows, int startRow = 0) {
        s << "&[\n";
        for (int ri = startRow; ri < rows.size(); ++ri) {
            s << "        vec![";
            const QStringList& r = rows[ri];
            for (int ci = 0; ci < r.size(); ++ci) {
                if (ci) s << ", ";
                s << "\"" << rustEscape(resolveValue(r[ci], file)) << "\".to_string()";
            }
            s << "],\n";
        }
        s << "    ]";
    };

    auto emitSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            if (step.hasDocString) {
                const QString meth = toFnName(step.keyword, step.text);
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
                    const QString meth = toFnName(step.keyword, step.text);
                    QString esc = def->docString;
                    esc.replace("\\", "\\\\");
                    esc.replace("\"", "\\\"");
                    esc.replace("\n", "\\n");
                    s << "    glue." << meth << "(\"" << esc << "\");\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                const QString meth = toFnName(step.keyword, step.text);
                s << "    glue." << meth << "();\n";
                continue;
            }

            // A Collection step carries rows of its element type.
            const QString effName = effectiveAttrSetName(step.attrSetName, file);
            const AttrSet* as = findAttrSet(effName, file);

            if (!step.attrSetName.isEmpty() && !as) {
                if (!isDataType(effName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
            }

            const QString meth = toFnName(step.keyword, step.text);

            if (!step.attrSetName.isEmpty() && as) {
                QStringList localErrs;
                QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;
                const QString listType = toTypeName(effName) + "String";
                s << "    glue." << meth << "(";
                emitStrSlice(listType, rows, as);
                s << ");\n";
            } else {
                const StepTable& tbl = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;
                s << "    glue." << meth << "(";
                emitGridSlice(tbl.rows, startRow);
                s << ");\n";
            }
        }
    };

    // ── Scenario tests ──────────────────────────────────────────────────────
    if (!file.scenarios.isEmpty())
        s << "// --- Scenario Tests ---\n\n";

    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;
        const QString fn = "scenario_" + toIdentifier(sc.name);

        const QStringList allTags = file.tags + sc.tags;
        if (!allTags.isEmpty())
            s << "// Tags: " << allTags.join(", ") << "\n";
        s << "#[test]\n";
        s << "fn " << fn << "() {\n";
        s << "    let mut glue = " << glueStruct << "::new();\n";
        emitSteps(file.backgroundSteps);
        emitSteps(sc.steps);
        s << "}\n\n";
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

        const QString ks = kindToSnake(kind);
        s << "// --- " << kind << " Tests ---\n\n";

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

            const QString fn      = ks + "_" + toIdentifier(nb.name);
            const QString glueFn  = "examples_" + ks + "_" + toIdentifier(nb.name);
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            if (!nb.tags.isEmpty())
                s << "// Tags: " << nb.tags.join(", ") << "\n";
            s << "#[test]\n";
            s << "fn " << fn << "() {\n";
            s << "    let mut glue = " << glueStruct << "::new();\n";

            if (as) {
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                const QString listType = toTypeName(nb.examples.attrSetName) + "String";
                s << "    glue." << glueFn << "(";
                emitStrSlice(listType, rows, as);
                s << ");\n";
            } else {
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "    glue." << glueFn << "(";
                emitGridSlice(rows);
                s << ");\n";
            }
            s << "}\n\n";
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Glue file
// ---------------------------------------------------------------------------

QVector<RustGenerator::GlueSig> RustGenerator::collectGlueSigs(const SpectableFile& file)
{
    QVector<GlueSig> sigs;
    QSet<QString> seen;

    auto collectSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            const QString meth = toFnName(step.keyword, step.text);
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
                // A Collection step takes a slice of its element type.
                sigs.push_back({ meth, effectiveAttrSetName(step.attrSetName, file) + "String" });
            } else {
                sigs.push_back({ meth, "grid" });
            }
        }
    };

    collectSteps(file.backgroundSteps);
    collectSteps(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios)
        collectSteps(sc.steps);

    for (const NamedBlock& nb : file.namedBlocks) {
        // A context block belongs to another .spectable and is tested there —
        // generating a stub for it here produces a method no test ever calls.
        if (!nb.hasExamples || nb.isContext) continue;
        const QString meth = "examples_" + kindToSnake(nb.kind) + "_" + toIdentifier(nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr
            : findAttrSet(nb.examples.attrSetName, file);
        sigs.push_back({ meth, as ? (nb.examples.attrSetName + "String") : "grid" });
    }

    return sigs;
}

QString RustGenerator::genStubFn(const GlueSig& sig)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "    pub fn " << sig.method << "(&mut self) {\n";
        s << "        panic!(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
    } else if (sig.paramType == "docstring") {
        s << "    pub fn " << sig.method << "(&mut self, value: &str) {\n";
        s << "        println!(\"{}\", value);\n";
        s << "        panic!(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
    } else if (sig.paramType == "grid") {
        s << "    pub fn " << sig.method << "(&mut self, values: &[Vec<String>]) {\n";
        s << "        for value in values { println!(\"{:?}\", value); }\n";
        s << "        panic!(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
    } else {
        const QString pt = toTypeName(sig.paramType);
        s << "    pub fn " << sig.method << "(&mut self, values: &[" << pt << "]) {\n";
        s << "        for value in values { println!(\"{:?}\", value); }\n";
        s << "        panic!(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
    }
    return out;
}

bool RustGenerator::appendMissingStubs(const QString& gluePath,
                                        const QVector<GlueSig>& sigs,
                                        QStringList& msgs)
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
        if (!scan.contains(QStringLiteral("fn %1(").arg(sig.method)))
            stubs += "\n" + genStubFn(sig);
    }
    if (stubs.isEmpty()) return false;

    const int closingBrace = content.lastIndexOf("\n}");
    if (closingBrace < 0) {
        msgs << QString("WARNING:0:Could not locate closing brace in %1 — stubs not added")
                .arg(gluePath);
        return false;
    }

    content.insert(closingBrace, "\n" + stubs);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

QString RustGenerator::genGlueFile(const SpectableFile& file, const QString& glueStruct) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    QString out;
    QTextStream s(&out);

    s << "#![allow(dead_code, unused_variables, unused_imports)]\n\n";
    s << "use crate::common::*;\n";
    for (const QString& u : m_extraUses) s << u << "\n";
    s << "\n";
    s << "pub struct " << glueStruct << " {\n";
    s << "    // Add state fields here\n";
    s << "}\n\n";
    s << "impl " << glueStruct << " {\n";
    s << "    pub fn new() -> Self {\n";
    s << "        Self {}\n";
    s << "    }\n";
    for (const GlueSig& sig : sigs)
        s << "\n" << genStubFn(sig);
    s << "}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Production class generators
// ---------------------------------------------------------------------------

// DataType ValidValues → struct with is_valid() method
static QString genRustProductionClass(const NamedBlock& nb)
{
    const QString name = RustGenerator::toTypeName(nb.name);
    QString out;
    QTextStream s(&out);
    s << "#[derive(Debug, Clone, PartialEq, Eq)]\n";
    s << "pub struct " << name << " {\n";
    s << "    pub value: String,\n";
    s << "}\n\n";
    s << "impl " << name << " {\n";
    s << "    pub fn new(value: impl Into<String>) -> Self {\n";
    s << "        Self { value: value.into() }\n";
    s << "    }\n\n";

    int valueCol = -1, isValidCol = -1;
    for (int i = 0; i < nb.examples.header.size(); ++i) {
        const QString h = nb.examples.header[i].trimmed();
        if (h.compare("value", Qt::CaseInsensitive) == 0) valueCol = i;
        if (h.compare("isvalid", Qt::CaseInsensitive) == 0) isValidCol = i;
    }
    if (valueCol >= 0) {
        QStringList vals;
        for (const QStringList& row : nb.examples.rows) {
            if (valueCol >= row.size() || row[valueCol].trimmed().isEmpty()) continue;
            if (isValidCol >= 0 && isValidCol < row.size()) {
                const QString iv = row[isValidCol].trimmed().toLower();
                const bool isTrue = (iv == "true" || iv == "t" || iv == "yes"
                                   || iv == "y" || iv == "1");
                if (!isTrue) continue;  // skip examples marked invalid
            }
            vals << "\"" + row[valueCol].trimmed() + "\"";
        }
        if (!vals.isEmpty()) {
            s << "    pub fn is_valid(&self) -> bool {\n";
            s << "        matches!(self.value.to_lowercase().as_str(),\n";
            QStringList lower;
            for (const QString& v : vals) lower << v.toLower();
            s << "            " << lower.join(" | ") << "\n";
            s << "        )\n    }\n";
        }
    }
    s << "}\n\n";
    // From<String> is the conversion the generated Typed structs use, both in
    // from_str_struct and in from_json_value.
    s << "impl From<String> for " << name << " {\n";
    s << "    fn from(value: String) -> Self { Self { value } }\n";
    s << "}\n\n";
    s << "impl From<&str> for " << name << " {\n";
    s << "    fn from(value: &str) -> Self { Self { value: value.to_string() } }\n";
    s << "}\n\n";
    s << "impl std::fmt::Display for " << name << " {\n";
    s << "    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {\n";
    s << "        write!(f, \"{}\", self.value)\n    }\n}\n";
    return out;
}

// EnumerationValues DataType → enum
static QString genRustProductionEnum(const NamedBlock& nb)
{
    int valueCol = -1;
    for (int i = 0; i < nb.examples.header.size(); ++i)
        if (nb.examples.header[i].trimmed().compare("value", Qt::CaseInsensitive) == 0)
            { valueCol = i; break; }

    QStringList variants;
    if (valueCol >= 0)
        for (const QStringList& row : nb.examples.rows)
            if (valueCol < row.size() && !row[valueCol].trimmed().isEmpty())
                variants << row[valueCol].trimmed();

    const QString name = RustGenerator::toTypeName(nb.name);
    QString out;
    QTextStream s(&out);
    s << "#[derive(Debug, Clone, Copy, PartialEq, Eq)]\n";
    s << "pub enum " << name << " {\n";
    for (const QString& v : variants)
        s << "    " << v << ",\n";
    s << "}\n";
    return out;
}

// Entity → production struct with impl new()
static QString genRustProductionEntity(const AttrSet& as, const SpectableFile& file)
{
    (void)file;  // reserved for cross-entity type lookup
    const QString name = RustGenerator::toTypeName(as.name);
    QString out;
    QTextStream s(&out);

    // An entity's fields are typed by the DataTypes and entities alongside it,
    // which production/mod.rs re-exports.
    s << "use super::*;\n\n";
    // PartialEq because a Collection of this entity compares elements to delete
    // or update them; without it those methods do not satisfy their own bounds.
    s << "#[derive(Debug, Clone, PartialEq)]\n";
    s << "pub struct " << name << " {\n";
    for (const Field& f : as.fields) {
        const QString fid = RustGenerator::toIdentifier(f.name);
        const QString rt  = RustGenerator::rustType(f.type);
        s << "    pub " << fid << ": " << rt << ",\n";
    }
    s << "}\n\n";
    s << "impl " << name << " {\n";
    s << "    pub fn new(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString fid = RustGenerator::toIdentifier(as.fields[i].name);
        const QString rt  = RustGenerator::rustType(as.fields[i].type);
        s << fid << ": " << rt;
    }
    s << ") -> Self {\n";
    s << "        Self {";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << " " << RustGenerator::toIdentifier(as.fields[i].name);
    }
    s << " }\n    }\n}\n";
    return out;
}

// Collection → production struct with add/delete/read/update/size
static QString genRustProductionCollection(const Collection& col)
{
    const QString name     = RustGenerator::toTypeName(col.name);
    const QString elemType = RustGenerator::toTypeName(col.elementType);
    QString out;
    QTextStream s(&out);
    // The element type is a sibling in the production module.
    s << "use super::*;\n\n";
    if (!col.minimum.isEmpty())
        s << "pub const " << RustGenerator::toIdentifier(col.name).toUpper()
          << "_MINIMUM: usize = " << col.minimum << ";\n";
    if (!col.maximum.isEmpty())
        s << "pub const " << RustGenerator::toIdentifier(col.name).toUpper()
          << "_MAXIMUM: usize = " << col.maximum << ";\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty()) s << "\n";

    s << "#[derive(Debug, Clone, Default, PartialEq)]\n";
    s << "pub struct " << name << " {\n";
    s << "    items: Vec<" << elemType << ">,\n";
    s << "}\n\n";
    s << "impl " << name << " {\n";
    s << "    pub fn new() -> Self { Self::default() }\n\n";
    s << "    pub fn add(&mut self, item: " << elemType << ") {\n";
    s << "        self.items.push(item);\n    }\n\n";
    s << "    pub fn delete(&mut self, item: &" << elemType << ") -> bool\n";
    s << "    where\n        " << elemType << ": PartialEq,\n    {\n";
    s << "        if let Some(pos) = self.items.iter().position(|x| x == item) {\n";
    s << "            self.items.remove(pos);\n";
    s << "            true\n        } else { false }\n    }\n\n";
    s << "    pub fn read(&self) -> &[" << elemType << "] {\n";
    s << "        &self.items\n    }\n\n";
    s << "    pub fn update(&mut self, old_item: &" << elemType << ", new_item: " << elemType << ") -> bool\n";
    s << "    where\n        " << elemType << ": PartialEq,\n    {\n";
    s << "        if let Some(pos) = self.items.iter().position(|x| x == old_item) {\n";
    s << "            self.items[pos] = new_item;\n";
    s << "            true\n        } else { false }\n    }\n\n";
    s << "    pub fn size(&self) -> usize { self.items.len() }\n";
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool RustGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList RustGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraUses = opts.extraUses;
    m_tagFilter = opts.tagFilter;

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString specSnake  = toIdentifier(file.specName);
    const QString glueStruct = toTypeName(file.specName) + "Glue";

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

    // Copy source .spectable (if enabled)
    if (opts.copySpectable && !file.filePath.isEmpty()) {
        const QString dest = dir.filePath(QFileInfo(file.filePath).fileName());
        QFile::remove(dest);
        if (!QFile::copy(file.filePath, dest))
            msgs << QString("WARNING:0:Could not copy %1 to %2").arg(file.filePath, dest);
    }

    // Synthesize implicit AttrSets for NamedBlocks (same as JavaGenerator)
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

    // Write common structs
    QVector<AttrSet> domainSets;
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        const QString mid = toIdentifier(as.name);
        writeFile(commonDir.filePath(mid + "_string.rs"), genStringStruct(as, augmented), msgs);
        writeFile(commonDir.filePath(mid + "_typed.rs"),  genTypedStruct(as, augmented),  msgs);
        domainSets.push_back(as);
    }
    writeFile(commonDir.filePath("json.rs"), genRustJsonMod(),          msgs);
    {
        // Read the existing index so structs from the other .spectable files survive.
        QString existingMod;
        QFile mf(commonDir.filePath("mod.rs"));
        if (mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            existingMod = QTextStream(&mf).readAll();
            mf.close();
        }
        writeFile(commonDir.filePath("mod.rs"), genCommonMod(domainSets, existingMod, commonDir.path()), msgs);
    }

    // Test file
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specSnake, glueStruct, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(dir.filePath("test_" + specSnake + ".rs"), testContent, msgs);
    }

    // Glue file
    {
        const QString gluePath = dir.filePath(specSnake + "_glue.rs");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, glueStruct), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // Crate wiring. Every .spectable is converted on its own, so the module
    // lists are merged with what previous runs left rather than replaced.
    {
        auto mergeModFile = [&](const QString& path, const QStringList& wanted) {
            QStringList all;
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString existing = QTextStream(&f).readAll();
                f.close();
                for (const QString& l : existing.split('\n')) {
                    const QString t = l.trimmed();
                    if (!t.isEmpty() && !t.startsWith("#!") && !all.contains(t))
                        all << t;
                }
            }
            for (const QString& l : wanted) if (!all.contains(l)) all << l;
            all.sort();
            // A directory named TestFolder is not snake_case, and the generated
            // surface is wider than any one test uses.
            QString content = "#![allow(non_snake_case, unused_imports, dead_code)]\n\n";
            for (const QString& l : all) content += l + "\n";
            writeFile(path, content, msgs);
        };

        const QString glueMod = specSnake + "_glue";
        const QString testMod = "test_" + specSnake;

        if (specSubDir.isEmpty()) {
            QStringList root = { "pub mod common;",
                                 "pub mod " + glueMod + ";",
                                 "pub mod " + testMod + ";" };
            if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty())
                root << "pub mod production;";
            mergeModFile(QDir(opts.outputDir).filePath("lib.rs"), root);
        } else {
            const QString subMod = specSubDir.split('/').last();
            mergeModFile(dir.filePath("mod.rs"),
                         { "pub mod " + glueMod + ";", "pub mod " + testMod + ";" });
            QStringList root = { "pub mod common;", "pub mod " + subMod + ";" };
            if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty())
                root << "pub mod production;";
            mergeModFile(QDir(opts.outputDir).filePath("lib.rs"), root);
        }

        // Cargo.toml is written once and then left alone, like the other build
        // files, so a hand-added dependency survives.
        const QString cargoPath = QDir(opts.outputDir).filePath("Cargo.toml");
        if (!QFile::exists(cargoPath)) {
            QString crate = toIdentifier(QFileInfo(opts.outputDir).fileName());
            if (crate.isEmpty()) crate = "spectable";
            QString cargo;
            QTextStream cs(&cargo);
            cs << "[package]\nname = \"" << crate << "\"\nversion = \"0.1.0\"\n"
               << "edition = \"2021\"\n\n[lib]\npath = \"lib.rs\"\n";
            writeFile(cargoPath, cargo, msgs);
        }
    }

    // Production classes (DataType → struct/enum, Entity → struct, Collection → struct)
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists()) prodDir.mkpath(".");

        // A production file is only ever created, never overwritten. Look for a
        // declaration of the type anywhere in the folder rather than only for the
        // filename we would write, so a developer who groups several classes in one
        // file does not get a duplicate declaration emitted beside their own.
        const sourcescan::ProductionScan prodScan(prodDir.path(), {"*.rs"});
        auto alreadyImplemented = [&](const QString& prodPath, const QString& typeName) {
            if (QFile::exists(prodPath)) return true;
            const QString other = prodScan.declaredIn(typeName);
            if (other.isEmpty()) return false;
            msgs << QString("INFO:0:Production type '%1' is already implemented in %2 "
                            "- no template written").arg(typeName, other);
            return true;
        };

        // Modules are recorded whether or not the file was just written, so a
        // production class kept from an earlier run stays reachable.
        QStringList prodMods;
        auto record = [&](const QString& mid) {
            const QString a = "pub mod " + mid + ";";
            const QString b = "pub use " + mid + "::*;";
            if (!prodMods.contains(a)) prodMods << a;
            if (!prodMods.contains(b)) prodMods << b;
        };

        // DataType ValidValues → struct with is_valid(); EnumerationValues → enum
        for (const NamedBlock& nb : file.namedBlocks) {
            if (nb.isContext || !nb.hasExamples || nb.kind != "DataType") continue;
            const bool isValidValues =
                nb.examples.attrSetName.compare("ValidValues", Qt::CaseInsensitive) == 0;
            const bool isEnum =
                nb.examples.attrSetName.compare("EnumerationValues", Qt::CaseInsensitive) == 0;
            if (!isValidValues && !isEnum) continue;
            const QString mid = toIdentifier(nb.name);
            record(mid);
            const QString prodPath = prodDir.filePath(mid + ".rs");
            if (alreadyImplemented(prodPath, toTypeName(nb.name))) continue;
            writeFile(prodPath, isValidValues ? genRustProductionClass(nb)
                                              : genRustProductionEnum(nb), msgs);
        }

        // Entity → struct + impl new()
        for (const AttrSet& as : file.attrSets) {
            if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
            const QString mid = toIdentifier(as.name);
            record(mid);
            const QString prodPath = prodDir.filePath(mid + ".rs");
            if (alreadyImplemented(prodPath, toTypeName(as.name))) continue;
            writeFile(prodPath, genRustProductionEntity(as, file), msgs);
        }

        // Collection → struct with add/delete/read/update/size
        for (const Collection& col : file.collections) {
            if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
            const QString mid = toIdentifier(col.name);
            record(mid);
            const QString prodPath = prodDir.filePath(mid + ".rs");
            if (alreadyImplemented(prodPath, toTypeName(col.name))) continue;
            writeFile(prodPath, genRustProductionCollection(col), msgs);
        }

        // production/mod.rs, merged the same way as the crate root.
        {
            const QString modPath = prodDir.filePath("mod.rs");
            QStringList all;
            QFile f(modPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString existing = QTextStream(&f).readAll();
                f.close();
                for (const QString& l : existing.split('\n')) {
                    const QString t = l.trimmed();
                    if (!t.isEmpty() && !t.startsWith("#!") && !all.contains(t)) all << t;
                }
                all = sourcescan::dropEntriesForMissingFiles(
                          all, prodDir.path(),
                          QRegularExpression("^pub (?:mod|use) ([A-Za-z0-9_]+)"), ".rs");
            }
            for (const QString& l : prodMods) if (!all.contains(l)) all << l;
            all.sort();
            QString content = "#![allow(non_snake_case, unused_imports, dead_code)]\n\n";
            for (const QString& l : all) content += l + "\n";
            writeFile(modPath, content, msgs);
        }
    }

    return msgs;
}
