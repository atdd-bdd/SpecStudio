#include "RustGenerator.h"
#include "TagFilter.h"

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
    if (t == "float"   || t == "decimal")                     return "f64";
    if (t == "boolean" || t == "yesno" || t == "bool")        return "bool";
    if (t == "string" || t == "text"
     || t == "character" || t == "char")                       return "String";
    // date/time/duration — returned as String without external crate dependencies
    if (t == "date" || t == "time" || t == "datetime"
     || t == "duration")                                       return "String";
    return specType.trimmed();
}

QString RustGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("s.%1.parse::<i32>().unwrap_or_default()").arg(field);
    if (t == "float" || t == "decimal")
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
    return s.toLower();
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
        "Character", "String", "Text", "Integer", "Float", "Boolean",
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

        if (def->transposed || step.transposed) {
            QStringList row(fieldCount);
            const int start = def->transposed ? 1 : 0;
            for (int ri = start; ri < def->tableRows.size(); ++ri) {
                const QStringList& r = def->tableRows[ri];
                if (r.size() < 2) continue;
                const QString key = r[0].toLower();
                if (fieldIdx.contains(key)) row[fieldIdx[key]] = resolveCell(r[1]);
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
                    if (colMap[ci] >= 0) row[colMap[ci]] = resolveCell(dr[ci]);
                result << row;
            }
        }
        return result;
    }

    if (!step.hasTable || step.table.rows.isEmpty()) return result;

    QMap<QString, int> fieldIdx;
    for (int i = 0; i < attrSet->fields.size(); ++i)
        fieldIdx[attrSet->fields[i].name.toLower()] = i;

    if (step.table.transposed) {
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
                    row[fieldIdx[key]] = r[col];
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
                if (colMap[ci] >= 0) row[colMap[ci]] = resolveCell(dr[ci]);
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

QString RustGenerator::genStringStruct(const AttrSet& as) const
{
    const QString typeName = toTypeName(as.name) + "String";
    QString out;
    QTextStream s(&out);

    s << "#![allow(dead_code, unused_imports, unused_variables)]\n\n";
    for (const QString& u : m_extraUses) s << u << "\n";
    if (!m_extraUses.isEmpty()) s << "\n";

    s << "#[derive(Debug, Clone, Default)]\n";
    s << "pub struct " << typeName << " {\n";
    for (const Field& f : as.fields)
        s << "    pub " << toIdentifier(f.name) << ": String,\n";
    s << "}\n\n";

    s << "impl " << typeName << " {\n";
    s << "    pub fn from_vec(v: &[&str]) -> Self {\n";
    s << "        Self {\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        s << "            " << toIdentifier(as.fields[i].name)
          << ": v.get(" << i << ").copied().unwrap_or(\"\").to_string(),\n";
    }
    s << "        }\n    }\n}\n\n";

    s << "impl std::fmt::Display for " << typeName << " {\n";
    s << "    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {\n";
    s << "        write!(f,\n";

    QStringList placeholders, args;
    for (const Field& field : as.fields) {
        placeholders << (field.name + "={}");
        args << "self." + toIdentifier(field.name);
    }
    s << "            \"" << placeholders.join(", ") << "\",\n";
    for (int i = 0; i < args.size(); ++i) {
        s << "            " << args[i];
        if (i < args.size() - 1) s << ",";
        s << "\n";
    }
    s << "        )\n    }\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed struct
// ---------------------------------------------------------------------------

QString RustGenerator::genTypedStruct(const AttrSet& as) const
{
    const QString strTypeName = toTypeName(as.name) + "String";
    const QString typedName   = toTypeName(as.name) + "Typed";
    const QString strMod      = toIdentifier(as.name) + "_string";
    QString out;
    QTextStream s(&out);

    s << "#![allow(dead_code, unused_imports, unused_variables)]\n\n";
    s << "use super::" << strMod << "::" << strTypeName << ";\n";
    for (const QString& u : m_extraUses) s << u << "\n";
    s << "\n";

    s << "#[derive(Debug, Clone, Default)]\n";
    s << "pub struct " << typedName << " {\n";
    for (const Field& f : as.fields) {
        const QString rt = rustType(f.type);
        if (f.multiples)
            s << "    pub " << toIdentifier(f.name) << ": Vec<" << rt << ">,\n";
        else
            s << "    pub " << toIdentifier(f.name) << ": " << rt << ",\n";
    }
    s << "}\n\n";

    s << "impl " << typedName << " {\n";
    s << "    pub fn from_str_struct(s: &" << strTypeName << ") -> Self {\n";
    s << "        Self {\n";
    for (const Field& f : as.fields) {
        const QString fid  = toIdentifier(f.name);
        const QString expr = parseExpr(fid, f.type);
        if (f.multiples)
            s << "            " << fid << ": vec![" << expr << "],\n";
        else
            s << "            " << fid << ": " << expr << ",\n";
    }
    s << "        }\n    }\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Common mod.rs
// ---------------------------------------------------------------------------

QString RustGenerator::genCommonMod(const QVector<AttrSet>& attrSets) const
{
    QString out;
    QTextStream s(&out);
    for (const AttrSet& as : attrSets) {
        const QString mod = toIdentifier(as.name);
        s << "pub mod " << mod << "_string;\n";
        s << "pub mod " << mod << "_typed;\n";
        s << "pub use " << mod << "_string::*;\n";
        s << "pub use " << mod << "_typed::*;\n";
    }
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
    s << "use crate::" << specSnake << "_glue::" << glueStruct << ";\n";
    for (const QString& u : m_extraUses) s << u << "\n";
    s << "\n";

    // Helper: emit a slice of AttrSetString rows inline
    auto emitStrSlice = [&](const QString& listType, const QVector<QStringList>& rows) {
        if (rows.size() == 1) {
            s << "&[" << listType << "::from_vec(&[";
            const QStringList& row = rows[0];
            for (int ci = 0; ci < row.size(); ++ci) {
                if (ci) s << ", ";
                s << "\"" << rustEscape(row[ci]) << "\"";
            }
            s << "])]";
        } else {
            s << "&[\n";
            for (const QStringList& row : rows) {
                s << "        " << listType << "::from_vec(&[";
                for (int ci = 0; ci < row.size(); ++ci) {
                    if (ci) s << ", ";
                    s << "\"" << rustEscape(row[ci]) << "\"";
                }
                s << "]),\n";
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
                s << "\"" << rustEscape(resolveCell(r[ci])) << "\".to_string()";
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
                s << "    glue." << meth << "(";
                emitStrSlice(listType, rows);
                s << ");\n";
            } else {
                const StepTable& tbl = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.transposed) ? 1 : 0;
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
        if (!TagFilter::matches(m_tagFilter, sc.generatorTags)) continue;
        const QString fn = "scenario_" + toIdentifier(sc.name);

        if (!sc.tags.isEmpty())
            s << "// Tags: " << sc.tags.join(", ") << "\n";
        s << "#[test]\n";
        s << "fn " << fn << "() {\n";
        s << "    let mut glue = " << glueStruct << "::new();\n";
        emitSteps(file.backgroundSteps);
        emitSteps(sc.steps);
        s << "}\n\n";
    }

    // ── BusinessRule / Calculation / DataType tests ──────────────────────────
    static const QStringList namedKinds = { "BusinessRule", "Calculation", "DataType" };
    for (const QString& kind : namedKinds) {
        bool hasKind = false;
        for (const NamedBlock& nb : file.namedBlocks)
            if (nb.hasExamples && nb.kind == kind) { hasKind = true; break; }
        if (!hasKind) continue;

        const QString ks = kindToSnake(kind);
        s << "// --- " << kind << " Tests ---\n\n";

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind) continue;
            if (!TagFilter::matches(m_tagFilter, nb.generatorTags)) continue;

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
                emitStrSlice(listType, rows);
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

    QString stubs;
    for (const GlueSig& sig : sigs) {
        if (!content.contains(QStringLiteral("fn %1(").arg(sig.method)))
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

    QDir outDir(opts.outputDir);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create output directory: %1").arg(outDir.path());
        return msgs;
    }

    QDir commonDir(opts.outputDir + "/common");
    if (!commonDir.exists() && !commonDir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create common directory: %1").arg(commonDir.path());
        return msgs;
    }

    // Copy source .spectable (if enabled)
    if (opts.copySpectable && !file.filePath.isEmpty()) {
        const QString dest = outDir.filePath(QFileInfo(file.filePath).fileName());
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
                f.type = (isVV && c.compare("valid", Qt::CaseInsensitive) == 0)
                         ? "Boolean" : "String";
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
        writeFile(commonDir.filePath(mid + "_string.rs"), genStringStruct(as), msgs);
        writeFile(commonDir.filePath(mid + "_typed.rs"),  genTypedStruct(as),  msgs);
        domainSets.push_back(as);
    }
    writeFile(commonDir.filePath("mod.rs"), genCommonMod(domainSets), msgs);

    // Test file
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specSnake, glueStruct, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(outDir.filePath("test_" + specSnake + ".rs"), testContent, msgs);
    }

    // Glue file
    {
        const QString gluePath = outDir.filePath(specSnake + "_glue.rs");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, glueStruct), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    return msgs;
}
