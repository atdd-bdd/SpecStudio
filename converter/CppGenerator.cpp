#include "CppGenerator.h"
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

static QString cppEscape(const QString& s)
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

QString CppGenerator::cppType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")                                    return "int";
    if (t == "float"   || t == "decimal" || t == "scientific")           return "double";
    if (t == "boolean" || t == "yesno" || t == "bool")                   return "bool";
    if (t == "string"  || t == "text"  || t == "character" || t == "char") return "std::string";
    if (t == "date"    || t == "time"  || t == "datetime"  || t == "duration") return "std::string";
    return specType.trimmed();
}

bool CppGenerator::isAttrSetType(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

// The type a field takes inside the common headers. A nested Attributes block
// becomes that block's own Typed struct. A user DataType lives in the production
// folder, which common must not depend on, so its value is carried as text.
QString CppGenerator::cppCommonType(const Field& f, const SpectableFile& file)
{
    if (isAttrSetType(f.type, file)) return toTypeName(f.type) + "Typed";

    static const QSet<QString> builtin = {
        "integer", "int", "float", "decimal", "scientific", "boolean", "yesno",
        "bool", "string", "text", "character", "char", "date", "time",
        "datetime", "duration"
    };
    if (!builtin.contains(f.type.trimmed().toLower())) return "std::string";
    return cppType(f.type);
}

// A cell for a nested-object field is written "=SomeDefine"; expand that define
// against the nested Attributes block and build its String struct literal.
QString CppGenerator::nestedLiteral(const QString& cellValue, const QString& fieldType,
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
    return "std::string(\"" + cppEscape(cellValue) + "\")";
}

// A braced initializer for one row, used when the block has a nested-object field.
QString CppGenerator::stringLiteral(const AttrSet& as, const QStringList& row,
                                    const SpectableFile& file)
{
    QString expr = toTypeName(as.name) + "String{";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) expr += ", ";
        const QString cell = (i < row.size()) ? row[i] : QString();
        if (isAttrSetType(as.fields[i].type, file))
            expr += nestedLiteral(cell, as.fields[i].type, file);
        else
            expr += "\"" + cppEscape(cell) + "\"";
    }
    return expr + "}";
}

QString CppGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("!s.%1.empty() ? std::stoi(s.%1) : 0").arg(field);
    if (t == "float" || t == "decimal" || t == "scientific")
        return QString("!s.%1.empty() ? std::stod(s.%1) : 0.0").arg(field);
    if (t == "boolean" || t == "yesno" || t == "bool")
        // A spec writes Yes/No/True/False in any casing, so the comparison has
        // to be case-insensitive; parse_bool_cell lives in the String header.
        return QString("parse_bool_cell(s.%1)").arg(field);
    if (t == "string" || t == "text" || t == "character" || t == "char"
     || t == "date"   || t == "time" || t == "datetime"  || t == "duration")
        return QString("s.%1").arg(field);
    // User-defined
    return QString("%1(s.%2)").arg(specType.trimmed()).arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString CppGenerator::toIdentifier(const QString& name)
{
    QString s = name.trimmed();
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    if (!s.isEmpty() && s[0].isDigit()) s.prepend('_');
    return s.toLower();
}

QString CppGenerator::toTypeName(const QString& name)
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

QString CppGenerator::toFnName(const QString& keyword, const QString& stepText)
{
    QString s = keyword + "_" + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    return s.toLower();
}

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

bool CppGenerator::isDataType(const QString& name, const SpectableFile& file)
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

bool CppGenerator::isCollectionType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

QString CppGenerator::collectionElementType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return c.elementType;
    return {};
}

const AttrSet* CppGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return &as;
    return nullptr;
}

const Define* CppGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Row resolution
// ---------------------------------------------------------------------------

QVector<QStringList> CppGenerator::resolveStepRows(
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

QVector<QStringList> CppGenerator::resolveExamplesRows(
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
// String header generator
// ---------------------------------------------------------------------------

QString CppGenerator::genStringHeader(const AttrSet& as, const SpectableFile& file) const
{
    const QString typeName = toTypeName(as.name) + "String";
    QString out;
    QTextStream s(&out);

    s << "#pragma once\n";
    s << "#include <cctype>\n";
    s << "#include <string>\n";
    s << "#include <vector>\n";
    s << "#include <sstream>\n";
    // A nested Attributes block is held as that block's own String struct.
    QSet<QString> seenIncludes;
    for (const Field& f : as.fields) {
        if (!isAttrSetType(f.type, file)) continue;
        if (seenIncludes.contains(f.type.trimmed().toLower())) continue;
        seenIncludes.insert(f.type.trimmed().toLower());
        s << "#include \"" << toIdentifier(f.type) << "_string.h\"\n";
    }
    for (const QString& inc : m_extraIncludes) s << inc << "\n";
    s << "\n";

    // The Do-Not-Care marker a CompareOnly step puts in every column it does not
    // name. Guarded because every String header defines it.
    s << "#ifndef SPECTABLE_DNC_STRING\n";
    s << "#define SPECTABLE_DNC_STRING\n";
    s << "inline const std::string DNCString = \"?DNC?\";\n";
    s << "inline bool dnc_equal(const std::string& a, const std::string& b) {\n";
    s << "    return a == b || a == DNCString || b == DNCString;\n";
    s << "}\n";
    s << "// Reads the Yes/No/True/False text a spec cell may hold, in any casing.\n";
    s << "inline bool parse_bool_cell(const std::string& v) {\n";
    s << "    std::string t;\n";
    s << "    for (char c : v) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));\n";
    s << "    return t == \"true\" || t == \"t\" || t == \"yes\" || t == \"y\" || t == \"1\";\n";
    s << "}\n";
    s << "#endif\n\n";

    auto fieldType = [&](const Field& f) {
        return isAttrSetType(f.type, file) ? toTypeName(f.type) + "String"
                                          : QString("std::string");
    };

    s << "struct " << typeName << " {\n";
    for (const Field& f : as.fields)
        s << "    " << fieldType(f) << " " << toIdentifier(f.name) << ";\n";
    s << "\n";

    // from_vec factory
    s << "    static " << typeName << " from_vec(const std::vector<std::string>& v) {\n";
    s << "        " << typeName << " obj;\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        // A nested block cannot come from a flat vector; those rows are built
        // with a braced initializer instead.
        if (isAttrSetType(as.fields[i].type, file)) continue;
        s << "        if (v.size() > " << i << ") obj." << toIdentifier(as.fields[i].name)
          << " = v[" << i << "];\n";
    }
    s << "        return obj;\n";
    s << "    }\n\n";

    // to_string
    s << "    std::string to_string() const {\n";
    s << "        std::ostringstream ss;\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << "        ss << \", \";\n";
        const QString fid = toIdentifier(as.fields[i].name);
        s << "        ss << \"" << as.fields[i].name << "=\" << ";
        if (isAttrSetType(as.fields[i].type, file))
            s << fid << ".to_string();\n";
        else
            s << fid << ";\n";
    }
    s << "        return ss.str();\n";
    s << "    }\n\n";

    // A field holding the marker on either side matches whatever the other side
    // holds; a nested block delegates to its own comparison.
    s << "    bool operator==(const " << typeName << "& o) const {\n";
    if (as.fields.isEmpty()) {
        s << "        return true;\n";
    } else {
        s << "        return ";
        for (int i = 0; i < as.fields.size(); ++i) {
            if (i) s << "\n            && ";
            const QString fid = toIdentifier(as.fields[i].name);
            if (isAttrSetType(as.fields[i].type, file))
                s << fid << " == o." << fid;
            else
                s << "dnc_equal(" << fid << ", o." << fid << ")";
        }
        s << ";\n";
    }
    s << "    }\n";
    s << "    bool operator!=(const " << typeName << "& o) const { return !(*this == o); }\n";
    s << "};\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed header generator
// ---------------------------------------------------------------------------

QString CppGenerator::genTypedHeader(const AttrSet& as, const SpectableFile& file) const
{
    const QString strName   = toTypeName(as.name) + "String";
    const QString typedName = toTypeName(as.name) + "Typed";
    const QString strFile   = toIdentifier(as.name) + "_string.h";
    QString out;
    QTextStream s(&out);

    s << "#pragma once\n";
    s << "#include <string>\n";
    s << "#include <vector>\n";
    s << "#include \"json.h\"\n";
    s << "#include \"" << strFile << "\"\n";
    // A nested Attributes block is held as that block's own Typed struct.
    QSet<QString> seenTypedIncludes;
    for (const Field& f : as.fields) {
        if (!isAttrSetType(f.type, file)) continue;
        if (seenTypedIncludes.contains(f.type.trimmed().toLower())) continue;
        seenTypedIncludes.insert(f.type.trimmed().toLower());
        s << "#include \"" << toIdentifier(f.type) << "_typed.h\"\n";
    }
    for (const QString& inc : m_extraIncludes) s << inc << "\n";
    s << "\n";

    s << "struct " << typedName << " {\n";
    for (const Field& f : as.fields) {
        const QString ct = cppCommonType(f, file);
        s << "    " << ct << " " << toIdentifier(f.name);
        // default value
        const QString tl = f.type.trimmed().toLower();
        if (tl == "integer" || tl == "int")
            s << " = 0";
        else if (tl == "float" || tl == "decimal" || tl == "scientific")
            s << " = 0.0";
        else if (tl == "boolean" || tl == "yesno" || tl == "bool")
            s << " = false";
        s << ";\n";
    }
    s << "\n";

    // from_string_struct
    s << "    static " << typedName << " from_string_struct(const " << strName << "& s) {\n";
    s << "        " << typedName << " t;\n";
    for (const Field& f : as.fields) {
        const QString fid  = toIdentifier(f.name);
        const QString ct   = cppCommonType(f, file);
        // A nested Attributes block builds its own Typed struct; a user DataType
        // is carried as text, so only the built-ins go through parseExpr.
        const QString expr = isAttrSetType(f.type, file)
            ? QString("%1::from_string_struct(s.%2)").arg(ct, fid)
            : (ct == "std::string" ? QString("s.%1").arg(fid) : parseExpr(fid, f.type));
        s << "        t." << fid << " = " << expr << ";\n";
    }
    s << "        return t;\n";
    s << "    }\n\n";

    // ---- JSON (see json.h — no third-party library) ----

    s << "    json::Value to_json_value() const {\n";
    s << "        json::Members m;\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        if (isAttrSetType(f.type, file)) {
            // Nested Attributes block — written as a nested object.
            s << "        m.emplace_back(\"" << fid << "\", " << fid << ".to_json_value());\n";
            continue;
        }
        s << "        m.emplace_back(\"" << fid << "\", json::Convert<" << cppCommonType(f, file)
          << ">::to_json(" << fid << "));\n";
    }
    s << "        return json::Value::make_object(std::move(m));\n";
    s << "    }\n\n";

    s << "    std::string to_json() const { return json::write(to_json_value()); }\n\n";

    s << "    static " << typedName << " from_json_value(const json::Value& v) {\n";
    s << "        " << typedName << " t;\n";
    for (const Field& f : as.fields) {
        const QString fid = toIdentifier(f.name);
        if (isAttrSetType(f.type, file)) {
            // Nested Attributes block — read as its own Typed struct.
            s << "        t." << fid << " = " << cppCommonType(f, file)
              << "::from_json_value(json::require(v, \"" << fid << "\"));\n";
            continue;
        }
        s << "        t." << fid << " = json::Convert<" << cppCommonType(f, file)
          << ">::from_json(json::require(v, \"" << fid << "\"), \"" << fid << "\");\n";
    }
    s << "        return t;\n";
    s << "    }\n\n";

    s << "    static " << typedName << " from_json(const std::string& text) {\n";
    s << "        return from_json_value(json::parse(text));\n";
    s << "    }\n\n";

    s << "    static std::string to_json_list(const std::vector<" << typedName << ">& list) {\n";
    s << "        json::Elements e;\n";
    s << "        for (const auto& item : list) e.push_back(item.to_json_value());\n";
    s << "        return json::write(json::Value::make_array(std::move(e)));\n";
    s << "    }\n\n";

    s << "    static std::vector<" << typedName << "> from_json_list(const std::string& text) {\n";
    s << "        std::vector<" << typedName << "> result;\n";
    s << "        const json::Value v = json::parse(text);\n";
    s << "        json::require_array(v, \"" << typedName << "\");\n";
    s << "        for (const json::Value& e : v.elements())\n";
    s << "            result.push_back(from_json_value(e));\n";
    s << "        return result;\n";
    s << "    }\n\n";

    s << "    bool operator==(const " << typedName << "& o) const {\n";
    if (as.fields.isEmpty()) {
        s << "        return true;\n";
    } else {
        s << "        return ";
        for (int i = 0; i < as.fields.size(); ++i) {
            if (i) s << "\n            && ";
            const QString fid = toIdentifier(as.fields[i].name);
            s << fid << " == o." << fid;
        }
        s << ";\n";
    }
    s << "    }\n";
    s << "    bool operator!=(const " << typedName << "& o) const { return !(*this == o); }\n";
    s << "};\n";

    return out;
}

// ---------------------------------------------------------------------------
// Common aggregate header
// ---------------------------------------------------------------------------

// Every .spectable in a project generates into the same common folder, and this
// aggregate header is rewritten on each one. Emitting only the current file's
// AttrSets meant the last file processed erased every struct contributed by the
// others. The existing header is therefore merged with the new entries.
QString CppGenerator::genCommonHeader(const QVector<AttrSet>& attrSets,
                                      const QString& existing,
                                      const QString& dir) const
{
    QStringList includes;
    for (const QString& line : existing.split('\n')) {
        const QString t = line.trimmed();
        if (t.startsWith("#include \"") && t != "#include \"json.h\""
         && !includes.contains(t))
            includes << t;
    }
    includes = sourcescan::dropEntriesForMissingFiles(
                   includes, dir, QRegularExpression(R"(^#include "([A-Za-z0-9_]+)\.h")"), ".h");
    for (const AttrSet& as : attrSets) {
        const QString id = toIdentifier(as.name);
        for (const QString& l : { QString("#include \"%1_string.h\"").arg(id),
                                  QString("#include \"%1_typed.h\"").arg(id) })
            if (!includes.contains(l)) includes << l;
    }
    includes.sort();

    QString out;
    QTextStream s(&out);
    s << "#pragma once\n";
    s << "#include \"json.h\"\n";
    for (const QString& l : includes) s << l << "\n";
    return out;
}

// ---------------------------------------------------------------------------
// common/json.h — header-only, dependency-free JSON reader/writer.
// Numbers keep their original text so that round-tripping never loses
// precision.  User-defined DataType classes plug in through json::Convert.
// ---------------------------------------------------------------------------

// Emitted in chunks: MSVC caps a single string literal at 16380 bytes.
static QString genJsonHeader()
{
    QString out;
    out += QString::fromLatin1(R"CPP(#pragma once
// Minimal dependency-free JSON reader/writer used by the generated Typed structs.
// No third-party library required.

#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace json {

class Value;
using Members  = std::vector<std::pair<std::string, Value>>;
using Elements = std::vector<Value>;

/** Thrown for malformed JSON, a missing field, or a value of the wrong type. */
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& msg) : std::runtime_error(msg) {}
};

/** Shortest representation of d that reads back as exactly d. */
inline std::string number_to_string(double d) {
    for (int prec = 15; prec <= 17; ++prec) {
        std::ostringstream ss;
        ss << std::setprecision(prec) << d;
        const std::string s = ss.str();
        if (std::strtod(s.c_str(), nullptr) == d) return s;
    }
    std::ostringstream ss;
    ss << std::setprecision(17) << d;
    return ss.str();
}

class Value {
public:
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Value() = default;

    static Value make_null()                    { return Value(); }
    static Value make_bool(bool b)              { Value v; v.kind_ = Kind::Bool;   v.bool_ = b; return v; }
    static Value make_number_raw(std::string r) { Value v; v.kind_ = Kind::Number; v.text_ = std::move(r); return v; }
    static Value make_number(double d)          { return make_number_raw(number_to_string(d)); }
    static Value make_number(int i)             { return make_number_raw(std::to_string(i)); }
    static Value make_number(long long i)       { return make_number_raw(std::to_string(i)); }
    static Value make_string(std::string s)     { Value v; v.kind_ = Kind::String; v.text_ = std::move(s); return v; }
    static Value make_array(Elements e)         { Value v; v.kind_ = Kind::Array;  v.arr_ = std::make_shared<Elements>(std::move(e)); return v; }
    static Value make_object(Members m)         { Value v; v.kind_ = Kind::Object; v.obj_ = std::make_shared<Members>(std::move(m)); return v; }

    Kind kind()      const { return kind_; }
    bool is_null()   const { return kind_ == Kind::Null; }
    bool is_array()  const { return kind_ == Kind::Array; }
    bool is_object() const { return kind_ == Kind::Object; }

    bool               boolean() const { return bool_; }
    const std::string& text()    const { return text_; }

    const Elements& elements() const {
        static const Elements empty;
        return arr_ ? *arr_ : empty;
    }
    const Members& members() const {
        static const Members empty;
        return obj_ ? *obj_ : empty;
    }

    /** Pointer to the named member, or nullptr when absent. */
    const Value* find(const std::string& key) const {
        if (!obj_) return nullptr;
        for (const auto& kv : *obj_)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }

    std::string describe() const {
        switch (kind_) {
            case Kind::Null:   return "null";
            case Kind::Bool:   return "a boolean";
            case Kind::Number: return "a number";
            case Kind::String: return "a string";
            case Kind::Array:  return "an array";
            case Kind::Object: return "an object";
        }
        return "a value";
    }

private:
    Kind        kind_ = Kind::Null;
    bool        bool_ = false;
    std::string text_;
    std::shared_ptr<Elements> arr_;
    std::shared_ptr<Members>  obj_;
};

)CPP");
    out += QString::fromLatin1(R"CPP(
// ---------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------

inline void write_string(std::ostringstream& os, const std::string& s) {
    os << '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b";  break;
            case '\f': os << "\\f";  break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:
                if (c < 0x20) {
                    os << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(c) << std::dec << std::setfill(' ');
                } else {
                    os << static_cast<char>(c);   // UTF-8 bytes pass through
                }
        }
    }
    os << '"';
}

inline void write_value(std::ostringstream& os, const Value& v) {
    switch (v.kind()) {
        case Value::Kind::Null:   os << "null"; break;
        case Value::Kind::Bool:   os << (v.boolean() ? "true" : "false"); break;
        case Value::Kind::Number: os << v.text(); break;
        case Value::Kind::String: write_string(os, v.text()); break;
        case Value::Kind::Array: {
            os << '[';
            bool first = true;
            for (const Value& e : v.elements()) {
                if (!first) os << ',';
                first = false;
                write_value(os, e);
            }
            os << ']';
            break;
        }
        case Value::Kind::Object: {
            os << '{';
            bool first = true;
            for (const auto& kv : v.members()) {
                if (!first) os << ',';
                first = false;
                write_string(os, kv.first);
                os << ':';
                write_value(os, kv.second);
            }
            os << '}';
            break;
        }
    }
}

inline std::string write(const Value& v) {
    std::ostringstream os;
    write_value(os, v);
    return os.str();
}

// ---------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------

class Parser {
public:
    explicit Parser(const std::string& src) : src_(src) {}

    Value parse_document() {
        Value v = read_value();
        skip_ws();
        if (i_ != src_.size())
            throw Error("Trailing content at offset " + std::to_string(i_));
        return v;
    }

private:
    const std::string& src_;
    std::size_t i_ = 0;

    bool at_end() const { return i_ >= src_.size(); }

    void skip_ws() {
        while (i_ < src_.size()) {
            const char c = src_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i_;
            else break;
        }
    }

    [[noreturn]] void fail(const std::string& msg) const {
        throw Error(msg + " at offset " + std::to_string(i_));
    }

    void expect_word(const char* word) {
        const std::size_t n = std::char_traits<char>::length(word);
        if (src_.compare(i_, n, word) != 0) fail(std::string("Expected '") + word + "'");
        i_ += n;
    }

    Value read_value() {
        skip_ws();
        if (at_end()) fail("Unexpected end of JSON input");
        switch (src_[i_]) {
            case '{': return read_object();
            case '[': return read_array();
            case '"': return Value::make_string(read_string());
            case 't': expect_word("true");  return Value::make_bool(true);
            case 'f': expect_word("false"); return Value::make_bool(false);
            case 'n': expect_word("null");  return Value::make_null();
            default:  return read_number();
        }
    }

)CPP");
    out += QString::fromLatin1(R"CPP(
    Value read_object() {
        Members m;
        ++i_;                       // consume '{'
        skip_ws();
        if (!at_end() && src_[i_] == '}') { ++i_; return Value::make_object(std::move(m)); }
        for (;;) {
            skip_ws();
            if (at_end() || src_[i_] != '"') fail("Expected a string key");
            std::string key = read_string();
            skip_ws();
            if (at_end() || src_[i_] != ':') fail("Expected ':' after key '" + key + "'");
            ++i_;
            m.emplace_back(std::move(key), read_value());
            skip_ws();
            if (at_end()) fail("Unterminated object");
            const char d = src_[i_];
            if (d == ',') { ++i_; continue; }
            if (d == '}') { ++i_; return Value::make_object(std::move(m)); }
            fail("Expected ',' or '}'");
        }
    }

    Value read_array() {
        Elements e;
        ++i_;                       // consume '['
        skip_ws();
        if (!at_end() && src_[i_] == ']') { ++i_; return Value::make_array(std::move(e)); }
        for (;;) {
            e.push_back(read_value());
            skip_ws();
            if (at_end()) fail("Unterminated array");
            const char d = src_[i_];
            if (d == ',') { ++i_; continue; }
            if (d == ']') { ++i_; return Value::make_array(std::move(e)); }
            fail("Expected ',' or ']'");
        }
    }

    /** Encode one code point as UTF-8 (so \uXXXX escapes survive the round trip). */
    static void append_utf8(std::string& out, unsigned int cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    unsigned int read_hex4() {
        if (i_ + 4 > src_.size()) fail("Truncated \\u escape");
        unsigned int cp = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = src_[i_ + k];
            cp <<= 4;
            if      (c >= '0' && c <= '9') cp |= static_cast<unsigned int>(c - '0');
            else if (c >= 'a' && c <= 'f') cp |= static_cast<unsigned int>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp |= static_cast<unsigned int>(c - 'A' + 10);
            else fail("Invalid \\u escape");
        }
        i_ += 4;
        return cp;
    }

    std::string read_string() {
        ++i_;                       // consume opening quote
        std::string out;
        for (;;) {
            if (at_end()) fail("Unterminated string");
            const char c = src_[i_++];
            if (c == '"')  return out;
            if (c != '\\') { out += c; continue; }
            if (at_end()) fail("Unterminated escape");
            const char e = src_[i_++];
            switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    unsigned int cp = read_hex4();
                    // Combine a surrogate pair when both halves are present.
                    if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 1 < src_.size()
                        && src_[i_] == '\\' && src_[i_ + 1] == 'u') {
                        const std::size_t save = i_;
                        i_ += 2;
                        const unsigned int lo = read_hex4();
                        if (lo >= 0xDC00 && lo <= 0xDFFF)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        else
                            i_ = save;
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail(std::string("Invalid escape '\\") + e + "'");
            }
        }
    }

    Value read_number() {
        const std::size_t start = i_;
        if (!at_end() && src_[i_] == '-') ++i_;
        while (!at_end()) {
            const char c = src_[i_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E'
             || c == '+' || c == '-') ++i_;
            else break;
        }
        if (start == i_) fail("Expected a value");
        const std::string text = src_.substr(start, i_ - start);
        char* end = nullptr;
        std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0')
            throw Error("Invalid number '" + text + "' at offset " + std::to_string(start));
        return Value::make_number_raw(text);
    }
};

inline Value parse(const std::string& text) {
    Parser p(text);
    return p.parse_document();
}

)CPP");
    out += QString::fromLatin1(R"CPP(
// ---------------------------------------------------------------------
// Field accessors — a missing key or a wrong type throws json::Error.
// An explicit JSON null is passed through rather than treated as an error.
// ---------------------------------------------------------------------

inline const Value& require(const Value& obj, const std::string& key) {
    if (!obj.is_object()) throw Error("Expected an object holding field '" + key + "'");
    const Value* v = obj.find(key);
    if (!v) throw Error("Missing JSON field '" + key + "'");
    return *v;
}

[[noreturn]] inline void type_error(const std::string& ctx, const std::string& expected,
                                    const Value& actual) {
    throw Error("JSON field '" + ctx + "' is not " + expected + " (got " + actual.describe() + ")");
}

inline std::string as_string(const Value& v, const std::string& ctx) {
    if (v.kind() == Value::Kind::String || v.kind() == Value::Kind::Number) return v.text();
    if (v.kind() == Value::Kind::Bool)   return v.boolean() ? "true" : "false";
    if (v.is_null())                     return std::string();
    type_error(ctx, "a string", v);
}

inline double as_double(const Value& v, const std::string& ctx) {
    if (v.kind() == Value::Kind::Number || v.kind() == Value::Kind::String) {
        const std::string& t = v.text();
        char* end = nullptr;
        const double d = std::strtod(t.c_str(), &end);
        if (end != t.c_str() && *end == '\0') return d;
    }
    type_error(ctx, "a number", v);
}

inline int as_int(const Value& v, const std::string& ctx) {
    const double d = as_double(v, ctx);
    const int    i = static_cast<int>(d);
    if (static_cast<double>(i) != d) type_error(ctx, "an integer", v);
    return i;
}

inline bool as_bool(const Value& v, const std::string& ctx) {
    if (v.kind() == Value::Kind::Bool) return v.boolean();
    if (v.kind() == Value::Kind::String) {
        const std::string& t = v.text();
        if (t == "true"  || t == "t" || t == "yes" || t == "y" || t == "1") return true;
        if (t == "false" || t == "f" || t == "no"  || t == "n" || t == "0") return false;
    }
    type_error(ctx, "a boolean", v);
}

// Returns void rather than the element vector: a reference-returning accessor
// taking a string literal trips -Wdangling-reference at every call site.
// Callers check, then iterate v.elements() on their own named Value.
inline void require_array(const Value& v, const std::string& ctx) {
    if (!v.is_array()) type_error(ctx, "an array", v);
}

// ---------------------------------------------------------------------
// Convert — the customization point for field types.
//
// The default handles any user-defined DataType that is streamable with
// operator<< and constructible from std::string, matching the convention
// the generated Typed structs already use.  Specialize it for types that
// need something else.
// ---------------------------------------------------------------------

template <class T>
struct Convert {
    static Value to_json(const T& v) {
        std::ostringstream ss;
        ss << v;
        return Value::make_string(ss.str());
    }
    static T from_json(const Value& v, const std::string& ctx) {
        return T(as_string(v, ctx));
    }
};

template <>
struct Convert<std::string> {
    static Value       to_json(const std::string& v)                      { return Value::make_string(v); }
    static std::string from_json(const Value& v, const std::string& ctx)  { return as_string(v, ctx); }
};

template <>
struct Convert<int> {
    static Value to_json(int v)                                  { return Value::make_number(v); }
    static int   from_json(const Value& v, const std::string& c) { return as_int(v, c); }
};

template <>
struct Convert<double> {
    static Value  to_json(double v)                                 { return Value::make_number(v); }
    static double from_json(const Value& v, const std::string& c)   { return as_double(v, c); }
};

template <>
struct Convert<bool> {
    static Value to_json(bool v)                                  { return Value::make_bool(v); }
    static bool  from_json(const Value& v, const std::string& c)  { return as_bool(v, c); }
};

} // namespace json
)CPP");
    return out;
}

// ---------------------------------------------------------------------------
// Test file
// ---------------------------------------------------------------------------

QString CppGenerator::genTestFile(const SpectableFile& file, const QString& specSnake,
                                   const QString& glueClass, const QString& commonRelPath,
                                   QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    s << "#include <gtest/gtest.h>\n";
    s << "#include <iostream>\n";
    s << "#include \"" << commonRelPath << "/common.h\"\n";
    s << "#include \"" << specSnake << "_glue.h\"\n";
    for (const QString& inc : m_extraIncludes) s << inc << "\n";
    s << "\n";

    int objectCounter = 0;
    const QString specClass = toTypeName(file.specName);

    // A block with a nested-object field cannot be built from a flat vector, so
    // those rows use a braced initializer instead.
    auto emitStrVec = [&](const QString& listType, const QVector<QStringList>& rows,
                          const AttrSet* as) {
        bool hasNested = false;
        if (as)
            for (const Field& f : as->fields)
                if (isAttrSetType(f.type, file)) { hasNested = true; break; }

        s << "    std::vector<" << listType << "> objectList" << (++objectCounter) << " = {\n";
        for (const QStringList& row : rows) {
            if (hasNested) {
                s << "        " << stringLiteral(*as, row, file) << ",\n";
                continue;
            }
            s << "        " << listType << "::from_vec({";
            for (int ci = 0; ci < row.size(); ++ci) {
                if (ci) s << ", ";
                s << "\"" << cppEscape(row[ci]) << "\"";
            }
            s << "}),\n";
        }
        s << "    };\n";
    };

    auto emitGridVec = [&](const QVector<QStringList>& rows, int startRow = 0) {
        s << "    std::vector<std::vector<std::string>> stringListList" << (++objectCounter) << " = {\n";
        for (int ri = startRow; ri < rows.size(); ++ri) {
            s << "        {";
            const QStringList& r = rows[ri];
            for (int ci = 0; ci < r.size(); ++ci) {
                if (ci) s << ", ";
                s << "\"" << cppEscape(resolveValue(r[ci], file)) << "\"";
            }
            s << "},\n";
        }
        s << "    };\n";
    };

    auto emitSteps = [&](const QVector<Step>& steps, const QString& glueVar) {
        for (const Step& step : steps) {
            if (step.hasDocString) {
                const QString meth = toFnName(step.keyword, step.text);
                s << "    " << glueVar << "." << meth << "(\"" << cppEscape(step.docString) << "\");\n";
                continue;
            }
            if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
                const Define* def = findDefine(step.defineRef, file);
                if (def && def->hasDocString) {
                    const QString meth = toFnName(step.keyword, step.text);
                    s << "    " << glueVar << "." << meth << "(\"" << cppEscape(def->docString) << "\");\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                s << "    " << glueVar << "." << toFnName(step.keyword, step.text) << "();\n";
                continue;
            }

            const QString effectiveAttrSetName = (!step.attrSetName.isEmpty() && isCollectionType(step.attrSetName, file))
                ? collectionElementType(step.attrSetName, file)
                : step.attrSetName;
            const AttrSet* as = findAttrSet(effectiveAttrSetName, file);

            if (!step.attrSetName.isEmpty() && !as) {
                if (!isDataType(effectiveAttrSetName, file)) {
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
                const QString listType = toTypeName(effectiveAttrSetName) + "String";
                const int idx = objectCounter + 1;
                emitStrVec(listType, rows, as);
                s << "    " << glueVar << "." << meth << "(objectList" << idx << ");\n\n";
            } else {
                const StepTable& tbl = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty() && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;
                const int idx = objectCounter + 1;
                emitGridVec(tbl.rows, startRow);
                s << "    " << glueVar << "." << meth << "(stringListList" << idx << ");\n\n";
            }
        }
    };

    // Scenario tests
    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;
        const QString testName = "Scenario_" + toTypeName(sc.name);
        s << "TEST(" << specClass << ", " << testName << ") {\n";
        s << "    " << glueClass << " glue;\n";
        emitSteps(file.backgroundSteps, "glue");
        emitSteps(sc.steps, "glue");
        s << "}\n\n";
    }

    // NamedBlock tests
    static const QStringList namedKinds = { "BusinessRule", "Calculation", "DataType" };
    QSet<QString> seenBlocks;
    for (const QString& kind : namedKinds) {
        bool hasKind = false;
        for (const NamedBlock& nb : file.namedBlocks)
            if (nb.hasExamples && nb.kind == kind && !nb.isContext
                && !seenBlocks.contains(kind + ":" + nb.name.toLower()))
                { hasKind = true; break; }
        if (!hasKind) continue;

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind || nb.isContext) continue;
            const QString blockKey = kind + ":" + nb.name.toLower();
            if (seenBlocks.contains(blockKey)) {
                errors << QString("WARNING:%1:%2 '%3' declared in multiple files — only first tested")
                              .arg(nb.line).arg(kind).arg(nb.name);
                continue;
            }
            seenBlocks.insert(blockKey);

            const QStringList effectiveGenTags = file.generatorTags + nb.generatorTags;
            if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;

            const QString testName = kind + "_" + toTypeName(nb.name);
            const QString glueFn   = "examples_" + kind.toLower() + "_" + toIdentifier(nb.name);
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            s << "TEST(" << specClass << ", " << testName << ") {\n";
            s << "    " << glueClass << " glue;\n";

            if (as) {
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                const QString listType = toTypeName(nb.examples.attrSetName) + "String";
                const int idx = objectCounter + 1;
                emitStrVec(listType, rows, as);
                s << "    glue." << glueFn << "(objectList" << idx << ");\n";
            } else {
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                const int idx = objectCounter + 1;
                emitGridVec(rows);
                s << "    glue." << glueFn << "(stringListList" << idx << ");\n";
            }
            s << "}\n\n";
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Glue file
// ---------------------------------------------------------------------------

QVector<CppGenerator::GlueSig> CppGenerator::collectGlueSigs(const SpectableFile& file)
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
        // A context block belongs to another .spectable and is tested there —
        // generating a stub for it here produces a method no test ever calls.
        if (!nb.hasExamples || nb.isContext) continue;
        const QString meth = "examples_" + nb.kind.toLower() + "_" + toIdentifier(nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr
            : findAttrSet(nb.examples.attrSetName, file);
        sigs.push_back({ meth, as ? (nb.examples.attrSetName + "String") : "grid" });
    }

    return sigs;
}

QString CppGenerator::genStubMethod(const GlueSig& sig)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "    void " << sig.method << "() {\n";
        s << "        ADD_FAILURE() << \"Not implemented: " << sig.method << "\";\n";
        s << "    }\n";
    } else if (sig.paramType == "docstring") {
        s << "    void " << sig.method << "(const std::string& value) {\n";
        s << "        std::cout << value << \"\\n\";\n";
        s << "        ADD_FAILURE() << \"Not implemented: " << sig.method << "\";\n";
        s << "    }\n";
    } else if (sig.paramType == "grid") {
        s << "    void " << sig.method << "(const std::vector<std::vector<std::string>>& values) {\n";
        s << "        for (const auto& row : values) { for (const auto& c : row) std::cout << c << \" \"; std::cout << \"\\n\"; }\n";
        s << "        ADD_FAILURE() << \"Not implemented: " << sig.method << "\";\n";
        s << "    }\n";
    } else {
        const QString pt = toTypeName(sig.paramType);
        s << "    void " << sig.method << "(const std::vector<" << pt << ">& values) {\n";
        s << "        for (const auto& v : values) { std::cout << v.to_string() << \"\\n\"; }\n";
        s << "        ADD_FAILURE() << \"Not implemented: " << sig.method << "\";\n";
        s << "    }\n";
    }
    return out;
}

bool CppGenerator::appendMissingStubs(const QString& gluePath,
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
        if (!scan.contains(QString("void %1(").arg(sig.method)))
            stubs += "\n" + genStubMethod(sig);
    }
    if (stubs.isEmpty()) return false;

    // Insert before the closing "};" of the class
    const int closingBrace = content.lastIndexOf("\n};");
    if (closingBrace < 0) {
        msgs << QString("WARNING:0:Could not locate closing brace in %1 — stubs not added").arg(gluePath);
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

QString CppGenerator::genGlueFile(const SpectableFile& file, const QString& glueClass,
                                   const QString& commonRelPath) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    QString out;
    QTextStream s(&out);

    s << "#pragma once\n";
    s << "#include <gtest/gtest.h>\n";
    s << "#include <iostream>\n";
    s << "#include <string>\n";
    s << "#include <vector>\n";
    s << "#include \"" << commonRelPath << "/common.h\"\n";
    for (const QString& inc : m_extraIncludes) s << inc << "\n";
    s << "\n";
    s << "class " << glueClass << " {\n";
    s << "public:\n";
    s << "    static constexpr const char* DNC_STRING = \"?DNC?\";\n\n";

    for (const GlueSig& sig : sigs)
        s << genStubMethod(sig) << "\n";

    s << "};\n";
    return out;
}

// ---------------------------------------------------------------------------
// Production class generators
// ---------------------------------------------------------------------------

static QString genProductionEntityCpp(const AttrSet& as)
{
    QString out;
    QTextStream s(&out);

    QStringList includes;
    for (const Field& f : as.fields) {
        const QString tl = f.type.trimmed().toLower();
        if (tl == "string" || tl == "text" || tl == "character" || tl == "char"
         || tl == "date"   || tl == "time" || tl == "datetime"  || tl == "duration")
            if (!includes.contains("<string>")) includes << "<string>";
    }

    s << "#pragma once\n";
    for (const QString& inc : includes) s << "#include " << inc << "\n";
    s << "\n";

    s << "struct " << as.name << " {\n";
    for (const Field& f : as.fields) {
        const QString ct = CppGenerator::cppType(f.type);  // need to make cppType accessible or inline
        // We'll inline the logic here
        const QString tl = f.type.trimmed().toLower();
        QString ctype;
        if      (tl == "integer" || tl == "int")                                    ctype = "int";
        else if (tl == "float"   || tl == "decimal" || tl == "scientific")          ctype = "double";
        else if (tl == "boolean" || tl == "yesno" || tl == "bool")                  ctype = "bool";
        else if (tl == "string"  || tl == "text"  || tl == "character" || tl == "char"
              || tl == "date"    || tl == "time"  || tl == "datetime"  || tl == "duration")
            ctype = "std::string";
        else
            ctype = f.type.trimmed();

        s << "    " << ctype << " " << f.name;
        if (!f.defaultValue.isEmpty()) {
            if (tl == "integer" || tl == "int" || tl == "float" || tl == "decimal" || tl == "scientific")
                s << " = " << f.defaultValue;
            else if (tl == "boolean" || tl == "yesno" || tl == "bool")
                s << " = " << (f.defaultValue.toLower() == "true" || f.defaultValue == "1" || f.defaultValue.toLower() == "yes" ? "true" : "false");
            else
                s << " = \"" << f.defaultValue << "\"";
        } else {
            if (tl == "integer" || tl == "int") s << " = 0";
            else if (tl == "float" || tl == "decimal" || tl == "scientific") s << " = 0.0";
            else if (tl == "boolean" || tl == "yesno" || tl == "bool") s << " = false";
        }
        s << ";\n";
    }
    s << "\n";

    // Constructor
    s << "    " << as.name << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString tl = as.fields[i].type.trimmed().toLower();
        QString ctype;
        if      (tl == "integer" || tl == "int")   ctype = "int";
        else if (tl == "float"   || tl == "decimal" || tl == "scientific") ctype = "double";
        else if (tl == "boolean" || tl == "yesno" || tl == "bool") ctype = "bool";
        else if (tl == "string"  || tl == "text"  || tl == "character" || tl == "char"
              || tl == "date"    || tl == "time"  || tl == "datetime"  || tl == "duration")
            ctype = "std::string";
        else
            ctype = as.fields[i].type.trimmed();

        if (ctype == "std::string")
            s << "std::string " << as.fields[i].name << "_";
        else
            s << ctype << " " << as.fields[i].name << "_";
    }
    s << ")\n        : ";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << as.fields[i].name << "(" << as.fields[i].name << "_)";
    }
    s << " {}\n";
    s << "};\n";

    return out;
}

static QString genProductionCollectionCpp(const Collection& col)
{
    const QString elemType = col.elementType;
    const QString elemFile = elemType + ".h";
    QString out;
    QTextStream s(&out);

    s << "#pragma once\n";
    s << "#include <vector>\n";
    s << "#include <algorithm>\n";
    s << "#include \"" << elemFile << "\"\n";
    s << "\n";
    s << "class " << col.name << " {\n";
    s << "public:\n";
    if (!col.minimum.isEmpty())
        s << "    static constexpr int MINIMUM = " << col.minimum << ";\n";
    if (!col.maximum.isEmpty())
        s << "    static constexpr int MAXIMUM = " << col.maximum << ";\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty())
        s << "\n";

    s << "    void add(const " << elemType << "& item) { items_.push_back(item); }\n\n";

    s << "    bool remove(const " << elemType << "& item) {\n";
    s << "        auto it = std::find_if(items_.begin(), items_.end(),\n";
    s << "            [&](const " << elemType << "& x){ return x.name == item.name; });\n";
    s << "        if (it == items_.end()) return false;\n";
    s << "        items_.erase(it); return true;\n";
    s << "    }\n\n";

    s << "    const std::vector<" << elemType << ">& read() const { return items_; }\n\n";

    s << "    bool update(const " << elemType << "& old_item, const " << elemType << "& new_item) {\n";
    s << "        auto it = std::find_if(items_.begin(), items_.end(),\n";
    s << "            [&](const " << elemType << "& x){ return x.name == old_item.name; });\n";
    s << "        if (it == items_.end()) return false;\n";
    s << "        *it = new_item; return true;\n";
    s << "    }\n\n";

    s << "    int size() const { return static_cast<int>(items_.size()); }\n\n";

    s << "private:\n";
    s << "    std::vector<" << elemType << "> items_;\n";
    s << "};\n";

    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool CppGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList CppGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraIncludes = opts.extraIncludes;
    m_tagFilter     = opts.tagFilter;

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

    // Relative #include path from the (possibly mirrored) output dir back to common/
    const QString commonRelPath = dir.relativeFilePath(commonDir.path());

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

    // Write common structs
    QVector<AttrSet> domainSets;
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        const QString id = toIdentifier(as.name);
        writeFile(commonDir.filePath(id + "_string.h"), genStringHeader(as, augmented), msgs);
        writeFile(commonDir.filePath(id + "_typed.h"),  genTypedHeader(as, augmented),  msgs);
        domainSets.push_back(as);
    }
    writeFile(commonDir.filePath("json.h"),   genJsonHeader(),                 msgs);
    {
        // Read the existing header so structs from the other .spectable files survive.
        QString existingCommon;
        QFile cf(commonDir.filePath("common.h"));
        if (cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            existingCommon = QTextStream(&cf).readAll();
            cf.close();
        }
        writeFile(commonDir.filePath("common.h"),
                  genCommonHeader(domainSets, existingCommon, commonDir.path()), msgs);
    }

    // Test file (always overwritten)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specSnake, glueClass, commonRelPath, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(dir.filePath("test_" + specSnake + ".cpp"), testContent, msgs);
    }

    // Glue file
    {
        const QString gluePath = dir.filePath(specSnake + "_glue.h");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, glueClass, commonRelPath), msgs);
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

            // A production file is only ever created, never overwritten. Look for a
            // declaration of the type anywhere in the folder rather than only for the
            // filename we would write, so a developer who groups several classes in one
            // file does not get a duplicate declaration emitted beside their own.
            const sourcescan::ProductionScan prodScan(prodDir.path(), {"*.h"});
            auto alreadyImplemented = [&](const QString& prodPath, const QString& typeName) {
                if (QFile::exists(prodPath)) return true;
                const QString other = prodScan.declaredIn(typeName);
                if (other.isEmpty()) return false;
                msgs << QString("INFO:0:Production type '%1' is already implemented in %2 "
                                "- no template written").arg(typeName, other);
                return true;
            };
            // Entities
            for (const AttrSet& as : file.attrSets) {
                if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
                const QString prodPath = prodDir.filePath(as.name + ".h");
                if (!alreadyImplemented(prodPath, as.name))
                    writeFile(prodPath, genProductionEntityCpp(as), msgs);
            }
            // Collections
            for (const Collection& col : file.collections) {
                if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
                const QString prodPath = prodDir.filePath(col.name + ".h");
                if (!alreadyImplemented(prodPath, col.name))
                    writeFile(prodPath, genProductionCollectionCpp(col), msgs);
            }
        }
    }

    return msgs;
}
