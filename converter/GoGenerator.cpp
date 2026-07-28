#include "GoGenerator.h"
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

// ---------------------------------------------------------------------------
// Cell / value helpers
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

static QString goEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('"',  "\\\"");
    r.replace('\n', "\\n");
    return r;
}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

// A cell for a nested-object field is written "=SomeDefine"; expand that define
// against the nested Attributes block and build its String struct literal.
QString GoGenerator::goNestedLiteral(const QString& cellValue, const QString& fieldType,
                                      const SpectableFile& file, const QString& prefix)
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
        return goStringLiteral(subAs, row, file, prefix);
    }
    return "\"" + goEscape(cellValue) + "\"";
}

// A struct literal for one row, used when the block has a nested-object field
// and so cannot be built from a flat []string.
QString GoGenerator::goStringLiteral(const AttrSet& as, const QStringList& row,
                                      const SpectableFile& file, const QString& prefix)
{
    QString expr = prefix + toExported(as.name) + "String{";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) expr += ", ";
        const QString cell = (i < row.size()) ? row[i] : QString();
        expr += toExported(as.fields[i].name) + ": ";
        if (isAttrSetType(as.fields[i].type, file))
            expr += goNestedLiteral(cell, as.fields[i].type, file, prefix);
        else
            expr += "\"" + goEscape(cell) + "\"";
    }
    return expr + "}";
}

bool GoGenerator::isAttrSetType(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

// The type a field takes inside the common package. A nested Attributes block
// becomes that block's own Typed struct. A user DataType lives in the
// production package, which common must not depend on, so its value is carried
// as text — the glue converts it when it needs the production object.
QString GoGenerator::goCommonType(const Field& f, const SpectableFile& file)
{
    if (isAttrSetType(f.type, file)) return toExported(f.type) + "Typed";

    static const QSet<QString> builtin = {
        "integer", "int", "float", "decimal", "scientific", "boolean", "yesno",
        "bool", "string", "text", "character", "char", "date", "time",
        "datetime", "duration"
    };
    if (!builtin.contains(f.type.trimmed().toLower())) return "string";
    return goType(f.type);
}

QString GoGenerator::goType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")                                 return "int";
    if (t == "float"   || t == "decimal" || t == "scientific")        return "float64";
    if (t == "boolean" || t == "yesno" || t == "bool")               return "bool";
    if (t == "string"  || t == "text" || t == "character" || t == "char") return "string";
    if (t == "date"    || t == "time" || t == "datetime" || t == "duration") return "string";
    return specType.trimmed();
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString GoGenerator::toIdentifier(const QString& name)
{
    QString s = name.trimmed();
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    if (!s.isEmpty() && s[0].isDigit()) s.prepend('_');
    return s.toLower();
}

QString GoGenerator::toExported(const QString& name)
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

QString GoGenerator::toMethodName(const QString& keyword, const QString& stepText)
{
    QString s = keyword + " " + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), " ");
    const QStringList parts = s.split(' ', Qt::SkipEmptyParts);
    QString result;
    for (const QString& p : parts)
        if (!p.isEmpty()) result += p[0].toUpper() + p.mid(1);
    if (!result.isEmpty() && result[0].isDigit()) result.prepend('_');
    return result;
}

QString GoGenerator::toPackageName(const QString& name)
{
    QString s = toIdentifier(name);
    s.remove('_');
    return s.toLower();
}

// ---------------------------------------------------------------------------
// DataType / collection detection
// ---------------------------------------------------------------------------

bool GoGenerator::isDataType(const QString& name, const SpectableFile& file)
{
    static const QStringList builtins = {
        "Character","String","Text","Integer","Float","Scientific","Decimal","Boolean",
        "Date","Time","DateTime","Duration","YesNo"
    };
    for (const QString& b : builtins)
        if (b.compare(name, Qt::CaseInsensitive) == 0) return true;
    for (const QString& d : file.dataTypeNames)
        if (d.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

bool GoGenerator::isCollectionType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

QString GoGenerator::collectionElementType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return c.elementType;
    return {};
}

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

const AttrSet* GoGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return &as;
    return nullptr;
}

const Define* GoGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Row resolution
// ---------------------------------------------------------------------------

QVector<QStringList> GoGenerator::resolveStepRows(
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
        QMap<QString,int> fieldIdx;
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

    QMap<QString,int> fieldIdx;
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

QVector<QStringList> GoGenerator::resolveExamplesRows(
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
    QMap<QString,int> fieldIdx;
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
// String struct
// ---------------------------------------------------------------------------

QString GoGenerator::genStringStruct(const AttrSet& as, const QString& pkg,
                                     const SpectableFile& file) const
{
    const QString typeName = toExported(as.name) + "String";
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << "\n\nimport \"fmt\"\n\n";

    // A field whose type names another Attributes block holds that block's
    // String struct, not a bare string.
    auto fieldType = [&](const Field& f) {
        return isAttrSetType(f.type, file) ? toExported(f.type) + "String"
                                          : QString("string");
    };

    s << "type " << typeName << " struct {\n";
    for (const Field& f : as.fields)
        s << "\t" << toExported(f.name) << " " << fieldType(f) << "\n";
    s << "}\n\n";

    s << "func New" << typeName << "FromSlice(v []string) " << typeName << " {\n";
    s << "\ts := " << typeName << "{}\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        // A nested block cannot come from a flat slice; those rows are built
        // with a struct literal instead.
        if (isAttrSetType(as.fields[i].type, file)) continue;
        s << "\tif len(v) > " << i << " { s." << toExported(as.fields[i].name)
          << " = v[" << i << "] }\n";
    }
    s << "\treturn s\n}\n\n";

    s << "func (s " << typeName << ") String() string {\n";
    s << "\treturn fmt.Sprintf(\"";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << as.fields[i].name << (isAttrSetType(as.fields[i].type, file) ? "=%v" : "=%s");
    }
    s << "\"";
    for (const Field& f : as.fields)
        s << ", s." << toExported(f.name);
    s << ")\n}\n\n";

    // A field holding the Do-Not-Care marker on either side matches whatever
    // the other side holds — that is what lets a CompareOnly step name only the
    // columns it cares about. Use Equals, not ==, to compare rows. DNCEqual
    // itself lives in common.go, which every String struct shares.
    s << "func (s " << typeName << ") Equals(o " << typeName << ") bool {\n";
    if (as.fields.isEmpty()) {
        s << "\treturn true\n";
    } else {
        // Go inserts a semicolon at a line break, so the operator has to stay
        // at the end of the line rather than starting the next one.
        s << "\treturn ";
        for (int i = 0; i < as.fields.size(); ++i) {
            if (i) s << " &&\n\t\t";
            const QString fn = toExported(as.fields[i].name);
            // A nested block delegates to its own Equals; the marker is a text
            // convention and does not apply at that level.
            if (isAttrSetType(as.fields[i].type, file))
                s << "s." << fn << ".Equals(o." << fn << ")";
            else
                s << "DNCEqual(s." << fn << ", o." << fn << ")";
        }
        s << "\n";
    }
    s << "}\n\n";

    s << "func Equal" << typeName << "Slices(a, b []" << typeName << ") bool {\n";
    s << "\tif len(a) != len(b) { return false }\n";
    s << "\tfor i := range a {\n";
    s << "\t\tif !a[i].Equals(b[i]) { return false }\n";
    s << "\t}\n\treturn true\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed struct
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// common/json.go — field accessors over the standard-library encoding/json.
// Numbers are decoded with UseNumber() so large integers and decimals keep
// their exact text instead of being forced through float64.
// ---------------------------------------------------------------------------

static QString genGoJsonHelpers(const QString& pkg)
{
    QString out;
    QTextStream s(&out);
    s << "package " << pkg << "\n\n";
    s << QString::fromLatin1(R"GO(// Field accessors over encoding/json.
//
// A missing key or a value of the wrong type returns an error. An explicit
// JSON null is passed through as the zero value rather than treated as an
// error.

import (
	"bytes"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
)

func jsonDecode(text string) (interface{}, error) {
	dec := json.NewDecoder(strings.NewReader(text))
	dec.UseNumber() // keep numbers exact instead of coercing to float64
	var v interface{}
	if err := dec.Decode(&v); err != nil {
		return nil, fmt.Errorf("invalid JSON: %w", err)
	}
	// Reject trailing content after the first value. More() peeks at the next
	// non-whitespace byte, so it catches invalid trailing bytes too; Token()
	// would return an error for those and be mistaken for a clean end of input.
	if dec.More() {
		return nil, fmt.Errorf("invalid JSON: trailing content after top-level value")
	}
	return v, nil
}

// JSONParseObject parses JSON text that must hold an object.
func JSONParseObject(text string) (map[string]interface{}, error) {
	v, err := jsonDecode(text)
	if err != nil {
		return nil, err
	}
	m, ok := v.(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("expected a JSON object, got %s", jsonDescribe(v))
	}
	return m, nil
}

// JSONParseArray parses JSON text that must hold an array.
func JSONParseArray(text string) ([]interface{}, error) {
	v, err := jsonDecode(text)
	if err != nil {
		return nil, err
	}
	a, ok := v.([]interface{})
	if !ok {
		return nil, fmt.Errorf("expected a JSON array, got %s", jsonDescribe(v))
	}
	return a, nil
}

// JSONWrite serializes a value graph without escaping HTML characters.
func JSONWrite(v interface{}) (string, error) {
	var buf bytes.Buffer
	enc := json.NewEncoder(&buf)
	enc.SetEscapeHTML(false)
	if err := enc.Encode(v); err != nil {
		return "", err
	}
	return strings.TrimRight(buf.String(), "\n"), nil
}

func jsonDescribe(v interface{}) string {
	switch v.(type) {
	case nil:
		return "null"
	case bool:
		return "a boolean"
	case json.Number, float64:
		return "a number"
	case string:
		return "a string"
	case []interface{}:
		return "an array"
	case map[string]interface{}:
		return "an object"
	}
	return fmt.Sprintf("%T", v)
}

func jsonTypeError(ctx, expected string, actual interface{}) error {
	return fmt.Errorf("JSON field '%s' is not %s (got %s)", ctx, expected, jsonDescribe(actual))
}

// JSONRequire returns the named member, or an error when it is absent.
func JSONRequire(m map[string]interface{}, key string) (interface{}, error) {
	if m == nil {
		return nil, fmt.Errorf("expected an object holding field '%s'", key)
	}
	v, ok := m[key]
	if !ok {
		return nil, fmt.Errorf("missing JSON field '%s'", key)
	}
	return v, nil
}

// JSONAsString coerces a JSON scalar to a string.
func JSONAsString(v interface{}, ctx string) (string, error) {
	switch t := v.(type) {
	case nil:
		return "", nil
	case string:
		return t, nil
	case json.Number:
		return t.String(), nil
	case float64:
		return strconv.FormatFloat(t, 'g', -1, 64), nil
	case bool:
		if t {
			return "true", nil
		}
		return "false", nil
	}
	return "", jsonTypeError(ctx, "a string", v)
}

// JSONAsFloat coerces a JSON number (or numeric string) to float64.
func JSONAsFloat(v interface{}, ctx string) (float64, error) {
	switch t := v.(type) {
	case json.Number:
		f, err := t.Float64()
		if err != nil {
			return 0, jsonTypeError(ctx, "a number", v)
		}
		return f, nil
	case float64:
		return t, nil
	case string:
		f, err := strconv.ParseFloat(strings.TrimSpace(t), 64)
		if err != nil {
			return 0, jsonTypeError(ctx, "a number", v)
		}
		return f, nil
	}
	return 0, jsonTypeError(ctx, "a number", v)
}

// JSONAsInt accepts 7 and 7.0 for an integer field, but not 7.5.
func JSONAsInt(v interface{}, ctx string) (int, error) {
	if n, ok := v.(json.Number); ok {
		if i, err := n.Int64(); err == nil {
			return int(i), nil
		}
	}
	f, err := JSONAsFloat(v, ctx)
	if err != nil {
		return 0, err
	}
	i := int(f)
	if float64(i) != f {
		return 0, jsonTypeError(ctx, "an integer", v)
	}
	return i, nil
}

// JSONAsBool accepts a JSON boolean or the usual truthy/falsy spellings.
func JSONAsBool(v interface{}, ctx string) (bool, error) {
	switch t := v.(type) {
	case bool:
		return t, nil
	case string:
		switch strings.ToLower(strings.TrimSpace(t)) {
		case "true", "t", "yes", "y", "1":
			return true, nil
		case "false", "f", "no", "n", "0":
			return false, nil
		}
	}
	return false, jsonTypeError(ctx, "a boolean", v)
}

// JSONAsObject asserts that a value is a JSON object.
func JSONAsObject(v interface{}, ctx string) (map[string]interface{}, error) {
	if v == nil {
		return nil, nil
	}
	m, ok := v.(map[string]interface{})
	if !ok {
		return nil, jsonTypeError(ctx, "an object", v)
	}
	return m, nil
}
)GO");
    return out;
}

QString GoGenerator::genTypedStruct(const AttrSet& as, const QString& pkg,
                                     const SpectableFile& file) const
{
    const QString strType   = toExported(as.name) + "String";
    const QString typedName = toExported(as.name) + "Typed";
    QString out;
    QTextStream s(&out);

    // Only the int and float64 branches call strconv; the bool branch compares
    // strings. Including bool here emitted an unused import, which Go rejects.
    bool needsStrconv = false;
    for (const Field& f : as.fields) {
        const QString gt = goCommonType(f, file);
        if (gt == "int" || gt == "float64") { needsStrconv = true; break; }
    }

    s << "package " << pkg << "\n\n";
    if (needsStrconv) s << "import \"strconv\"\n\n";

    s << "type " << typedName << " struct {\n";
    for (const Field& f : as.fields) {
        const QString gt = goCommonType(f, file);
        s << "\t" << toExported(f.name) << " " << gt << "\n";
    }
    s << "}\n\n";

    s << "func New" << typedName << "FromString(s " << strType << ") " << typedName << " {\n";
    s << "\tt := " << typedName << "{}\n";
    for (const Field& f : as.fields) {
        const QString fe = toExported(f.name);
        const QString gt = goCommonType(f, file);
        const QString sf = "s." + fe;
        if (gt == "int") {
            s << "\tif v, err := strconv.Atoi(" << sf << "); err == nil { t." << fe << " = v }\n";
        } else if (gt == "float64") {
            s << "\tif v, err := strconv.ParseFloat(" << sf << ", 64); err == nil { t." << fe << " = v }\n";
        } else if (gt == "bool") {
            // A spec writes Yes/No/True/False in any casing, so the comparison
            // has to be case-insensitive; ParseBoolCell lives in common.go.
            s << "\tt." << fe << " = ParseBoolCell(" << sf << ")\n";
        } else if (gt == "string") {
            s << "\tt." << fe << " = " << sf << "\n";
        } else {
            // Nested Attributes block — build its own Typed struct.
            s << "\tt." << fe << " = New" << gt << "FromString(" << sf << ")\n";
        }
    }
    s << "\treturn t\n}\n\n";

    // ---- JSON (encoding/json; see common/json.go) ----

    s << "// ToJSONValue renders the struct as a plain map for encoding/json.\n";
    s << "func (t " << typedName << ") ToJSONValue() map[string]interface{} {\n";
    s << "\treturn map[string]interface{}{\n";
    for (const Field& f : as.fields) {
        const QString fe = toExported(f.name);
        const QString gt = goCommonType(f, file);
        const QString key = toIdentifier(f.name);
        if (gt == "int" || gt == "float64" || gt == "bool" || gt == "string")
            s << "\t\t\"" << key << "\": t." << fe << ",\n";
        else    // nested Attributes block
            s << "\t\t\"" << key << "\": t." << fe << ".ToJSONValue(),\n";
    }
    s << "\t}\n}\n\n";

    s << "func (t " << typedName << ") ToJSON() (string, error) {\n";
    s << "\treturn JSONWrite(t.ToJSONValue())\n";
    s << "}\n\n";

    s << "func New" << typedName << "FromJSONValue(m map[string]interface{}) ("
      << typedName << ", error) {\n";
    s << "\tt := " << typedName << "{}\n";
    for (const Field& f : as.fields) {
        const QString fe  = toExported(f.name);
        const QString gt  = goCommonType(f, file);
        const QString key = toIdentifier(f.name);
        const bool nested = !(gt == "int" || gt == "float64"
                           || gt == "bool" || gt == "string");
        const QString fn  = nested            ? "JSONAsObject"
                          : (gt == "int")     ? "JSONAsInt"
                          : (gt == "float64") ? "JSONAsFloat"
                          : (gt == "bool")    ? "JSONAsBool"
                                              : "JSONAsString";
        s << "\traw" << fe << ", err := JSONRequire(m, \"" << key << "\")\n";
        s << "\tif err != nil {\n\t\treturn t, err\n\t}\n";
        s << "\tval" << fe << ", err := " << fn << "(raw" << fe << ", \"" << key << "\")\n";
        s << "\tif err != nil {\n\t\treturn t, err\n\t}\n";
        if (!nested) {
            s << "\tt." << fe << " = val" << fe << "\n";
        } else {
            // Nested Attributes block — read it as its own Typed struct.
            s << "\tsub" << fe << ", err := New" << gt << "FromJSONValue(val" << fe << ")\n";
            s << "\tif err != nil {\n\t\treturn t, err\n\t}\n";
            s << "\tt." << fe << " = sub" << fe << "\n";
        }
    }
    s << "\treturn t, nil\n}\n\n";

    s << "func New" << typedName << "FromJSON(text string) (" << typedName << ", error) {\n";
    s << "\tm, err := JSONParseObject(text)\n";
    s << "\tif err != nil {\n\t\treturn " << typedName << "{}, err\n\t}\n";
    s << "\treturn New" << typedName << "FromJSONValue(m)\n";
    s << "}\n\n";

    s << "func " << typedName << "ToJSONList(list []" << typedName << ") (string, error) {\n";
    s << "\tarr := make([]interface{}, 0, len(list))\n";
    s << "\tfor _, item := range list {\n";
    s << "\t\tarr = append(arr, item.ToJSONValue())\n";
    s << "\t}\n";
    s << "\treturn JSONWrite(arr)\n";
    s << "}\n\n";

    s << "func " << typedName << "FromJSONList(text string) ([]" << typedName << ", error) {\n";
    s << "\traw, err := JSONParseArray(text)\n";
    s << "\tif err != nil {\n\t\treturn nil, err\n\t}\n";
    s << "\tresult := make([]" << typedName << ", 0, len(raw))\n";
    s << "\tfor _, e := range raw {\n";
    s << "\t\tm, err := JSONAsObject(e, \"" << typedName << "\")\n";
    s << "\t\tif err != nil {\n\t\t\treturn nil, err\n\t\t}\n";
    s << "\t\titem, err := New" << typedName << "FromJSONValue(m)\n";
    s << "\t\tif err != nil {\n\t\t\treturn nil, err\n\t\t}\n";
    s << "\t\tresult = append(result, item)\n";
    s << "\t}\n";
    s << "\treturn result, nil\n";
    s << "}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Glue sig collection
// ---------------------------------------------------------------------------

QVector<GoGenerator::GlueSig> GoGenerator::collectGlueSigs(const SpectableFile& file)
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
                const QString effName = isCollectionType(step.attrSetName, file)
                    ? collectionElementType(step.attrSetName, file) : step.attrSetName;
                sigs.push_back({ meth, effName + "String" });
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
        const QString meth = "Examples" + toExported(nb.kind) + toExported(nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr : findAttrSet(nb.examples.attrSetName, file);
        sigs.push_back({ meth, as ? (nb.examples.attrSetName + "String") : "grid" });
    }

    return sigs;
}

QString GoGenerator::genStubFn(const GlueSig& sig, const QString& glueType, bool failEveryTest)
{
    QString out;
    QTextStream s(&out);
    const QString recv = "func (g *" + glueType + ") ";

    if (sig.paramType.isEmpty()) {
        s << recv << sig.method << "(t *testing.T) {\n";
        if (failEveryTest)
            s << "\tt.Fatal(\"Not implemented: " << sig.method << "\")\n";
        s << "}\n";
    } else if (sig.paramType == "docstring") {
        s << recv << sig.method << "(t *testing.T, value string) {\n";
        s << "\t_ = value\n";
        if (failEveryTest)
            s << "\tt.Fatal(\"Not implemented: " << sig.method << "\")\n";
        s << "}\n";
    } else if (sig.paramType == "grid") {
        s << recv << sig.method << "(t *testing.T, values [][]string) {\n";
        s << "\t_ = values\n";
        if (failEveryTest)
            s << "\tt.Fatal(\"Not implemented: " << sig.method << "\")\n";
        s << "}\n";
    } else {
        const QString pt = toExported(sig.paramType);
        s << recv << sig.method << "(t *testing.T, values []" << pt << ") {\n";
        s << "\t_ = values\n";
        if (failEveryTest)
            s << "\tt.Fatal(\"Not implemented: " << sig.method << "\")\n";
        s << "}\n";
    }
    return out;
}

bool GoGenerator::appendMissingStubs(const QString& gluePath,
                                      const QVector<GlueSig>& sigs,
                                      const QString& glueType,
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
        if (!scan.contains(QStringLiteral("func (g *%1) %2(").arg(glueType, sig.method)))
            stubs += "\n" + genStubFn(sig, glueType, failEveryTest);
    }
    if (stubs.isEmpty()) return false;

    content += stubs;

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

// ---------------------------------------------------------------------------
// Glue file
// ---------------------------------------------------------------------------

QString GoGenerator::genGlueFile(const SpectableFile& file, const QString& specPkg,
                                  const QString& glueType) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);

    bool needsCommon = false;
    for (const GlueSig& sig : sigs)
        if (!sig.paramType.isEmpty() && sig.paramType != "grid" && sig.paramType != "docstring")
            { needsCommon = true; break; }

    QString out;
    QTextStream s(&out);

    s << "package " << specPkg << "\n\n";
    s << "import (\n\t\"testing\"\n";
    if (needsCommon) s << "\t\"" << m_modulePath << "/common\"\n";
    s << ")\n\n";

    s << "type " << glueType << " struct {\n\t// Add state fields here\n}\n\n";
    s << "func New" << glueType << "() *" << glueType << " { return &" << glueType << "{} }\n";

    for (const GlueSig& sig : sigs) {
        s << "\n";
        if (!sig.paramType.isEmpty() && sig.paramType != "grid" && sig.paramType != "docstring") {
            const QString pt = toExported(sig.paramType);
            s << "func (g *" << glueType << ") " << sig.method
              << "(t *testing.T, values []common." << pt << ") {\n";
            s << "\t_ = values\n";
            if (m_failEveryTest)
                s << "\tt.Fatal(\"Not implemented: " << sig.method << "\")\n";
            s << "}\n";
        } else {
            s << genStubFn(sig, glueType, m_failEveryTest);
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Test file
// ---------------------------------------------------------------------------

QString GoGenerator::genTestFile(const SpectableFile& file, const QString& specPkg,
                                  const QString& glueType, QStringList& errors) const
{
    // The body is built first so the import block can be decided from what the
    // body actually references — Go rejects an import that is not used.
    QString out;
    QTextStream s(&out);

    int objectCounter = 0;

    auto emitSteps = [&](const QVector<Step>& steps, const QString& glueVar) {
        for (const Step& step : steps) {
            if (step.hasDocString) {
                const QString meth = toMethodName(step.keyword, step.text);
                QString esc = step.docString;
                esc.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n");
                s << "\t" << glueVar << "." << meth << "(t, \"" << esc << "\")\n";
                continue;
            }
            if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                if (def && def->hasDocString) {
                    const QString meth = toMethodName(step.keyword, step.text);
                    QString esc = def->docString;
                    esc.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n");
                    s << "\t" << glueVar << "." << meth << "(t, \"" << esc << "\")\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                s << "\t" << glueVar << "." << toMethodName(step.keyword, step.text) << "(t)\n";
                continue;
            }

            const QString effectiveAttrSetName = (!step.attrSetName.isEmpty()
                && isCollectionType(step.attrSetName, file))
                ? collectionElementType(step.attrSetName, file) : step.attrSetName;
            const AttrSet* as = findAttrSet(effectiveAttrSetName, file);

            if (!step.attrSetName.isEmpty() && !as) {
                if (!isDataType(effectiveAttrSetName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
            }

            const QString meth = toMethodName(step.keyword, step.text);

            if (!step.attrSetName.isEmpty() && as) {
                ++objectCounter;
                const QString listType = "common." + toExported(effectiveAttrSetName) + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                QStringList localErrs;
                const QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;

                // A block with a nested-object field cannot be built from a
                // flat []string, so those rows use a struct literal instead.
                bool hasNested = false;
                for (const Field& f : as->fields)
                    if (isAttrSetType(f.type, file)) { hasNested = true; break; }

                s << "\t" << listVar << " := []" << listType << "{\n";
                for (const QStringList& row : rows) {
                    if (hasNested) {
                        s << "\t\t" << goStringLiteral(*as, row, file, "common.") << ",\n";
                        continue;
                    }
                    s << "\t\tcommon.New" << toExported(effectiveAttrSetName)
                      << "StringFromSlice([]string{";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << goEscape(row[ci]) << "\"";
                    }
                    s << "}),\n";
                }
                s << "\t}\n";
                s << "\t" << glueVar << "." << meth << "(t, " << listVar << ")\n";

            } else {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const StepTable& tbl = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;

                s << "\t" << listVar << " := [][]string{\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "\t\t{";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << goEscape(resolveValue(r[ci], file)) << "\"";
                    }
                    s << "},\n";
                }
                s << "\t}\n";
                s << "\t" << glueVar << "." << meth << "(t, " << listVar << ")\n";
            }
        }
    };

    // Scenarios
    for (const Scenario& sc : file.scenarios) {
        const QStringList effTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effTags)) continue;

        s << "func Test" << toExported(file.specName) << "_Scenario_"
          << toExported(sc.name) << "(t *testing.T) {\n";
        s << "\tglue := New" << glueType << "()\n";
        emitSteps(file.backgroundSteps, "glue");
        emitSteps(sc.steps, "glue");
        s << "}\n\n";
    }

    // BusinessRule / Calculation / DataType
    static const QStringList namedKinds = { "BusinessRule", "Calculation", "DataType" };
    QSet<QString> seenNamed;
    for (const QString& kind : namedKinds) {
        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind || nb.isContext) continue;
            const QString blockKey = kind + ":" + nb.name.toLower();
            if (seenNamed.contains(blockKey)) {
                errors << QString("WARNING:%1:%2 '%3' declared in multiple files")
                              .arg(nb.line).arg(kind).arg(nb.name);
                continue;
            }
            seenNamed.insert(blockKey);
            const QStringList effTags = file.generatorTags + nb.generatorTags;
            if (!TagFilter::matches(m_tagFilter, effTags)) continue;

            // Every specification in this directory shares one Go package, so
            // the specification name has to be part of the test function name
            // or two files declaring the same DataType would collide.
            const QString fn = "Test" + toExported(file.specName) + "_"
                             + toExported(kind) + "_" + toExported(nb.name);
            const QString glueMeth = "Examples" + toExported(kind) + toExported(nb.name);
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr : findAttrSet(nb.examples.attrSetName, file);

            s << "func " << fn << "(t *testing.T) {\n";
            s << "\tglue := New" << glueType << "()\n";

            if (as) {
                ++objectCounter;
                const QString listType = "common." + toExported(nb.examples.attrSetName) + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                s << "\t" << listVar << " := []" << listType << "{\n";
                for (const QStringList& row : rows) {
                    s << "\t\tcommon.New" << toExported(nb.examples.attrSetName)
                      << "StringFromSlice([]string{";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << goEscape(row[ci]) << "\"";
                    }
                    s << "}),\n";
                }
                s << "\t}\n";
                s << "\tglue." << glueMeth << "(t, " << listVar << ")\n";
            } else {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "\t" << listVar << " := [][]string{\n";
                for (const QStringList& row : rows) {
                    s << "\t\t{";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << goEscape(resolveValue(row[ci], file)) << "\"";
                    }
                    s << "},\n";
                }
                s << "\t}\n";
                s << "\tglue." << glueMeth << "(t, " << listVar << ")\n";
            }
            s << "}\n\n";
        }
    }

    QString header;
    QTextStream h(&header);
    h << "package " << specPkg << "\n\n";
    h << "import (\n\t\"testing\"\n";
    if (out.contains("common.")) h << "\t\"" << m_modulePath << "/common\"\n";
    h << ")\n\n";
    return header + out;
}

// ---------------------------------------------------------------------------
// Production generators
// ---------------------------------------------------------------------------

// A spec field can be named "Type", which lowercases to a Go keyword and will
// not parse as a parameter name.
QString GoGenerator::goParamName(const QString& fieldName)
{
    static const QSet<QString> keywords = {
        "break", "case", "chan", "const", "continue", "default", "defer", "else",
        "fallthrough", "for", "func", "go", "goto", "if", "import", "interface",
        "map", "package", "range", "return", "select", "struct", "switch", "type",
        "var"
    };
    const QString id = toIdentifier(fieldName);
    return keywords.contains(id) ? id + "Value" : id;
}

QString GoGenerator::genProductionEntity(const AttrSet& as, const QString& pkg) const
{
    const QString name = toExported(as.name);
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << "\n\n";
    s << "type " << name << " struct {\n";
    for (const Field& f : as.fields)
        s << "\t" << toExported(f.name) << " " << goType(f.type) << "\n";
    s << "}\n\n";

    s << "func New" << name << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << goParamName(as.fields[i].name) << " " << goType(as.fields[i].type);
    }
    s << ") *" << name << " {\n";
    s << "\treturn &" << name << "{";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << toExported(as.fields[i].name) << ": " << goParamName(as.fields[i].name);
    }
    s << "}\n}\n";

    return out;
}

QString GoGenerator::genProductionCollection(const Collection& col, const QString& pkg) const
{
    const QString name     = toExported(col.name);
    const QString elemType = toExported(col.elementType);
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << "\n\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty()) {
        s << "const (\n";
        if (!col.minimum.isEmpty())
            s << "\t" << name << "Minimum = " << col.minimum << "\n";
        if (!col.maximum.isEmpty())
            s << "\t" << name << "Maximum = " << col.maximum << "\n";
        s << ")\n\n";
    }

    s << "type " << name << " struct {\n\titems []*" << elemType << "\n}\n\n";
    s << "func New" << name << "() *" << name << " { return &" << name << "{} }\n\n";

    s << "func (c *" << name << ") Add(item *" << elemType << ") {\n";
    s << "\tc.items = append(c.items, item)\n}\n\n";

    s << "func (c *" << name << ") Delete(item *" << elemType << ") bool {\n";
    s << "\tfor i, v := range c.items {\n";
    s << "\t\tif v == item {\n";
    s << "\t\t\tc.items = append(c.items[:i], c.items[i+1:]...)\n";
    s << "\t\t\treturn true\n\t\t}\n\t}\n\treturn false\n}\n\n";

    s << "func (c *" << name << ") Read() []*" << elemType << " {\n";
    s << "\tresult := make([]*" << elemType << ", len(c.items))\n";
    s << "\tcopy(result, c.items)\n\treturn result\n}\n\n";

    s << "func (c *" << name << ") Update(oldItem, newItem *" << elemType << ") bool {\n";
    s << "\tfor i, v := range c.items {\n";
    s << "\t\tif v == oldItem {\n";
    s << "\t\t\tc.items[i] = newItem\n";
    s << "\t\t\treturn true\n\t\t}\n\t}\n\treturn false\n}\n\n";

    s << "func (c *" << name << ") Size() int { return len(c.items) }\n";

    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool GoGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList GoGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraImports = opts.extraImports;
    m_tagFilter    = opts.tagFilter;
    m_failEveryTest = opts.failEveryTest;

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString specId    = toPackageName(file.specName);   // file-name stem
    const QString glueType  = toExported(file.specName) + "Glue";
    const QString commonPkg = "common";

    // Go allows exactly one package per directory, so the package name comes
    // from the directory, not from the specification. The module path is the
    // output root's own name, which is what makes "<module>/common" resolve.
    m_modulePath = toPackageName(QFileInfo(opts.outputDir).fileName());
    if (m_modulePath.isEmpty()) m_modulePath = "spectable";

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

    // The package each generated file declares, taken from its own directory.
    const QString specPkg = toPackageName(QFileInfo(dir.absolutePath()).fileName());

    // go.mod makes the tree a module so "<module>/common" resolves. Written
    // once and then left alone, like the other build files.
    {
        const QString modPath = QDir(opts.outputDir).filePath("go.mod");
        if (!QFile::exists(modPath)) {
            QString mod;
            QTextStream ms(&mod);
            ms << "module " << m_modulePath << "\n\ngo 1.21\n";
            writeFile(modPath, mod, msgs);
        }
    }

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
                f.type = (isVV && c.compare("isvalid", Qt::CaseInsensitive) == 0) ? "YesNo" : "String";
                sa.fields.push_back(f);
            }
            if (!sa.fields.isEmpty()) augmented.attrSets.push_back(sa);
        }
    }

    // Write common package marker
    {
        QString commonGo;
        QTextStream s(&commonGo);
        s << "package " << commonPkg << "\n\nimport \"strings\"\n\n";
        s << "// Unused suppresses \"imported and not used\" errors during development.\n";
        s << "var Unused = struct{}{}\n\n";
        s << "// DNCString is the Do-Not-Care marker a CompareOnly step puts in\n";
        s << "// every column it does not name.\n";
        s << "const DNCString = \"?DNC?\"\n\n";
        s << "// DNCEqual compares two cells, treating the marker as a wildcard.\n";
        s << "func DNCEqual(a, b string) bool {\n";
        s << "\treturn a == b || a == DNCString || b == DNCString\n}\n\n";
        s << "// ParseBoolCell reads the Yes/No/True/False text a spec cell may\n";
        s << "// hold, in any casing.\n";
        s << "func ParseBoolCell(v string) bool {\n";
        s << "\tswitch strings.ToLower(strings.TrimSpace(v)) {\n";
        s << "\tcase \"true\", \"t\", \"yes\", \"y\", \"1\":\n\t\treturn true\n";
        s << "\t}\n\treturn false\n}\n";
        writeFile(commonDir.filePath("common.go"), commonGo, msgs);
    }

    writeFile(commonDir.filePath("json.go"), genGoJsonHelpers(commonPkg), msgs);

    // Write String + Typed structs for each AttrSet
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        const QString mid = toIdentifier(as.name);
        writeFile(commonDir.filePath(mid + "_string.go"),
                  genStringStruct(as, commonPkg, augmented), msgs);
        writeFile(commonDir.filePath(mid + "_typed.go"),
                  genTypedStruct(as, commonPkg, augmented), msgs);
    }

    // Test file (always overwritten)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specPkg, glueType, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(dir.filePath(specId + "_test.go"), testContent, msgs);
    }

    // Glue file
    {
        const QString gluePath = dir.filePath(specId + "_glue.go");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, specPkg, glueType), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, glueType, msgs, m_failEveryTest))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // Production classes
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        // Go resolves an import by directory, so the package has to be named
        // after the folder the files land in.
        const QString prodPkg = opts.productionClassesPackage.isEmpty()
            ? toPackageName(QFileInfo(opts.productionClassesDir).fileName())
            : opts.productionClassesPackage;
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists()) prodDir.mkpath(".");

        // A production file is only ever created, never overwritten. Look for a
        // declaration of the type anywhere in the folder rather than only for the
        // filename we would write, so a developer who groups several classes in one
        // file does not get a duplicate declaration emitted beside their own.
        const sourcescan::ProductionScan prodScan(prodDir.path(), {"*.go"});
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
            const QString prodPath = prodDir.filePath(toIdentifier(as.name) + ".go");
            if (alreadyImplemented(prodPath, toExported(as.name))) continue;
            writeFile(prodPath, genProductionEntity(as, prodPkg), msgs);
        }

        for (const Collection& col : file.collections) {
            if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
            const QString prodPath = prodDir.filePath(toIdentifier(col.name) + ".go");
            if (alreadyImplemented(prodPath, toExported(col.name))) continue;
            writeFile(prodPath, genProductionCollection(col, prodPkg), msgs);
        }
    }

    return msgs;
}
