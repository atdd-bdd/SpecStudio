#include "PythonGenerator.h"
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

static QString pyEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('\'', "\\'");
    r.replace('\n', "\\n");
    return r;
}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

QString PythonGenerator::pythonType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")                          return "int";
    if (t == "float"   || t == "scientific")                   return "float";
    if (t == "decimal")                                        return "decimal.Decimal";
    if (t == "boolean" || t == "yesno" || t == "bool")         return "bool";
    // date/time/duration — str without external dependencies
    return "str";
}

QString PythonGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("int(s.%1) if s.%1 else 0").arg(field);
    if (t == "float" || t == "scientific")
        return QString("float(s.%1) if s.%1 else 0.0").arg(field);
    if (t == "decimal")
        return QString("decimal.Decimal(s.%1) if s.%1 else decimal.Decimal('0')").arg(field);
    if (t == "boolean" || t == "yesno" || t == "bool")
        return QString("s.%1.lower() in ('true', 't', 'yes', 'y', '1')").arg(field);
    // str / date / time / datetime / duration / user-defined
    return QString("s.%1").arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString PythonGenerator::toIdentifier(const QString& name)
{
    // Convert to snake_case: split on non-alnum, lowercase
    QString s = name.trimmed();
    // Insert underscore before uppercase letters that follow lowercase (camelCase → snake_case)
    static QRegularExpression camelRe("([a-z])([A-Z])");
    s.replace(camelRe, "\\1_\\2");
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    if (!s.isEmpty() && s[0].isDigit()) s.prepend('_');
    return s.toLower();
}

QString PythonGenerator::toTypeName(const QString& name)
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

QString PythonGenerator::toMethodName(const QString& keyword, const QString& stepText)
{
    QString s = keyword + "_" + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    return s.toLower();
}

QString PythonGenerator::toModuleName(const QString& name)
{
    return toIdentifier(name);
}

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

bool PythonGenerator::isDataType(const QString& name, const SpectableFile& file)
{
    static const QStringList builtins = {
        "Character", "String", "Text", "Integer", "Float", "Scientific", "Boolean",
        "Date", "Time", "DateTime", "Duration", "YesNo", "Bool", "Int", "Decimal"
    };
    for (const QString& b : builtins)
        if (b.compare(name, Qt::CaseInsensitive) == 0) return true;
    for (const QString& d : file.dataTypeNames)
        if (d.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

bool PythonGenerator::isCollectionType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

QString PythonGenerator::collectionElementType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return c.elementType;
    return {};
}

const AttrSet* PythonGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return &as;
    return nullptr;
}

const Define* PythonGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Table resolution (mirrors CSharpGenerator / RustGenerator)
// ---------------------------------------------------------------------------

QVector<QStringList> PythonGenerator::resolveStepRows(
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

        if (def->vertical || step.vertical) {
            QStringList row(fieldCount);
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
                QStringList row(fieldCount);
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
// Examples-table row resolution for NamedBlock
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
// String class generator  →  {name}_string.py
// ---------------------------------------------------------------------------

QString PythonGenerator::genStringClass(const AttrSet& as) const
{
    const QString cn  = toTypeName(as.name) + "String";
    QString out;
    QTextStream s(&out);

    for (const QString& imp : m_extraImports) s << imp << "\n";
    if (!m_extraImports.isEmpty()) s << "\n";

    s << "class " << cn << ":\n";

    // __init__
    s << "    def __init__(self";
    for (const Field& f : as.fields)
        s << ", " << toIdentifier(f.name) << ": str = ''";
    s << "):\n";
    for (const Field& f : as.fields)
        s << "        self." << toIdentifier(f.name) << " = " << toIdentifier(f.name) << "\n";
    s << "\n";

    // from_list classmethod
    s << "    @classmethod\n";
    s << "    def from_list(cls, values):\n";
    s << "        v = list(values)\n";
    s << "        return cls(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const QString fid = toIdentifier(as.fields[i].name);
        s << "            v[" << i << "] if len(v) > " << i << " else ''";
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "        )\n\n";

    // __str__
    s << "    def __str__(self):\n";
    s << "        return (";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << " +\n                ";
        const QString fname = as.fields[i].name;
        const QString fid   = toIdentifier(fname);
        s << "f'" << fname << "={self." << fid << "}'";
        if (i < as.fields.size() - 1) s << " + ', '";
    }
    s << ")\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed class generator  →  {name}_typed.py
// ---------------------------------------------------------------------------

QString PythonGenerator::genTypedClass(const AttrSet& as) const
{
    const QString strCn  = toTypeName(as.name) + "String";
    const QString typedCn = toTypeName(as.name) + "Typed";
    const QString strMod  = toModuleName(as.name) + "_string";
    QString out;
    QTextStream s(&out);

    bool needsDecimal = false;
    for (const Field& f : as.fields)
        if (pythonType(f.type) == "decimal.Decimal") { needsDecimal = true; break; }

    if (needsDecimal) s << "import decimal\n";
    s << "from . import json_util as _json\n";
    s << "from ." << strMod << " import " << strCn << "\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";

    s << "class " << typedCn << ":\n";

    // __init__
    s << "    def __init__(self";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        const QString pt  = pythonType(f.type);
        const QString def = (pt == "int") ? "0" : (pt == "float") ? "0.0"
                          : (pt == "decimal.Decimal") ? "decimal.Decimal('0')"
                          : (pt == "bool") ? "False" : "''";
        s << ", " << fid << ": " << pt << " = " << def;
    }
    s << "):\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        s << "        self." << fid << " = " << fid << "\n";
    }
    s << "\n";

    // from_string_obj classmethod
    s << "    @classmethod\n";
    s << "    def from_string_obj(cls, s: " << strCn << ") -> '" << typedCn << "':\n";
    s << "        return cls(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f   = as.fields[i];
        const QString fid = toIdentifier(f.name);
        const QString expr = parseExpr(fid, f.type);
        s << "            " << expr;
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "        )\n\n";

    // ---- JSON (standard-library json module only) ----

    s << "    def to_json_value(self) -> dict:\n";
    s << "        return {\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        s << "            '" << fid << "': self." << fid << ",\n";
    }
    s << "        }\n\n";

    s << "    def to_json(self) -> str:\n";
    s << "        return _json.dumps(self.to_json_value())\n\n";

    s << "    @classmethod\n";
    s << "    def from_json_value(cls, m: dict) -> '" << typedCn << "':\n";
    s << "        return cls(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f    = as.fields[i];
        const QString fid = toIdentifier(f.name);
        const QString pt  = pythonType(f.type);
        const QString fn  = (pt == "int")             ? "as_int"
                          : (pt == "float")           ? "as_float"
                          : (pt == "decimal.Decimal") ? "as_decimal"
                          : (pt == "bool")            ? "as_bool"
                                                      : "as_str";
        s << "            _json." << fn << "(_json.require(m, '" << fid << "'), '" << fid << "')";
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "        )\n\n";

    s << "    @classmethod\n";
    s << "    def from_json(cls, text: str) -> '" << typedCn << "':\n";
    s << "        return cls.from_json_value(_json.loads(text))\n\n";

    s << "    @staticmethod\n";
    s << "    def to_json_list(items) -> str:\n";
    s << "        return _json.dumps([item.to_json_value() for item in items])\n\n";

    s << "    @classmethod\n";
    s << "    def from_json_list(cls, text: str) -> list:\n";
    s << "        raw = _json.as_list(_json.loads(text), '" << typedCn << "')\n";
    s << "        return [cls.from_json_value(e) for e in raw]\n";

    return out;
}

// ---------------------------------------------------------------------------
// common/__init__.py
// ---------------------------------------------------------------------------

// Every .spectable in a project generates into the same common/ package, and
// this index is rewritten on each one. Emitting only the current file's
// AttrSets meant the last file processed erased every class contributed by the
// others, so `from common import *` resolved almost nothing. The existing
// index is therefore merged with the new entries instead of replaced.
QString PythonGenerator::genCommonInit(const QVector<AttrSet>& attrSets,
                                        const QString& existing) const
{
    QStringList lines;
    for (const QString& line : existing.split('\n')) {
        const QString t = line.trimmed();
        if (!t.isEmpty() && !lines.contains(t)) lines << t;
    }
    for (const AttrSet& as : attrSets) {
        const QString cn  = toTypeName(as.name);
        const QString mod = toModuleName(as.name);
        const QString a = QString("from .%1_string import %2String").arg(mod, cn);
        const QString b = QString("from .%1_typed import %2Typed").arg(mod, cn);
        if (!lines.contains(a)) lines << a;
        if (!lines.contains(b)) lines << b;
    }
    lines.sort();
    return lines.join('\n') + "\n";
}

// ---------------------------------------------------------------------------
// common/json_util.py — thin field accessors over the standard-library json
// module.  No third-party package required.  Numbers are parsed as Decimal so
// that decimal fields survive the round trip exactly.
// ---------------------------------------------------------------------------

static QString genJsonUtil()
{
    return QString::fromLatin1(R"PY("""Field accessors layered over the standard-library json module.

A missing key or a value of the wrong type raises ValueError.  An explicit
JSON null is passed through as None rather than treated as an error.
"""

import decimal
import json


def loads(text):
    """Parse JSON text, keeping numbers exact (floats become Decimal)."""
    try:
        return json.loads(text, parse_float=decimal.Decimal)
    except json.JSONDecodeError as exc:
        raise ValueError("Invalid JSON: %s" % exc) from exc


def dumps(value):
    """Serialize a value graph.  Decimal is written as a string so no
    precision is lost; the readers here and in the other generated languages
    accept a number or a string for decimal fields."""
    return json.dumps(value, default=_fallback)


def _fallback(obj):
    if isinstance(obj, decimal.Decimal):
        return str(obj)
    raise TypeError("Cannot serialize %r to JSON" % type(obj).__name__)


def _describe(value):
    if value is None:                 return "null"
    if isinstance(value, bool):       return "a boolean"
    if isinstance(value, (int, float, decimal.Decimal)): return "a number"
    if isinstance(value, str):        return "a string"
    if isinstance(value, list):       return "an array"
    if isinstance(value, dict):       return "an object"
    return type(value).__name__


def _type_error(ctx, expected, value):
    return ValueError("JSON field '%s' is not %s (got %s)" % (ctx, expected, _describe(value)))


def require(obj, key):
    if not isinstance(obj, dict):
        raise ValueError("Expected an object holding field '%s'" % key)
    if key not in obj:
        raise ValueError("Missing JSON field '%s'" % key)
    return obj[key]


def as_str(value, ctx):
    if value is None:                 return None
    if isinstance(value, str):        return value
    if isinstance(value, bool):       return "true" if value else "false"
    if isinstance(value, (int, float, decimal.Decimal)): return str(value)
    raise _type_error(ctx, "a string", value)


def as_decimal(value, ctx):
    if isinstance(value, bool):
        raise _type_error(ctx, "a number", value)
    if isinstance(value, decimal.Decimal):
        return value
    if isinstance(value, (int, float)):
        return decimal.Decimal(str(value))
    if isinstance(value, str):
        try:
            return decimal.Decimal(value.strip())
        except decimal.InvalidOperation:
            raise _type_error(ctx, "a number", value) from None
    raise _type_error(ctx, "a number", value)


def as_int(value, ctx):
    d = as_decimal(value, ctx)
    if d != d.to_integral_value():
        raise _type_error(ctx, "an integer", value)
    return int(d)


def as_float(value, ctx):
    return float(as_decimal(value, ctx))


def as_bool(value, ctx):
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        low = value.strip().lower()
        if low in ("true", "t", "yes", "y", "1"):  return True
        if low in ("false", "f", "no", "n", "0"):  return False
    raise _type_error(ctx, "a boolean", value)


def as_list(value, ctx):
    if value is None:               return None
    if isinstance(value, list):     return value
    raise _type_error(ctx, "an array", value)
)PY");
}

// ---------------------------------------------------------------------------
// Test file generator  →  Test_{SpecName}.py   (always overwritten)
// ---------------------------------------------------------------------------

QString PythonGenerator::genTestFile(const SpectableFile& file, const QString& specSnake,
                                      const QString& glueClass, QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    s << "import pytest\n";
    s << "from common import *\n";
    s << "from " << specSnake << "_glue import " << glueClass << "\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";

    int objectCounter = 0;

    auto emitSteps = [&](const QVector<Step>& steps, const QString& glueVar) {
        for (const Step& step : steps) {
            if (step.hasDocString) {
                const QString meth = toMethodName(step.keyword, step.text);
                QString esc = step.docString;
                esc.replace("\\", "\\\\");
                esc.replace("'",  "\\'");
                esc.replace("\n", "\\n");
                s << "    " << glueVar << "." << meth << "('" << esc << "')\n\n";
                continue;
            }
            if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                if (def && def->hasDocString) {
                    const QString meth = toMethodName(step.keyword, step.text);
                    QString esc = def->docString;
                    esc.replace("\\", "\\\\");
                    esc.replace("'",  "\\'");
                    esc.replace("\n", "\\n");
                    s << "    " << glueVar << "." << meth << "('" << esc << "')\n\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                const QString meth = toMethodName(step.keyword, step.text);
                s << "    " << glueVar << "." << meth << "()\n\n";
                continue;
            }

            const QString effectiveAttrSetName =
                (!step.attrSetName.isEmpty() && isCollectionType(step.attrSetName, file))
                ? collectionElementType(step.attrSetName, file)
                : step.attrSetName;
            const AttrSet* as = findAttrSet(effectiveAttrSetName, file);

            if (!step.attrSetName.isEmpty() && as == nullptr) {
                if (!isDataType(effectiveAttrSetName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined — add an 'Attributes %2' block")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
            }

            const QString meth = toMethodName(step.keyword, step.text);

            if (!step.attrSetName.isEmpty() && as) {
                ++objectCounter;
                const QString listType = toTypeName(effectiveAttrSetName) + "String";
                const QString listVar  = QString("object_list_%1").arg(objectCounter);

                QStringList localErrs;
                QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;

                s << "    " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "        " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "'" << pyEscape(row[ci]) << "'";
                    }
                    s << "),\n";
                }
                s << "    ]\n";
                s << "    " << glueVar << "." << meth << "(" << listVar << ")\n\n";

            } else if (step.hasTable && as == nullptr) {
                ++objectCounter;
                const QString listVar = QString("string_list_list_%1").arg(objectCounter);
                const StepTable& tbl  = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                       && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;

                s << "    " << listVar << " = [\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "        [";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "'" << pyEscape(resolveValue(r[ci], file)) << "'";
                    }
                    s << "],\n";
                }
                s << "    ]\n";
                s << "    " << glueVar << "." << meth << "(" << listVar << ")\n\n";
            }
        }
    };

    // ── Scenarios ────────────────────────────────────────────────────────────
    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;

        const QString glueVar = "glue";
        const QString fnName  = "test_Scenario_" + toTypeName(sc.name);

        const QStringList allTags = file.tags + sc.tags;
        for (const QString& tag : allTags)
            s << "@pytest.mark." << tag << "\n";
        s << "def " << fnName << "():\n";
        s << "    " << glueVar << " = " << glueClass << "()\n\n";
        emitSteps(file.backgroundSteps, glueVar);
        emitSteps(sc.steps, glueVar);
        s << "\n";
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

        s << "# --- " << kind << " Tests ---\n\n";

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

            const QString fnName   = "test_" + kind + "_" + toTypeName(nb.name);
            const QString glueMeth = "examples_" + kind + "_" + toTypeName(nb.name);
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            for (const QString& tag : nb.tags) s << "@pytest.mark." << tag << "\n";
            s << "def " << fnName << "():\n";
            s << "    glue = " << glueClass << "()\n";

            if (as) {
                ++objectCounter;
                const QString listType = toTypeName(nb.examples.attrSetName) + "String";
                const QString listVar  = QString("object_list_%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                s << "    " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "        " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "'" << pyEscape(row[ci]) << "'";
                    }
                    s << "),\n";
                }
                s << "    ]\n";
                s << "    glue." << glueMeth << "(" << listVar << ")\n\n";
            } else {
                ++objectCounter;
                const QString listVar = QString("string_list_list_%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "    " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "        [";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "'" << pyEscape(resolveValue(row[ci], file)) << "'";
                    }
                    s << "],\n";
                }
                s << "    ]\n";
                s << "    glue." << glueMeth << "(" << listVar << ")\n\n";
            }
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Glue signatures
// ---------------------------------------------------------------------------

QVector<PythonGenerator::GlueSig> PythonGenerator::collectGlueSigs(const SpectableFile& file)
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
        // Context blocks belong to another .spectable and are tested there.
        // Emitting stubs for them here produced glue methods no test calls.
        if (!nb.hasExamples || nb.isContext) continue;
        const QString meth = "examples_" + nb.kind + "_" + toTypeName(nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr
            : findAttrSet(nb.examples.attrSetName, file);
        sigs.push_back({ meth, as ? (nb.examples.attrSetName + "String") : "grid" });
    }

    return sigs;
}

// ---------------------------------------------------------------------------
// Stub method generation
// ---------------------------------------------------------------------------

QString PythonGenerator::genStubMethod(const GlueSig& sig)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "    def " << sig.method << "(self):\n";
        s << "        raise NotImplementedError('" << sig.method << "')\n";
    } else if (sig.paramType == "docstring") {
        s << "    def " << sig.method << "(self, value: str):\n";
        s << "        print(value)\n";
        s << "        raise NotImplementedError('" << sig.method << "')\n";
    } else if (sig.paramType == "grid") {
        s << "    def " << sig.method << "(self, values: list):\n";
        s << "        for row in values:\n";
        s << "            print(row)\n";
        s << "        raise NotImplementedError('" << sig.method << "')\n";
    } else {
        const QString pt = toTypeName(sig.paramType);
        s << "    def " << sig.method << "(self, values: list):\n";
        s << "        for value in values:\n";
        s << "            print(value)\n";
        s << "        raise NotImplementedError('" << sig.method << "')\n";
    }
    return out;
}

// ---------------------------------------------------------------------------
// Glue file generator
// ---------------------------------------------------------------------------

QString PythonGenerator::genGlueFile(const SpectableFile& file, const QString& glueClass) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    QString out;
    QTextStream s(&out);

    s << "from common import *\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n\n";
    s << "class " << glueClass << ":\n";
    s << "    DNC_STRING = '?DNC?'\n\n";

    for (const GlueSig& sig : sigs)
        s << genStubMethod(sig) << "\n";

    return out;
}

// ---------------------------------------------------------------------------
// Append missing glue stubs
// ---------------------------------------------------------------------------

bool PythonGenerator::appendMissingStubs(const QString& gluePath,
                                          const QVector<GlueSig>& sigs,
                                          QStringList& msgs)
{
    QFile f(gluePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString content = QTextStream(&f).readAll();
    f.close();
    // A commented-out method has been removed as far as the compiler is
    // concerned, so search a copy with comments blanked out.
    const QString scan = sourcescan::stripHashComments(content);

    QString stubs;
    for (const GlueSig& sig : sigs) {
        const QString signature = QString("    def %1(self").arg(sig.method);
        if (!scan.contains(signature))
            stubs += "\n" + genStubMethod(sig);
    }
    if (stubs.isEmpty()) return false;

    // Append before end of file (Python has no closing brace)
    content += "\n" + stubs;

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

// ---------------------------------------------------------------------------
// Production class generators
// ---------------------------------------------------------------------------

QString PythonGenerator::genProductionEntity(const AttrSet& as)
{
    QString out;
    QTextStream s(&out);

    bool needsDecimal = false;
    for (const Field& f : as.fields)
        if (pythonType(f.type) == "decimal.Decimal") { needsDecimal = true; break; }
    if (needsDecimal) s << "import decimal\n\n";

    const QString cn = toTypeName(as.name);
    s << "class " << cn << ":\n";
    s << "    def __init__(self";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        const QString pt  = pythonType(f.type);
        const QString def = f.defaultValue.isEmpty()
            ? ((pt == "int") ? "0" : (pt == "float") ? "0.0"
               : (pt == "decimal.Decimal") ? "decimal.Decimal('0')"
               : (pt == "bool") ? "False" : "''")
            : f.defaultValue;
        s << ", " << fid << ": " << pt << " = " << def;
    }
    s << "):\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        s << "        self." << fid << " = " << fid << "\n";
    }
    if (as.fields.isEmpty())
        s << "        pass\n";
    return out;
}

QString PythonGenerator::genProductionCollection(const Collection& col)
{
    QString out;
    QTextStream s(&out);

    const QString cn   = toTypeName(col.name);
    const QString elem = toTypeName(col.elementType);
    const QString emod = toModuleName(col.elementType);

    s << "from " << emod << " import " << elem << "\n\n\n";
    s << "class " << cn << ":\n";
    if (!col.minimum.isEmpty())
        s << "    MINIMUM = " << col.minimum << "\n";
    if (!col.maximum.isEmpty())
        s << "    MAXIMUM = " << col.maximum << "\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty()) s << "\n";

    s << "    def __init__(self):\n";
    s << "        self._items: list[" << elem << "] = []\n\n";

    s << "    def add(self, item: " << elem << ") -> None:\n";
    s << "        self._items.append(item)\n\n";

    s << "    def delete(self, item: " << elem << ") -> bool:\n";
    s << "        try:\n";
    s << "            self._items.remove(item)\n";
    s << "            return True\n";
    s << "        except ValueError:\n";
    s << "            return False\n\n";

    s << "    def read(self) -> list[" << elem << "]:\n";
    s << "        return list(self._items)\n\n";

    s << "    def update(self, old_item: " << elem << ", new_item: " << elem << ") -> bool:\n";
    s << "        try:\n";
    s << "            idx = self._items.index(old_item)\n";
    s << "            self._items[idx] = new_item\n";
    s << "            return True\n";
    s << "        except ValueError:\n";
    s << "            return False\n\n";

    s << "    def size(self) -> int:\n";
    s << "        return len(self._items)\n";

    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool PythonGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList PythonGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraImports = opts.extraImports;
    m_tagFilter    = opts.tagFilter;

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString specSnake  = toIdentifier(file.specName);
    const QString glueClass  = toTypeName(file.specName) + "Glue";

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

    // Write common String/Typed classes
    QVector<AttrSet> domainSets;
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        const QString mod = toModuleName(as.name);
        writeFile(commonDir.filePath(mod + "_string.py"), genStringClass(as), msgs);
        writeFile(commonDir.filePath(mod + "_typed.py"),  genTypedClass(as),  msgs);
        domainSets.push_back(as);
    }
    writeFile(commonDir.filePath("json_util.py"), genJsonUtil(),               msgs);
    {
        const QString initPath = commonDir.filePath("__init__.py");
        QString existing;
        QFile ef(initPath);
        if (ef.open(QIODevice::ReadOnly | QIODevice::Text))
            existing = QTextStream(&ef).readAll();
        writeFile(initPath, genCommonInit(domainSets, existing), msgs);
    }

    // Test file (always overwritten)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specSnake, glueClass, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(dir.filePath("Test_" + toTypeName(file.specName) + ".py"), testContent, msgs);
    }

    // Glue file (write fresh if absent; append missing stubs otherwise)
    {
        const QString gluePath = dir.filePath(specSnake + "_glue.py");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, glueClass), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // Production classes
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists() && !prodDir.mkpath(".")) {
            msgs << QString("ERROR:0:Cannot create production directory: %1").arg(prodDir.path());
        } else {
            // Entity production classes
            for (const AttrSet& as : file.attrSets) {
                if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
                const QString prodPath = prodDir.filePath(toTypeName(as.name) + ".py");
                if (QFile::exists(prodPath)) continue;
                writeFile(prodPath, genProductionEntity(as), msgs);
            }
            // Collection production classes
            for (const Collection& col : file.collections) {
                if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
                const QString prodPath = prodDir.filePath(toTypeName(col.name) + ".py");
                if (QFile::exists(prodPath)) continue;
                writeFile(prodPath, genProductionCollection(col), msgs);
            }
        }
    }

    return msgs;
}
