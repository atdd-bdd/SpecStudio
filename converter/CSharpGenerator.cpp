#include "CSharpGenerator.h"
#include "TagFilter.h"
#include "SourceScan.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>
#include <algorithm>

// ---------------------------------------------------------------------------
// Cell value helpers
// ---------------------------------------------------------------------------

// Replace ~ with space (tilde is the space placeholder in table cells)
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

// Join a namespace prefix with a suffix; if prefix is empty, return suffix alone.
static QString joinNs(const QString& prefix, const QString& suffix)
{
    return prefix.isEmpty() ? suffix : prefix + "." + suffix;
}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

QString CSharpGenerator::csharpType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")          return "int";
    if (t == "float"   || t == "scientific")   return "double";
    if (t == "decimal")                        return "decimal";
    if (t == "boolean" || t == "yesno"
     || t == "bool")                           return "bool";
    if (t == "date" || t == "time"
     || t == "datetime")                       return "DateTime";
    if (t == "duration")                       return "TimeSpan";
    if (t == "string" || t == "text"
     || t == "character" || t == "char")       return "string";
    // User-defined type: return as-is (caller must have the type defined)
    return specType.trimmed();
}

// Produce the expression used inside To<Name>Typed() to convert one field
QString CSharpGenerator::parseExpr(const QString& field, const QString& specType,
                                   const SpectableFile* file)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")     return QString("int.Parse(this.%1)").arg(field);
    if (t == "float"   || t == "scientific") return QString("double.Parse(this.%1)").arg(field);
    if (t == "decimal")                   return QString("decimal.Parse(this.%1)").arg(field);
    if (t == "boolean" || t == "yesno"
     || t == "bool")                      return QString(
        "(this.%1.Equals(\"true\", System.StringComparison.OrdinalIgnoreCase) "
        "|| this.%1.Equals(\"t\", System.StringComparison.OrdinalIgnoreCase) "
        "|| this.%1.Equals(\"yes\", System.StringComparison.OrdinalIgnoreCase) "
        "|| this.%1.Equals(\"y\", System.StringComparison.OrdinalIgnoreCase) "
        "|| this.%1 == \"1\")").arg(field);
    if (t == "date" || t == "time"
     || t == "datetime")                  return QString("DateTime.Parse(this.%1)").arg(field);
    if (t == "duration")                  return QString("TimeSpan.Parse(this.%1)").arg(field);
    if (t == "string" || t == "text"
     || t == "character" || t == "char")  return QString("this.%1").arg(field);
    // User-defined: wrap in constructor
    return QString("new %1(this.%2)").arg(specType.trimmed()).arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString CSharpGenerator::toClassName(const QString& name)
{
    // Replace non-alnum with '_', ensure it starts with a letter
    QString s = name;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s = s.remove(QRegularExpression("^_+|_+$"));
    if (!s.isEmpty() && s[0].isDigit()) s.prepend("_");
    return s;
}

QString CSharpGenerator::toMethodName(const QString& keyword, const QString& stepText)
{
    // "Given" + "checking account" → "Given_checking_account"
    QString s = keyword + "_" + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s = s.remove(QRegularExpression("^_+|_+$"));
    return s;
}

QString CSharpGenerator::toCamelCase(const QString& fieldName)
{
    // "Transfer Amount" → "transferAmount", "FirstName" → "firstName"
    const QStringList parts = fieldName.split(QRegularExpression(R"([\s_]+)"),
                                               Qt::SkipEmptyParts);
    if (parts.isEmpty()) return fieldName;
    QString result = parts[0][0].toLower() + parts[0].mid(1);
    for (int i = 1; i < parts.size(); ++i)
        result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

// ---------------------------------------------------------------------------
// DataType detection (built-in names + user-declared)
// ---------------------------------------------------------------------------

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

// A field whose type is another Attributes/Entity block. Such a field is
// carried as <Name>String / <Name>Typed, exactly as JavaGenerator does it -
// not as a user DataType constructed from a string.
static bool isAttrSetType(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

// Field type inside the *String class: everything is a string except a nested
// AttributeSet, which is that block's own String class.
static QString stringFieldType(const Field& f, const SpectableFile& file)
{
    return isAttrSetType(f.type, file) ? f.type.trimmed() + "String" : QString("string");
}

// Field type inside the *Typed class.
static QString typedFieldType(const Field& f, const SpectableFile& file)
{
    if (isCollectionType(f.type, file)) {
        const QString elem = collectionElementType(f.type, file);
        const QString inner = isAttrSetType(elem, file) ? elem + "Typed"
                                                        : CSharpGenerator::csharpType(elem);
        return "List<" + inner + ">";
    }
    if (isAttrSetType(f.type, file)) return f.type.trimmed() + "Typed";
    return CSharpGenerator::csharpType(f.type);
}

// ---------------------------------------------------------------------------
// AttrSet / Define lookup
// ---------------------------------------------------------------------------

const AttrSet* CSharpGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0)
            return &as;
    return nullptr;
}

const Define* CSharpGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0)
            return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Table resolution
// ---------------------------------------------------------------------------

// Returns list of value-rows, each in AttrSet field order.
// For a normal table: header row tells us column→field mapping.
// For a vertical table: each row is [AttrName, Value]; we collect one instance.
// For a =DefineName ref: expand the define.
QVector<QStringList> CSharpGenerator::resolveStepRows(
    const Step& step, const AttrSet* attrSet,
    const SpectableFile& file, QStringList& errors)
{
    QVector<QStringList> result;
    if (!attrSet) return result;

    const int fieldCount = attrSet->fields.size();

    // ── Define reference ──────────────────────────────────────────────────────
    if (!step.defineRef.isEmpty()) {
        const Define* def = findDefine(step.defineRef, file);
        if (!def) {
            errors << QString("WARNING:%1:Define '%2' not found")
                      .arg(step.line).arg(step.defineRef);
            return result;
        }
        if (!def->isTable) {
            // Scalar define — one single-cell instance
            QStringList row(fieldCount);
            if (fieldCount > 0) row[0] = def->scalarValue;
            result << row;
            return result;
        }

        // Build field→index map
        QMap<QString, int> fieldIdx;
        for (int i = 0; i < attrSet->fields.size(); ++i)
            fieldIdx[attrSet->fields[i].name.toLower()] = i;

        // A step with Vertical flag and a defineRef means the define rows are key=value pairs.
        // Also, if the define was explicitly detected as vertical (via "Attribute"/"Name" header),
        // treat as key=value. Otherwise if step.vertical, treat as key=value from row 0.
        const bool useKV = def->vertical || step.vertical;
        if (useKV) {
            // key=value rows: each row is [FieldName, Value].
            // If def->vertical: row 0 is the "Attribute/Name" header — skip it.
            // Otherwise: row 0 is the first data row — start from 0.
            const int startIdx = def->vertical ? 1 : 0;
            QStringList row(fieldCount);
            for (int ri = startIdx; ri < def->tableRows.size(); ++ri) {
                const QStringList& r = def->tableRows[ri];
                if (r.size() < 2) continue;
                QString key = r[0].toLower();
                if (fieldIdx.contains(key))
                    row[fieldIdx[key]] = resolveValue(r[1], file);
            }
            result << row;
        } else {
            // Multi-row define (first row = headers)
            if (def->tableRows.isEmpty()) return result;
            const QStringList& hdrs = def->tableRows[0];
            QVector<int> colMap; // colMap[col] = field index (-1 if not found)
            for (const QString& h : hdrs)
                colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);
            for (int ri = 1; ri < def->tableRows.size(); ++ri) {
                QStringList row(fieldCount);
                const QStringList& dr = def->tableRows[ri];
                for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
                    if (colMap[ci] >= 0) row[colMap[ci]] = resolveValue(dr[ci], file);
                result << row;
            }
        }
        return result;
    }

    // ── Inline table ──────────────────────────────────────────────────────────
    if (!step.hasTable || step.table.rows.isEmpty()) return result;

    // Build field→index map
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
            QStringList row(fieldCount);
            for (const QStringList& r : step.table.rows) {
                if (r.size() < 2) continue;
                const QString key = r[0].toLower();
                if (fieldIdx.contains(key) && col < r.size())
                    row[fieldIdx[key]] = resolveValue(r[col], file);
            }
            result << row;
        }
    } else {
        // Normal: rows[0] = header, rows[1..] = data
        if (step.table.rows.size() < 2) return result;
        const QStringList& hdrs = step.table.rows[0];
        QVector<int> colMap;
        for (const QString& h : hdrs)
            colMap << (fieldIdx.contains(h.toLower()) ? fieldIdx[h.toLower()] : -1);
        for (int ri = 1; ri < step.table.rows.size(); ++ri) {
            QStringList row(fieldCount);
            const QStringList& dr = step.table.rows[ri];
            for (int ci = 0; ci < colMap.size() && ci < dr.size(); ++ci)
                if (colMap[ci] >= 0) row[colMap[ci]] = resolveValue(dr[ci], file);
            result << row;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Examples-table row resolution for NamedBlock (BusinessRule / Calc / DataType)
// ---------------------------------------------------------------------------

static QVector<QStringList> resolveExamplesRows(const NamedBlock& block, const AttrSet* as)
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
// String class generator
// ---------------------------------------------------------------------------

QString CSharpGenerator::genStringClass(const AttrSet& as, const QString& ns, const SpectableFile& file) const
{
    const QString cn = as.name + "String";
    QString out;
    QTextStream s(&out);

    s << "namespace " << ns << "\n{\n";
    for (const QString& u : m_extraImports) s << u << "\n";
    if (!m_extraImports.isEmpty()) s << "\n";
    s << "    public class " << cn << "\n    {\n";

    // Fields
    for (const Field& f : as.fields)
        s << "        public string " << toCamelCase(f.name) << ";\n";
    s << "\n";

    // Constructor
    s << "        public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << stringFieldType(as.fields[i], file) << " " << toCamelCase(as.fields[i].name);
    }
    s << ")\n        {\n";
    for (const Field& f : as.fields)
        s << "            this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "        }\n\n";

    // To<Name>Typed()
    const QString tn = as.name + "Typed";
    s << "        public " << tn << " To" << tn << "()\n        {\n";
    s << "            return new " << tn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f = as.fields[i];
        const QString expr = parseExpr(toCamelCase(f.name), f.type, &file);
        s << "                " << expr;
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "            );\n        }\n\n";

    // ToString()
    s << "        public override string ToString()\n        {\n";
    s << "            return $\"";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << as.fields[i].name << "={" << toCamelCase(as.fields[i].name) << "}";
    }
    s << "\";\n        }\n";

    s << "    }\n}\n";
    return out;
}

// ---------------------------------------------------------------------------
// common/Json.cs — typed field accessors over System.Text.Json.
//
// The previous implementation called JsonSerializer.Serialize(this), which
// silently produced "{}" because System.Text.Json ignores public fields, and
// Deserialize needed a parameterless constructor that is never generated.
// Reading and writing are now explicit, field by field.
// ---------------------------------------------------------------------------

static QString genCSharpJsonClass(const QString& ns, const QStringList& extraImports)
{
    QString out;
    QTextStream s(&out);
    s << "namespace " << ns << "\n{\n";
    s << "using System;\n";
    s << "using System.Globalization;\n";
    s << "using System.Text.Json;\n";
    for (const QString& u : extraImports) s << u << "\n";
    s << "\n";
    s << QString::fromLatin1(R"CS(    /// <summary>
    /// Field accessors over System.Text.Json.  A missing key or a value of the
    /// wrong type throws JsonException.  An explicit JSON null is passed through
    /// rather than treated as an error.
    /// </summary>
    public static class Json
    {
        /// <summary>Parse JSON text into a detached element.</summary>
        public static JsonElement Parse(string text)
        {
            if (text == null) throw new JsonException("JSON text is null");
            try
            {
                // Clone() detaches the element so it stays valid after the
                // document is disposed.
                using (var doc = JsonDocument.Parse(text))
                    return doc.RootElement.Clone();
            }
            catch (JsonException ex)
            {
                throw new JsonException("Invalid JSON: " + ex.Message, ex);
            }
        }

        private static string Describe(JsonElement v)
        {
            switch (v.ValueKind)
            {
                case JsonValueKind.Null:
                case JsonValueKind.Undefined: return "null";
                case JsonValueKind.True:
                case JsonValueKind.False:     return "a boolean";
                case JsonValueKind.Number:    return "a number";
                case JsonValueKind.String:    return "a string";
                case JsonValueKind.Array:     return "an array";
                case JsonValueKind.Object:    return "an object";
                default:                      return "a value";
            }
        }

        private static JsonException TypeError(string ctx, string expected, JsonElement actual)
        {
            return new JsonException(
                "JSON field '" + ctx + "' is not " + expected + " (got " + Describe(actual) + ")");
        }

        public static JsonElement Require(JsonElement obj, string key)
        {
            if (obj.ValueKind != JsonValueKind.Object)
                throw new JsonException("Expected an object holding field '" + key + "'");
            JsonElement value;
            if (!obj.TryGetProperty(key, out value))
                throw new JsonException("Missing JSON field '" + key + "'");
            return value;
        }

        public static void RequireArray(JsonElement v, string ctx)
        {
            if (v.ValueKind != JsonValueKind.Array) throw TypeError(ctx, "an array", v);
        }

        /// <summary>Invariant text for any value; used for user-defined DataTypes.</summary>
        public static string ToText(object value)
        {
            return value == null ? null : Convert.ToString(value, CultureInfo.InvariantCulture);
        }

        public static string AsString(JsonElement v, string ctx)
        {
            switch (v.ValueKind)
            {
                case JsonValueKind.Null:
                case JsonValueKind.Undefined: return null;
                case JsonValueKind.String:    return v.GetString();
                case JsonValueKind.Number:    return v.GetRawText();
                case JsonValueKind.True:      return "true";
                case JsonValueKind.False:     return "false";
                default: throw TypeError(ctx, "a string", v);
            }
        }

        public static decimal AsDecimal(JsonElement v, string ctx)
        {
            decimal d;
            if (v.ValueKind == JsonValueKind.Number && v.TryGetDecimal(out d)) return d;
            if (v.ValueKind == JsonValueKind.String
                && decimal.TryParse(v.GetString(), NumberStyles.Any,
                                    CultureInfo.InvariantCulture, out d)) return d;
            throw TypeError(ctx, "a number", v);
        }

        public static double AsDouble(JsonElement v, string ctx)
        {
            double d;
            if (v.ValueKind == JsonValueKind.Number && v.TryGetDouble(out d)) return d;
            if (v.ValueKind == JsonValueKind.String
                && double.TryParse(v.GetString(), NumberStyles.Any,
                                   CultureInfo.InvariantCulture, out d)) return d;
            throw TypeError(ctx, "a number", v);
        }

        public static int AsInt(JsonElement v, string ctx)
        {
            int i;
            if (v.ValueKind == JsonValueKind.Number && v.TryGetInt32(out i)) return i;
            // Accept 7.0 for an integer field, but not 7.5.
            decimal d = AsDecimal(v, ctx);
            if (decimal.Truncate(d) != d || d < int.MinValue || d > int.MaxValue)
                throw TypeError(ctx, "an integer", v);
            return (int)d;
        }

        public static bool AsBool(JsonElement v, string ctx)
        {
            if (v.ValueKind == JsonValueKind.True)  return true;
            if (v.ValueKind == JsonValueKind.False) return false;
            if (v.ValueKind == JsonValueKind.String)
            {
                string t = (v.GetString() ?? string.Empty).Trim().ToLowerInvariant();
                if (t == "true"  || t == "t" || t == "yes" || t == "y" || t == "1") return true;
                if (t == "false" || t == "f" || t == "no"  || t == "n" || t == "0") return false;
            }
            throw TypeError(ctx, "a boolean", v);
        }

        public static DateTime AsDateTime(JsonElement v, string ctx)
        {
            DateTime dt;
            string text = AsString(v, ctx);
            if (text != null && DateTime.TryParse(text, CultureInfo.InvariantCulture,
                                                  DateTimeStyles.RoundtripKind, out dt)) return dt;
            throw TypeError(ctx, "a date/time", v);
        }

        public static TimeSpan AsTimeSpan(JsonElement v, string ctx)
        {
            TimeSpan ts;
            string text = AsString(v, ctx);
            if (text != null && TimeSpan.TryParse(text, CultureInfo.InvariantCulture, out ts)) return ts;
            throw TypeError(ctx, "a duration", v);
        }
    }
)CS");
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Typed class generator
// ---------------------------------------------------------------------------

QString CSharpGenerator::genTypedClass(const AttrSet& as, const QString& ns, const SpectableFile& file) const
{
    const QString cn = as.name + "Typed";
    QString out;
    QTextStream s(&out);

    s << "namespace " << ns << "\n{\n";
    s << "using System;\n";
    s << "using System.Buffers;\n";
    s << "using System.Collections.Generic;\n";
    s << "using System.Globalization;\n";
    s << "using System.Text;\n";
    s << "using System.Text.Json;\n";
    for (const QString& u : m_extraImports) s << u << "\n";
    s << "\n";
    s << "    public class " << cn << "\n    {\n";

    // Fields
    for (const Field& f : as.fields)
        s << "        public " << csharpType(f.type) << " " << toCamelCase(f.name) << ";\n";
    s << "\n";

    // Constructor
    s << "        public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        s << csharpType(f.type) << " " << toCamelCase(f.name);
    }
    s << ")\n        {\n";
    for (const Field& f : as.fields)
        s << "            this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "        }\n\n";

    // ---- JSON: explicit per-field read/write over System.Text.Json ----

    s << "        public void WriteJson(Utf8JsonWriter w)\n        {\n";
    s << "            w.WriteStartObject();\n";
    for (const Field& f : as.fields) {
        const QString fn = toCamelCase(f.name);
        const QString ct = csharpType(f.type);
        if (ct == "int" || ct == "double" || ct == "decimal")
            s << "            w.WriteNumber(\"" << fn << "\", this." << fn << ");\n";
        else if (ct == "bool")
            s << "            w.WriteBoolean(\"" << fn << "\", this." << fn << ");\n";
        else if (ct == "DateTime")
            s << "            w.WriteString(\"" << fn << "\", this." << fn
              << ".ToString(\"o\", CultureInfo.InvariantCulture));\n";
        else if (ct == "TimeSpan")
            s << "            w.WriteString(\"" << fn << "\", this." << fn
              << ".ToString(\"c\", CultureInfo.InvariantCulture));\n";
        else if (ct == "string")
            s << "            w.WriteString(\"" << fn << "\", this." << fn << ");\n";
        else    // user-defined DataType — same text convention parseExpr assumes
            s << "            w.WriteString(\"" << fn << "\", Json.ToText(this." << fn << "));\n";
    }
    s << "            w.WriteEndObject();\n";
    s << "        }\n\n";

    s << "        public string ToJSON()\n        {\n";
    s << "            var buffer = new ArrayBufferWriter<byte>();\n";
    s << "            using (var w = new Utf8JsonWriter(buffer)) { WriteJson(w); }\n";
    s << "            return Encoding.UTF8.GetString(buffer.WrittenSpan);\n";
    s << "        }\n\n";

    s << "        public static " << cn << " FromJsonElement(JsonElement m)\n        {\n";
    s << "            return new " << cn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f   = as.fields[i];
        const QString fn = toCamelCase(f.name);
        const QString ct = csharpType(f.type);
        const QString src = QString("Json.Require(m, \"%1\")").arg(fn);
        QString expr;
        if (ct == "int")           expr = QString("Json.AsInt(%1, \"%2\")").arg(src, fn);
        else if (ct == "double")   expr = QString("Json.AsDouble(%1, \"%2\")").arg(src, fn);
        else if (ct == "decimal")  expr = QString("Json.AsDecimal(%1, \"%2\")").arg(src, fn);
        else if (ct == "bool")     expr = QString("Json.AsBool(%1, \"%2\")").arg(src, fn);
        else if (ct == "DateTime") expr = QString("Json.AsDateTime(%1, \"%2\")").arg(src, fn);
        else if (ct == "TimeSpan") expr = QString("Json.AsTimeSpan(%1, \"%2\")").arg(src, fn);
        else if (ct == "string")   expr = QString("Json.AsString(%1, \"%2\")").arg(src, fn);
        else                       expr = QString("new %1(Json.AsString(%2, \"%3\"))")
                                              .arg(f.type.trimmed(), src, fn);
        s << "                " << expr;
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "            );\n        }\n\n";

    s << "        public static " << cn << " FromJSON(string json)\n        {\n";
    s << "            return FromJsonElement(Json.Parse(json));\n";
    s << "        }\n\n";

    s << "        public static string ToJSONList(List<" << cn << "> list)\n        {\n";
    s << "            var buffer = new ArrayBufferWriter<byte>();\n";
    s << "            using (var w = new Utf8JsonWriter(buffer))\n            {\n";
    s << "                w.WriteStartArray();\n";
    s << "                foreach (var item in list) item.WriteJson(w);\n";
    s << "                w.WriteEndArray();\n";
    s << "            }\n";
    s << "            return Encoding.UTF8.GetString(buffer.WrittenSpan);\n";
    s << "        }\n\n";

    s << "        public static List<" << cn << "> FromJSONList(string json)\n        {\n";
    s << "            var result = new List<" << cn << ">();\n";
    s << "            var root = Json.Parse(json);\n";
    s << "            Json.RequireArray(root, \"" << cn << "\");\n";
    s << "            foreach (var e in root.EnumerateArray()) result.Add(FromJsonElement(e));\n";
    s << "            return result;\n";
    s << "        }\n";

    s << "    }\n}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Test file generator
// ---------------------------------------------------------------------------

QString CSharpGenerator::genTestFile(const SpectableFile& file, const QString& ns,
                                     const QString& className, QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    s << "namespace " << ns << "{\n";
    s << "using Microsoft.VisualStudio.TestTools.UnitTesting;\n";
    s << "using System.Collections.Generic;\n";
    s << "using " << m_commonNs << ";\n";
    for (const QString& u : m_extraImports) s << u << "\n";
    s << "\n[TestClass]\n";
    s << "public class " << className << "{\n\n";

    // Collect all steps (background + per-scenario) into one method
    int objectCounter = 0;

    auto emitSteps = [&](const QVector<Step>& steps, const QString& glueVar) {
        for (const Step& step : steps) {
            if (step.hasDocString) {
                const QString meth = toMethodName(step.keyword, step.text);
                QString esc = step.docString;
                esc.replace("\\", "\\\\");
                esc.replace("\"", "\\\"");
                esc.replace("\n", "\\n");
                s << "         " << glueVar << "." << meth << "(\"" << esc << "\");\n\n";
                continue;
            }
            if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                if (def && def->hasDocString) {
                    const QString meth = toMethodName(step.keyword, step.text);
                    QString esc = def->docString;
                    esc.replace("\\", "\\\\");
                    esc.replace("\"", "\\\"");
                    esc.replace("\n", "\\n");
                    s << "         " << glueVar << "." << meth << "(\"" << esc << "\");\n\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                // Bare step — call with no arguments
                const QString meth = toMethodName(step.keyword, step.text);
                s << "         " << glueVar << "." << meth << "();\n\n";
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
                // DataType grid step — fall through to the List<List<string>> branch below
            }

            if (!step.attrSetName.isEmpty() && as) {
                // Typed list
                ++objectCounter;
                const QString listType  = effectiveAttrSetName + "String";
                const QString listVar   = QString("objectList%1").arg(objectCounter);

                QStringList localErrs;
                QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;

                s << "         List<" << listType << "> " << listVar
                  << " = new List<" << listType << ">{\n";
                for (const QStringList& row : rows) {
                    s << "             new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ",";
                        s << "\"" << row[ci] << "\"";
                    }
                    s << "),\n";
                }
                s << "         };\n";
                const QString meth = toMethodName(step.keyword, step.text);
                s << "         " << glueVar << "." << meth << "(" << listVar << ");\n\n";

            } else if (step.hasTable && as == nullptr) {
                // No matching AttrSet: use List<List<string>>
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);

                const StepTable& tbl = step.table;
                // DataType/primitive-typed steps have no header — all rows are data.
                // For normal multi-column tables, rows[0] is the column-name header.
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;

                s << "         List<List<string>> " << listVar
                  << " = new List<List<string>>{\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "            new List<string>{ ";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << resolveValue(r[ci], file) << "\"";
                    }
                    s << " },\n";
                }
                s << "         };\n";
                const QString meth = toMethodName(step.keyword, step.text);
                s << "         " << glueVar << "." << meth << "(" << listVar << ");\n\n";
            }
        }
    };

    auto emitCsCategories = [&](const QStringList& tags) {
        for (const QString& t : tags) s << "[TestCategory(\"" << t << "\")]\n";
    };

    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;
        const QString meth = "Test_Scenario_" + toClassName(sc.name);
        const QString glueClass = className + "_glue";
        const QString glueVar   = glueClass[0].toLower() + glueClass.mid(1) + "_object";

        emitCsCategories(file.tags + sc.tags);
        s << "[TestMethod]\n";
        s << "public void " << meth << "(){\n";
        s << "     " << glueClass << " " << glueVar << " = new " << glueClass << "();\n\n";

        // Background steps first
        emitSteps(file.backgroundSteps, glueVar);
        // Then scenario-specific steps
        emitSteps(sc.steps, glueVar);

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

        s << "// -------------------------\n";
        s << "// " << kind << " Tests\n";
        s << "// -------------------------\n";

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

            emitCsCategories(nb.tags);
            s << "[TestMethod]\n";
            s << "public void " << meth << "(){\n";
            s << "     " << glueClass << " glue = new " << glueClass << "();\n";

            if (as) {
                ++objectCounter;
                const QString listType = nb.examples.attrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                s << "     List<" << listType << "> " << listVar
                  << " = new List<" << listType << ">{\n";
                for (const QStringList& row : rows) {
                    s << "         new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ",";
                        s << "\"" << row[ci] << "\"";
                    }
                    s << "),\n";
                }
                s << "     };\n";
                s << "     glue." << glueMeth << "(" << listVar << ");\n";
            } else {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "     List<List<string>> " << listVar
                  << " = new List<List<string>>{\n";
                for (const QStringList& row : rows) {
                    s << "         new List<string>{ ";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << resolveValue(row[ci], file) << "\"";
                    }
                    s << " },\n";
                }
                s << "     };\n";
                s << "     glue." << glueMeth << "(" << listVar << ");\n";
            }
            s << "}\n\n";
        }
    }

    s << "}\n}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Glue file generator
// ---------------------------------------------------------------------------

QVector<CSharpGenerator::GlueSig> CSharpGenerator::collectGlueSigs(const SpectableFile& file)
{
    QVector<GlueSig> sigs;
    QSet<QString> seen;

    auto collectSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            const QString meth = toMethodName(step.keyword, step.text);
            if (seen.contains(meth)) continue;
            seen.insert(meth);
            if (step.hasDocString) {
                sigs.push_back({ meth, "docstring", false });
            } else if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                sigs.push_back({ meth, (def && def->hasDocString) ? "docstring" : "", false });
            } else if (step.attrSetName.isEmpty() && !step.hasTable) {
                sigs.push_back({ meth, "", false });           // void / no parameter
            } else if (!step.attrSetName.isEmpty() && !isDataType(step.attrSetName, file)) {
                const QString effectiveName = isCollectionType(step.attrSetName, file)
                    ? collectionElementType(step.attrSetName, file)
                    : step.attrSetName;
                sigs.push_back({ meth, effectiveName + "String", true });
            } else if (!step.attrSetName.isEmpty() && isDataType(step.attrSetName, file)) {
                sigs.push_back({ meth, "List<List<string>>", false });  // grid
            } else {
                sigs.push_back({ meth, "List<string>", true });
            }
        }
    };

    collectSteps(file.backgroundSteps);
    collectSteps(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios)
        collectSteps(sc.steps);

    // Named blocks — emit ExamplesBusinessRule_*, ExamplesCalculation_*, ExamplesDataType_*
    for (const NamedBlock& nb : file.namedBlocks) {
        // Context blocks belong to another .spectable and are tested there.
        // Emitting stubs for them here produced glue methods no test calls.
        if (!nb.hasExamples || nb.isContext) continue;
        const QString meth = "Examples_" + nb.kind + "_" + toClassName(nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr
            : findAttrSet(nb.examples.attrSetName, file);
        if (as)
            sigs.push_back({ meth, nb.examples.attrSetName + "String", true });
        else
            sigs.push_back({ meth, "List<List<string>>", false });
    }

    return sigs;
}

QString CSharpGenerator::genStubMethod(const GlueSig& sig)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "        public void " << sig.method << "()\n";
        s << "        {\n";
        s << "            Assert.Fail(\"Not implemented: " << sig.method << "\");\n";
        s << "        }\n";
        return out;
    }
    if (sig.paramType == "docstring") {
        s << "        public void " << sig.method << "(string value)\n";
        s << "        {\n";
        s << "            Console.WriteLine(value);\n";
        s << "            Assert.Fail(\"Not implemented: " << sig.method << "\");\n";
        s << "        }\n";
        return out;
    }
    const QString paramType = sig.isList
        ? QString("List<%1>").arg(sig.paramType)
        : sig.paramType;
    s << "        public void " << sig.method << "(" << paramType << " values)\n";
    s << "        {\n";
    if (sig.paramType == "List<List<string>>") {
        s << "            foreach (var row in values)\n";
        s << "            {\n";
        s << "                Console.WriteLine(string.Join(\", \", row));\n";
        s << "            }\n";
    } else {
        s << "            foreach (var value in values)\n";
        s << "            {\n";
        s << "                Console.WriteLine(value);\n";
        s << "            }\n";
    }
    s << "            Assert.Fail(\"Not implemented: " << sig.method << "\");\n";
    s << "        }\n";
    return out;
}

bool CSharpGenerator::appendMissingStubs(const QString& gluePath,
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
        const QString signature = QStringLiteral("public void %1(").arg(sig.method);
        if (!scan.contains(signature))
            stubs += "\n" + genStubMethod(sig);
    }
    if (stubs.isEmpty()) return false;

    // Insert before the closing "    }\n}\n" of the class
    const int closingClass = content.lastIndexOf("    }\n}");
    if (closingClass < 0) {
        msgs << QString("WARNING:0:Could not locate class closing brace in %1 — stubs not added")
                .arg(gluePath);
        return false;
    }

    content.insert(closingClass, stubs + "\n");

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

QString CSharpGenerator::genGlueFile(const SpectableFile& file, const QString& ns,
                                     const QString& className) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    const QString glueClass = className + "_glue";
    QString out;
    QTextStream s(&out);

    s << "namespace " << ns << "\n{\n";
    s << "    using System;\n";
    s << "    using System.Collections.Generic;\n";
    s << "    using " << m_commonNs << ";\n";
    // The namespace import is what makes the qualified `Assert.Fail(...)` in
    // the generated stubs compile; the static import additionally allows the
    // unqualified `Fail(...)` / `AreEqual(...)` that hand-written glue uses.
    s << "    using Microsoft.VisualStudio.TestTools.UnitTesting;\n";
    s << "    using static Microsoft.VisualStudio.TestTools.UnitTesting.Assert;\n";
    for (const QString& u : m_extraImports) s << "    " << u << "\n";
    s << "\n";
    s << "    public class " << glueClass << "\n    {\n";
    s << "        const string DNCString = \"?DNC?\";\n\n";

    for (const GlueSig& sig : sigs)
        s << genStubMethod(sig) << "\n";

    s << "    }\n}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Production class generators
// ---------------------------------------------------------------------------

// ValidValues DataType → class with IsValid() method
static QString genCSharpProductionClass(const NamedBlock& nb, const QString& ns)
{
    const QString name = nb.name;
    QString out;
    QTextStream s(&out);
    s << "namespace " << ns << "\n{\n";
    s << "    using System;\n\n";
    s << "    public class " << name << "\n    {\n";
    s << "        public readonly string Value;\n\n";
    s << "        public " << name << "(string value)\n        {\n";
    s << "            Value = value ?? string.Empty;\n";
    s << "        }\n\n";
    // Collect valid values from ValidValues table
    int valueCol = -1;
    for (int i = 0; i < nb.examples.header.size(); ++i)
        if (nb.examples.header[i].trimmed().compare("value", Qt::CaseInsensitive) == 0)
            { valueCol = i; break; }
    if (valueCol >= 0) {
        QStringList vals;
        for (const QStringList& row : nb.examples.rows)
            if (valueCol < row.size() && !row[valueCol].trimmed().isEmpty())
                vals << "\"" + row[valueCol].trimmed() + "\"";
        if (!vals.isEmpty()) {
            s << "        private static readonly string[] ValidValues = { " << vals.join(", ") << " };\n\n";
            s << "        public bool IsValid() =>\n";
            s << "            System.Array.Exists(ValidValues, v => v.Equals(Value, StringComparison.OrdinalIgnoreCase));\n\n";
        }
    }
    s << "        public override string ToString() => $\"" << name << "{{Value}}\";\n";
    s << "        public override bool Equals(object? obj) =>\n";
    s << "            obj is " << name << " other && string.Equals(Value, other.Value, StringComparison.OrdinalIgnoreCase);\n";
    s << "        public override int GetHashCode() => Value.GetHashCode(StringComparison.OrdinalIgnoreCase);\n";
    s << "    }\n}\n";
    return out;
}

// EnumerationValues DataType → enum
static QString genCSharpProductionEnum(const NamedBlock& nb, const QString& ns)
{
    int valueCol = -1, notesCol = -1;
    for (int i = 0; i < nb.examples.header.size(); ++i) {
        const QString h = nb.examples.header[i].trimmed().toLower();
        if (h == "value") valueCol = i;
        if (h == "notes") notesCol = i;
    }
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
    s << "namespace " << ns << "\n{\n";
    if (hasNotes) {
        s << "    // Description attribute requires System.ComponentModel\n";
        s << "    using System.ComponentModel;\n\n";
    }
    s << "    public enum " << name << "\n    {\n";
    for (int i = 0; i < constants.size(); ++i) {
        if (hasNotes && !constants[i].note.isEmpty()) {
            QString escaped = constants[i].note;
            escaped.replace("\\","\\\\").replace("\"","\\\"");
            s << "        [Description(\"" << escaped << "\")]\n";
        }
        s << "        " << constants[i].name;
        if (i + 1 < constants.size()) s << ",";
        s << "\n";
    }
    s << "    }\n}\n";
    return out;
}

// Entity AttrSet → production class with constructor + Builder
static QString genCSharpProductionEntity(const AttrSet& as, const QString& ns,
                                          const SpectableFile& file)
{
    (void)file;  // reserved for cross-entity field type lookup
    const QString name = as.name;
    QString out;
    QTextStream s(&out);
    s << "namespace " << ns << "\n{\n";
    s << "    public class " << name << "\n    {\n";

    // Helper: field name → PascalCase property name
    auto toProp = [](const QString& name) -> QString {
        const QString cc = CSharpGenerator::toCamelCase(name);
        if (cc.isEmpty()) return name;
        return cc[0].toUpper() + cc.mid(1);
    };

    // Properties (read-only)
    for (const Field& f : as.fields) {
        const QString csType = CSharpGenerator::csharpType(f.type);
        s << "        public " << csType << " " << toProp(f.name) << " { get; }\n";
    }
    s << "\n";

    // Constructor
    s << "        public " << name << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        s << CSharpGenerator::csharpType(f.type) << " " << CSharpGenerator::toCamelCase(f.name);
    }
    s << ")\n        {\n";
    for (const Field& f : as.fields)
        s << "            " << toProp(f.name) << " = " << CSharpGenerator::toCamelCase(f.name) << ";\n";
    s << "        }\n\n";

    // Builder nested class
    s << "        public class Builder\n        {\n";
    for (const Field& f : as.fields)
        s << "            private " << CSharpGenerator::csharpType(f.type)
          << " _" << CSharpGenerator::toCamelCase(f.name) << ";\n";
    s << "\n";
    for (const Field& f : as.fields) {
        const QString fn = CSharpGenerator::toCamelCase(f.name);
        s << "            public Builder " << toProp(f.name)
          << "(" << CSharpGenerator::csharpType(f.type) << " value) { _" << fn << " = value; return this; }\n";
    }
    s << "\n            public " << name << " Build() =>\n";
    s << "                new " << name << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << "_" << CSharpGenerator::toCamelCase(as.fields[i].name);
    }
    s << ");\n        }\n";

    s << "    }\n}\n";
    return out;
}

// Collection → production class with add/delete/read/update
static QString genCSharpProductionCollection(const Collection& col, const QString& ns)
{
    const QString className = col.name;
    const QString elemType  = col.elementType;
    QString out;
    QTextStream s(&out);
    s << "namespace " << ns << "\n{\n";
    s << "    using System.Collections.Generic;\n\n";
    s << "    public class " << className << "\n    {\n";
    if (!col.minimum.isEmpty())
        s << "        public const int Minimum = " << col.minimum << ";\n";
    if (!col.maximum.isEmpty())
        s << "        public const int Maximum = " << col.maximum << ";\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty()) s << "\n";
    s << "        private readonly List<" << elemType << "> _items = new();\n\n";
    s << "        public void Add(" << elemType << " item) => _items.Add(item);\n\n";
    s << "        public bool Delete(" << elemType << " item) => _items.Remove(item);\n\n";
    s << "        public IReadOnlyList<" << elemType << "> Read() => _items.AsReadOnly();\n\n";
    s << "        public bool Update(" << elemType << " oldItem, " << elemType << " newItem)\n        {\n";
    s << "            int idx = _items.IndexOf(oldItem);\n";
    s << "            if (idx < 0) return false;\n";
    s << "            _items[idx] = newItem;\n";
    s << "            return true;\n";
    s << "        }\n\n";
    s << "        public int Count => _items.Count;\n";
    s << "    }\n}\n";
    return out;
}

// ---------------------------------------------------------------------------
// File writing helper
// ---------------------------------------------------------------------------

bool CSharpGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList CSharpGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraImports = opts.extraImports;
    m_tagFilter    = opts.tagFilter;
    m_commonNs     = joinNs(opts.nsPrefix, "common");

    // Typed classes name production DataTypes (SimpleText, Dollar, ...) directly,
    // so the production namespace has to be in scope or nothing compiles. Java
    // injects the equivalent import; C# was missing it entirely.
    if (opts.createProductionClasses) {
        const QString prodNs = opts.productionClassesNamespace.isEmpty()
                             ? joinNs(opts.nsPrefix, "domain")
                             : opts.productionClassesNamespace;
        const QString prodUsing = "using " + prodNs + ";";
        if (!prodNs.isEmpty() && !m_extraImports.contains(prodUsing))
            m_extraImports.prepend(prodUsing);
    }

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString className = toClassName(file.specName);

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

    const QString ns = joinNs(opts.nsPrefix, className);

    QDir dir(specSubDir.isEmpty() ? opts.outputDir : opts.outputDir + "/" + specSubDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create output directory: %1").arg(dir.path());
        return msgs;
    }

    // Common classes go into outputDir/common/
    QDir commonDir(opts.outputDir + "/common");
    if (!commonDir.exists() && !commonDir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create common directory: %1").arg(commonDir.path());
        return msgs;
    }
    const QString commonNs = joinNs(opts.nsPrefix, "common");

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
            for (const QString& col : nb.examples.header) {
                const QString c = col.trimmed();
                if (!c.isEmpty()) {
                    Field f; f.name = c;
                    f.type = (isValidValues && c.compare("isvalid", Qt::CaseInsensitive) == 0)
                             ? "YesNo" : "String";
                    sa.fields.push_back(f);
                }
            }
            if (!sa.fields.isEmpty())
                augmented.attrSets.push_back(sa);
        }
    }

    // Copy the source .spectable file into the output folder (if enabled)
    if (opts.copySpectable && !file.filePath.isEmpty()) {
        const QString destPath = dir.filePath(QFileInfo(file.filePath).fileName());
        QFile::remove(destPath);
        if (!QFile::copy(file.filePath, destPath))
            msgs << QString("WARNING:0:Could not copy %1 to %2").arg(file.filePath, destPath);
    }

    writeFile(commonDir.filePath("Json.cs"), genCSharpJsonClass(commonNs, m_extraImports), msgs);

    // 1. String + Typed classes for each AttrSet → go into common/
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }

        writeFile(commonDir.filePath(as.name + "String.cs"), genStringClass(as, commonNs, augmented), msgs);
        writeFile(commonDir.filePath(as.name + "Typed.cs"),  genTypedClass(as, commonNs, augmented),  msgs);
    }

    // 2. Unit test file (always overwritten, but only if no errors)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, ns, className, testErrs);
        msgs << testErrs;
        const bool testHasErrors = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!testHasErrors)
            writeFile(dir.filePath(className + "_Tests.cs"), testContent, msgs);
    }

    // 3. Glue file: write fresh if absent/overwrite; otherwise append any missing stubs
    {
        const QString gluePath = dir.filePath(className + "_glue.cs");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, ns, className), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // 4. Production classes (DataType → class/enum, Entity → class+Builder, Collection → class)
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        const QString prodNs = opts.productionClassesNamespace.isEmpty()
                               ? joinNs(opts.nsPrefix, "domain") : opts.productionClassesNamespace;
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists()) prodDir.mkpath(".");

        // DataType ValidValues → class; EnumerationValues → enum
        for (const NamedBlock& nb : file.namedBlocks) {
            if (nb.isContext || !nb.hasExamples || nb.kind != "DataType") continue;
            const bool isValidValues =
                nb.examples.attrSetName.compare("ValidValues", Qt::CaseInsensitive) == 0;
            const bool isEnum =
                nb.examples.attrSetName.compare("EnumerationValues", Qt::CaseInsensitive) == 0;
            if (!isValidValues && !isEnum) continue;
            const QString prodPath = prodDir.filePath(nb.name + ".cs");
            if (QFile::exists(prodPath)) continue;
            if (isValidValues)
                writeFile(prodPath, genCSharpProductionClass(nb, prodNs), msgs);
            else
                writeFile(prodPath, genCSharpProductionEnum(nb, prodNs), msgs);
        }

        // Entity → production class with Builder
        for (const AttrSet& as : file.attrSets) {
            if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
            const QString prodPath = prodDir.filePath(as.name + ".cs");
            if (QFile::exists(prodPath)) continue;
            writeFile(prodPath, genCSharpProductionEntity(as, prodNs, file), msgs);
        }

        // Collection → production class
        for (const Collection& col : file.collections) {
            if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
            const QString prodPath = prodDir.filePath(col.name + ".cs");
            if (QFile::exists(prodPath)) continue;
            writeFile(prodPath, genCSharpProductionCollection(col, prodNs), msgs);
        }
    }

    return msgs;
}
