#include "SwiftGenerator.h"
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

static QString swiftEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('"',  "\\\"");
    return r;
}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

QString SwiftGenerator::swiftType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")                          return "Int";
    if (t == "float"   || t == "decimal" || t == "scientific") return "Double";
    if (t == "boolean" || t == "yesno" || t == "bool")         return "Bool";
    if (t == "string" || t == "text"
     || t == "character" || t == "char")                       return "String";
    // date/time/duration — returned as String without external dependencies
    if (t == "date" || t == "time" || t == "datetime"
     || t == "duration")                                       return "String";
    return specType.trimmed();
}

QString SwiftGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("Int(s.%1) ?? 0").arg(field);
    if (t == "float" || t == "decimal" || t == "scientific")
        return QString("Double(s.%1) ?? 0.0").arg(field);
    if (t == "boolean" || t == "yesno" || t == "bool")
        return QString(
            "[\"true\", \"t\", \"yes\", \"y\", \"1\"].contains(s.%1.lowercased())").arg(field);
    // String-backed built-ins (date/time/datetime/duration/string/text/char)
    if (t == "string" || t == "text" || t == "character" || t == "char"
     || t == "date"   || t == "time" || t == "datetime"  || t == "duration")
        return QString("s.%1").arg(field);
    // User-defined type — assumes a String-based init(_:), same intent as Rust's Type::from(...)
    return QString("%1(s.%2)").arg(specType.trimmed()).arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString SwiftGenerator::toIdentifier(const QString& name)
{
    const QStringList parts = name.trimmed().split(QRegularExpression(R"([\s_]+)"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return "value";
    QString result = parts[0][0].toLower() + parts[0].mid(1);
    for (int i = 1; i < parts.size(); ++i)
        result += parts[i][0].toUpper() + parts[i].mid(1);
    if (!result.isEmpty() && result[0].isDigit()) result.prepend('_');
    return result;
}

QString SwiftGenerator::toTypeName(const QString& name)
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

QString SwiftGenerator::toFnName(const QString& keyword, const QString& stepText)
{
    QString combined = (keyword + " " + stepText).toLower();
    combined.replace(QRegularExpression(R"([^a-z0-9]+)"), " ");
    const QStringList parts = combined.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return "step";
    QString result = parts[0];
    for (int i = 1; i < parts.size(); ++i)
        result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

static QString kindToTitle(const QString& kind)
{
    if (kind == "BusinessRule") return "BusinessRule";
    if (kind == "Calculation")  return "Calculation";
    if (kind == "DataType")     return "DataType";
    return SwiftGenerator::toTypeName(kind);
}

// ---------------------------------------------------------------------------
// DataType detection
// ---------------------------------------------------------------------------

bool SwiftGenerator::isDataType(const QString& name, const SpectableFile& file)
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

const AttrSet* SwiftGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return &as;
    return nullptr;
}

const Define* SwiftGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Row resolution (same logic as RustGenerator/JavaGenerator)
// ---------------------------------------------------------------------------

QVector<QStringList> SwiftGenerator::resolveStepRows(
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

QVector<QStringList> SwiftGenerator::resolveExamplesRows(
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

QString SwiftGenerator::genStringStruct(const AttrSet& as) const
{
    const QString typeName = toTypeName(as.name) + "String";
    QString out;
    QTextStream s(&out);

    for (const QString& imp : m_extraImports) s << imp << "\n";
    if (!m_extraImports.isEmpty()) s << "\n";

    s << "public struct " << typeName << ": CustomStringConvertible, Equatable {\n";
    for (const Field& f : as.fields)
        s << "    public let " << toIdentifier(f.name) << ": String\n";
    s << "\n";

    s << "    public init(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString fid = toIdentifier(as.fields[i].name);
        s << fid << ": String";
    }
    s << ") {\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        s << "        self." << fid << " = " << fid << "\n";
    }
    s << "    }\n\n";

    s << "    public init(fromArray v: [String]) {\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const QString fid = toIdentifier(as.fields[i].name);
        s << "        self." << fid << " = v.count > " << i << " ? v[" << i << "] : \"\"\n";
    }
    s << "    }\n\n";

    s << "    public var description: String {\n";
    s << "        return \"";
    QStringList parts;
    for (const Field& f : as.fields)
        parts << (f.name + "=\\(" + toIdentifier(f.name) + ")");
    s << parts.join(", ") << "\"\n";
    s << "    }\n";
    s << "}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed struct
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// common/Json.swift — field accessors over Foundation's JSONSerialization.
// No third-party package required.
// ---------------------------------------------------------------------------

static QString genSwiftJsonFile(const QStringList& extraImports)
{
    QString out;
    QTextStream s(&out);
    s << "import Foundation\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    s << "\n";
    s << QString::fromLatin1(R"SW(/// Raised for malformed JSON, a missing field, or a value of the wrong type.
public enum JsonError: Error, CustomStringConvertible {
    case invalid(String)

    public var description: String {
        switch self {
        case .invalid(let message): return message
        }
    }
}

/// Field accessors over JSONSerialization.
///
/// A missing key or a value of the wrong type throws JsonError. An explicit
/// JSON null is passed through as an empty/zero value rather than an error.
public enum Json {

    // -----------------------------------------------------------------
    // Reading
    // -----------------------------------------------------------------

    public static func parseObject(_ text: String) throws -> [String: Any] {
        let value = try parseAny(text)
        guard let object = value as? [String: Any] else {
            throw JsonError.invalid("Expected a JSON object, got \(describe(value))")
        }
        return object
    }

    public static func parseArray(_ text: String) throws -> [Any] {
        let value = try parseAny(text)
        guard let array = value as? [Any] else {
            throw JsonError.invalid("Expected a JSON array, got \(describe(value))")
        }
        return array
    }

    private static func parseAny(_ text: String) throws -> Any {
        guard let data = text.data(using: .utf8) else {
            throw JsonError.invalid("JSON text is not valid UTF-8")
        }
        do {
            return try JSONSerialization.jsonObject(with: data, options: [])
        } catch {
            throw JsonError.invalid("Invalid JSON: \(error.localizedDescription)")
        }
    }

    // -----------------------------------------------------------------
    // Writing
    // -----------------------------------------------------------------

    public static func write(_ value: Any) throws -> String {
        do {
            // sortedKeys keeps the output stable: Swift dictionaries are unordered.
            let data = try JSONSerialization.data(withJSONObject: value,
                                                  options: [.sortedKeys])
            guard let text = String(data: data, encoding: .utf8) else {
                throw JsonError.invalid("Could not encode JSON as UTF-8")
            }
            return text
        } catch let error as JsonError {
            throw error
        } catch {
            throw JsonError.invalid("Could not serialize to JSON: \(error.localizedDescription)")
        }
    }

    // -----------------------------------------------------------------
    // Field accessors
    // -----------------------------------------------------------------

    /// True when the value came from JSON `true`/`false`.
    ///
    /// CFGetTypeID/CFBooleanGetTypeID are CoreFoundation and exist only on
    /// Apple platforms; swift-corelibs-foundation on Windows and Linux hands
    /// back a Swift Bool instead, so each side gets the check it supports.
    private static func isBoolean(_ value: Any) -> Bool {
        #if canImport(Darwin)
        if let number = value as? NSNumber {
            return CFGetTypeID(number) == CFBooleanGetTypeID()
        }
        return false
        #else
        return value is Bool
        #endif
    }

    private static func describe(_ value: Any?) -> String {
        guard let value = value else { return "null" }
        if value is NSNull        { return "null" }
        if value is String        { return "a string" }
        if value is [Any]         { return "an array" }
        if value is [String: Any] { return "an object" }
        if isBoolean(value)       { return "a boolean" }
        if value is NSNumber      { return "a number" }
        if value is Bool          { return "a boolean" }
        return "a number"
    }

    private static func typeError(_ ctx: String, _ expected: String, _ actual: Any?) -> JsonError {
        return JsonError.invalid(
            "JSON field '\(ctx)' is not \(expected) (got \(describe(actual)))")
    }

    public static func require(_ object: [String: Any], _ key: String) throws -> Any {
        guard let value = object[key] else {
            throw JsonError.invalid("Missing JSON field '\(key)'")
        }
        return value
    }

    public static func asString(_ value: Any?, _ ctx: String) throws -> String {
        if value == nil || value is NSNull { return "" }
        if let text = value as? String     { return text }
        if let flag = value as? Bool       { return flag ? "true" : "false" }
        if let number = value as? NSNumber { return number.stringValue }
        throw typeError(ctx, "a string", value)
    }

    public static func asDouble(_ value: Any?, _ ctx: String) throws -> Double {
        if let number = value as? NSNumber { return number.doubleValue }
        if let text = value as? String, let parsed = Double(text.trimmingCharacters(in: .whitespaces)) {
            return parsed
        }
        throw typeError(ctx, "a number", value)
    }

    /// Accepts 7 and 7.0 for an integer field, but not 7.5.
    public static func asInt(_ value: Any?, _ ctx: String) throws -> Int {
        if let number = value as? NSNumber {
            let d = number.doubleValue
            guard d == d.rounded(), d >= Double(Int.min), d <= Double(Int.max) else {
                throw typeError(ctx, "an integer", value)
            }
            return Int(d)
        }
        if let text = value as? String, let parsed = Int(text.trimmingCharacters(in: .whitespaces)) {
            return parsed
        }
        throw typeError(ctx, "an integer", value)
    }

    public static func asBool(_ value: Any?, _ ctx: String) throws -> Bool {
        if let value = value, isBoolean(value) {
            if let number = value as? NSNumber { return number.boolValue }
            if let flag = value as? Bool       { return flag }
        }
        if let flag = value as? Bool { return flag }
        if let text = value as? String {
            switch text.trimmingCharacters(in: .whitespaces).lowercased() {
            case "true", "t", "yes", "y", "1":  return true
            case "false", "f", "no", "n", "0":  return false
            default: break
            }
        }
        throw typeError(ctx, "a boolean", value)
    }

    public static func asObject(_ value: Any?, _ ctx: String) throws -> [String: Any] {
        guard let object = value as? [String: Any] else {
            throw typeError(ctx, "an object", value)
        }
        return object
    }
}
)SW");
    return out;
}

QString SwiftGenerator::genTypedStruct(const AttrSet& as) const
{
    const QString strTypeName = toTypeName(as.name) + "String";
    const QString typedName   = toTypeName(as.name) + "Typed";
    QString out;
    QTextStream s(&out);

    for (const QString& imp : m_extraImports) s << imp << "\n";
    if (!m_extraImports.isEmpty()) s << "\n";

    s << "public struct " << typedName << " {\n";
    for (const Field& f : as.fields) {
        const QString st = swiftType(f.type);
        s << "    public let " << toIdentifier(f.name) << ": " << st << "\n";
    }
    s << "\n";

    s << "    public init(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        s << toIdentifier(f.name) << ": " << swiftType(f.type);
    }
    s << ") {\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        s << "        self." << fid << " = " << fid << "\n";
    }
    s << "    }\n\n";

    s << "    public init(from s: " << strTypeName << ") {\n";
    for (const Field& f : as.fields) {
        const QString fid  = toIdentifier(f.name);
        const QString expr = parseExpr(fid, f.type);
        s << "        self." << fid << " = " << expr << "\n";
    }
    s << "    }\n\n";

    // ---- JSON (Foundation's JSONSerialization; see common/Json.swift) ----

    s << "    public func toJSONValue() -> [String: Any] {\n";
    s << "        return [\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        const QString st  = swiftType(f.type);
        if (st == "Int" || st == "Double" || st == "Bool" || st == "String")
            s << "            \"" << fid << "\": " << fid << ",\n";
        else    // user-defined type — same text convention init(from:) assumes
            s << "            \"" << fid << "\": String(describing: " << fid << "),\n";
    }
    s << "        ]\n";
    s << "    }\n\n";

    s << "    public func toJSON() throws -> String {\n";
    s << "        return try Json.write(toJSONValue())\n";
    s << "    }\n\n";

    s << "    public init(fromJSONValue m: [String: Any]) throws {\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        const QString st  = swiftType(f.type);
        const QString fn  = (st == "Int")    ? "asInt"
                          : (st == "Double") ? "asDouble"
                          : (st == "Bool")   ? "asBool"
                                             : "asString";
        const QString src = QString("try Json.%1(Json.require(m, \"%2\"), \"%2\")").arg(fn, fid);
        if (st == "Int" || st == "Double" || st == "Bool" || st == "String")
            s << "        self." << fid << " = " << src << "\n";
        else
            s << "        self." << fid << " = " << f.type.trimmed() << "(" << src << ")\n";
    }
    s << "    }\n\n";

    s << "    public init(fromJSON text: String) throws {\n";
    s << "        try self.init(fromJSONValue: Json.parseObject(text))\n";
    s << "    }\n\n";

    s << "    public static func toJSONList(_ list: [" << typedName << "]) throws -> String {\n";
    s << "        return try Json.write(list.map { $0.toJSONValue() })\n";
    s << "    }\n\n";

    s << "    public static func fromJSONList(_ text: String) throws -> [" << typedName << "] {\n";
    s << "        return try Json.parseArray(text).map {\n";
    s << "            try " << typedName << "(fromJSONValue: Json.asObject($0, \"" << typedName << "\"))\n";
    s << "        }\n";
    s << "    }\n";
    s << "}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Test file
// ---------------------------------------------------------------------------

QString SwiftGenerator::genTestFile(const SpectableFile& file, const QString& className,
                                     const QString& glueClass, QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    s << "import XCTest\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";

    // Helper: emit an array literal of AttrSetString rows inline
    auto emitStrArray = [&](const QString& listType, const QVector<QStringList>& rows) {
        s << "[\n";
        for (const QStringList& row : rows) {
            s << "            " << listType << "(fromArray: [";
            for (int ci = 0; ci < row.size(); ++ci) {
                if (ci) s << ", ";
                s << "\"" << swiftEscape(row[ci]) << "\"";
            }
            s << "]),\n";
        }
        s << "        ]";
    };

    // Helper: emit an array literal of [String] rows inline
    auto emitGridArray = [&](const QVector<QStringList>& rows, int startRow = 0) {
        s << "[\n";
        for (int ri = startRow; ri < rows.size(); ++ri) {
            s << "            [";
            const QStringList& r = rows[ri];
            for (int ci = 0; ci < r.size(); ++ci) {
                if (ci) s << ", ";
                s << "\"" << swiftEscape(resolveValue(r[ci], file)) << "\"";
            }
            s << "],\n";
        }
        s << "        ]";
    };

    auto emitSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            if (step.hasDocString) {
                const QString meth = toFnName(step.keyword, step.text);
                QString esc = step.docString;
                esc.replace("\\", "\\\\");
                esc.replace("\"", "\\\"");
                esc.replace("\n", "\\n");
                s << "        glue." << meth << "(\"" << esc << "\")\n";
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
                    s << "        glue." << meth << "(\"" << esc << "\")\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                const QString meth = toFnName(step.keyword, step.text);
                s << "        glue." << meth << "()\n";
                continue;
            }

            const AttrSet* as = findAttrSet(step.attrSetName, file);

            if (!step.attrSetName.isEmpty() && !as) {
                if (!isDataType(step.attrSetName, file)) {
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
                const QString listType = toTypeName(step.attrSetName) + "String";
                s << "        glue." << meth << "(";
                emitStrArray(listType, rows);
                s << ")\n";
            } else {
                const StepTable& tbl = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;
                s << "        glue." << meth << "(";
                emitGridArray(tbl.rows, startRow);
                s << ")\n";
            }
        }
    };

    s << "final class " << className << "Tests: XCTestCase {\n\n";

    // ── Scenario tests ──────────────────────────────────────────────────────
    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;
        const QString fn = "test" + toTypeName(sc.name);

        const QStringList allTags = file.tags + sc.tags;
        if (!allTags.isEmpty())
            s << "    // Tags: " << allTags.join(", ") << "\n";
        s << "    func " << fn << "() {\n";
        s << "        let glue = " << glueClass << "()\n";
        emitSteps(file.backgroundSteps);
        emitSteps(sc.steps);
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

            const QString fn     = "test" + kindToTitle(kind) + toTypeName(nb.name);
            const QString glueFn = "examples" + kindToTitle(kind) + toTypeName(nb.name);
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            if (!nb.tags.isEmpty())
                s << "    // Tags: " << nb.tags.join(", ") << "\n";
            s << "    func " << fn << "() {\n";
            s << "        let glue = " << glueClass << "()\n";

            if (as) {
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                const QString listType = toTypeName(nb.examples.attrSetName) + "String";
                s << "        glue." << glueFn << "(";
                emitStrArray(listType, rows);
                s << ")\n";
            } else {
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "        glue." << glueFn << "(";
                emitGridArray(rows);
                s << ")\n";
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

QVector<SwiftGenerator::GlueSig> SwiftGenerator::collectGlueSigs(const SpectableFile& file)
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
                sigs.push_back({ meth, step.attrSetName + "String" });
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
        if (!nb.hasExamples) continue;
        const QString meth = "examples" + kindToTitle(nb.kind) + toTypeName(nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr
            : findAttrSet(nb.examples.attrSetName, file);
        sigs.push_back({ meth, as ? (nb.examples.attrSetName + "String") : "grid" });
    }

    return sigs;
}

QString SwiftGenerator::genStubFn(const GlueSig& sig)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "    public func " << sig.method << "() {\n";
        s << "        XCTFail(\"Not implemented: " << sig.method << "\")\n";
        s << "    }\n";
    } else if (sig.paramType == "docstring") {
        s << "    public func " << sig.method << "(_ value: String) {\n";
        s << "        print(value)\n";
        s << "        XCTFail(\"Not implemented: " << sig.method << "\")\n";
        s << "    }\n";
    } else if (sig.paramType == "grid") {
        s << "    public func " << sig.method << "(_ values: [[String]]) {\n";
        s << "        for value in values { print(value) }\n";
        s << "        XCTFail(\"Not implemented: " << sig.method << "\")\n";
        s << "    }\n";
    } else {
        const QString pt = toTypeName(sig.paramType);
        s << "    public func " << sig.method << "(_ values: [" << pt << "]) {\n";
        s << "        for value in values { print(value) }\n";
        s << "        XCTFail(\"Not implemented: " << sig.method << "\")\n";
        s << "    }\n";
    }
    return out;
}

bool SwiftGenerator::appendMissingStubs(const QString& gluePath,
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
        if (!scan.contains(QStringLiteral("func %1(").arg(sig.method)))
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

QString SwiftGenerator::genGlueFile(const SpectableFile& file, const QString& glueClass) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    QString out;
    QTextStream s(&out);

    s << "import XCTest\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";
    s << "public class " << glueClass << " {\n";
    s << "    public init() {}\n";
    for (const GlueSig& sig : sigs)
        s << "\n" << genStubFn(sig);
    s << "}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Production class generators
// ---------------------------------------------------------------------------

// DataType ValidValues → struct with isValid property
static QString genSwiftProductionClass(const NamedBlock& nb)
{
    const QString name = SwiftGenerator::toTypeName(nb.name);
    QString out;
    QTextStream s(&out);
    s << "public struct " << name << ": Equatable, CustomStringConvertible {\n";
    s << "    public let value: String\n\n";
    s << "    public init(_ value: String) {\n";
    s << "        self.value = value\n";
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
            vals << "\"" + row[valueCol].trimmed().toLower() + "\"";
        }
        if (!vals.isEmpty()) {
            s << "    public var isValid: Bool {\n";
            s << "        return [" << vals.join(", ") << "].contains(value.lowercased())\n";
            s << "    }\n\n";
        }
    }

    s << "    public var description: String { return value }\n";
    s << "}\n";
    return out;
}

// EnumerationValues DataType → enum
static QString genSwiftProductionEnum(const NamedBlock& nb)
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

    const QString name = SwiftGenerator::toTypeName(nb.name);
    QString out;
    QTextStream s(&out);
    s << "public enum " << name << ": String, CaseIterable {\n";
    for (const QString& v : variants)
        s << "    case " << SwiftGenerator::toIdentifier(v) << " = \"" << v << "\"\n";
    s << "}\n";
    return out;
}

// Entity → production struct with memberwise init
static QString genSwiftProductionEntity(const AttrSet& as, const SpectableFile& file)
{
    (void)file;  // reserved for cross-entity type lookup
    const QString name = SwiftGenerator::toTypeName(as.name);
    QString out;
    QTextStream s(&out);

    s << "public struct " << name << " {\n";
    for (const Field& f : as.fields) {
        const QString fid = SwiftGenerator::toIdentifier(f.name);
        const QString st  = SwiftGenerator::swiftType(f.type);
        s << "    public let " << fid << ": " << st << "\n";
    }
    s << "\n";
    s << "    public init(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString fid = SwiftGenerator::toIdentifier(as.fields[i].name);
        const QString st  = SwiftGenerator::swiftType(as.fields[i].type);
        s << fid << ": " << st;
    }
    s << ") {\n";
    for (const Field& f : as.fields) {
        const QString fid = SwiftGenerator::toIdentifier(f.name);
        s << "        self." << fid << " = " << fid << "\n";
    }
    s << "    }\n";
    s << "}\n";
    return out;
}

// Collection → production class with add/delete/read/update/size
static QString genSwiftProductionCollection(const Collection& col)
{
    const QString name     = SwiftGenerator::toTypeName(col.name);
    const QString elemType = SwiftGenerator::toTypeName(col.elementType);
    QString out;
    QTextStream s(&out);
    if (!col.minimum.isEmpty())
        s << "public let " << SwiftGenerator::toIdentifier(col.name) << "Minimum = " << col.minimum << "\n";
    if (!col.maximum.isEmpty())
        s << "public let " << SwiftGenerator::toIdentifier(col.name) << "Maximum = " << col.maximum << "\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty()) s << "\n";

    s << "public class " << name << " {\n";
    s << "    private var items: [" << elemType << "] = []\n\n";
    s << "    public init() {}\n\n";
    s << "    public func add(_ item: " << elemType << ") {\n";
    s << "        items.append(item)\n    }\n\n";
    s << "    public func delete(_ item: " << elemType << ") -> Bool where " << elemType << ": Equatable {\n";
    s << "        if let idx = items.firstIndex(where: { $0 == item }) {\n";
    s << "            items.remove(at: idx)\n";
    s << "            return true\n        }\n        return false\n    }\n\n";
    s << "    public func read() -> [" << elemType << "] {\n";
    s << "        return items\n    }\n\n";
    s << "    public func update(_ oldItem: " << elemType << ", with newItem: " << elemType
      << ") -> Bool where " << elemType << ": Equatable {\n";
    s << "        if let idx = items.firstIndex(where: { $0 == oldItem }) {\n";
    s << "            items[idx] = newItem\n";
    s << "            return true\n        }\n        return false\n    }\n\n";
    s << "    public var size: Int { return items.count }\n";
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool SwiftGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList SwiftGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraImports = opts.extraImports;
    m_tagFilter    = opts.tagFilter;

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString className = toTypeName(file.specName);
    const QString glueClass = className + "Glue";

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

    // Synthesize implicit AttrSets for NamedBlocks (same as JavaGenerator/RustGenerator)
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

    // Write common structs. Swift files in the same target see each other
    // without per-file imports, so — unlike Rust's mod.rs — no index file
    // or "use" statement is needed to wire them together.
    writeFile(commonDir.filePath("Json.swift"), genSwiftJsonFile(m_extraImports), msgs);

    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        const QString tn = toTypeName(as.name);
        writeFile(commonDir.filePath(tn + "String.swift"), genStringStruct(as), msgs);
        writeFile(commonDir.filePath(tn + "Typed.swift"),  genTypedStruct(as),  msgs);
    }

    // Test file
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, className, glueClass, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(dir.filePath(className + "Tests.swift"), testContent, msgs);
    }

    // Glue file
    {
        const QString gluePath = dir.filePath(className + "Glue.swift");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, glueClass), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // Production classes (DataType → struct/enum, Entity → struct, Collection → class)
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists()) prodDir.mkpath(".");

        // DataType ValidValues → struct with isValid; EnumerationValues → enum
        for (const NamedBlock& nb : file.namedBlocks) {
            if (nb.isContext || !nb.hasExamples || nb.kind != "DataType") continue;
            const bool isValidValues =
                nb.examples.attrSetName.compare("ValidValues", Qt::CaseInsensitive) == 0;
            const bool isEnum =
                nb.examples.attrSetName.compare("EnumerationValues", Qt::CaseInsensitive) == 0;
            if (!isValidValues && !isEnum) continue;
            const QString prodPath = prodDir.filePath(toTypeName(nb.name) + ".swift");
            if (QFile::exists(prodPath)) continue;
            writeFile(prodPath, isValidValues ? genSwiftProductionClass(nb)
                                              : genSwiftProductionEnum(nb), msgs);
        }

        // Entity → struct with memberwise init
        for (const AttrSet& as : file.attrSets) {
            if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
            const QString prodPath = prodDir.filePath(toTypeName(as.name) + ".swift");
            if (QFile::exists(prodPath)) continue;
            writeFile(prodPath, genSwiftProductionEntity(as, file), msgs);
        }

        // Collection → class with add/delete/read/update/size
        for (const Collection& col : file.collections) {
            if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
            const QString prodPath = prodDir.filePath(toTypeName(col.name) + ".swift");
            if (QFile::exists(prodPath)) continue;
            writeFile(prodPath, genSwiftProductionCollection(col), msgs);
        }
    }

    return msgs;
}
