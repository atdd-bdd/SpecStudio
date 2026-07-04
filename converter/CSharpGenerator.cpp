#include "CSharpGenerator.h"
#include "TagFilter.h"

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
    if (t == "float"   || t == "decimal")      return "double";
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
QString CSharpGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")     return QString("int.Parse(this.%1)").arg(field);
    if (t == "float"   || t == "decimal") return QString("double.Parse(this.%1)").arg(field);
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
// For a transposed table: each row is [AttrName, Value]; we collect one instance.
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

        // A step with Transposed flag and a defineRef means the define rows are key=value pairs.
        // Also, if the define was explicitly detected as transposed (via "Attribute"/"Name" header),
        // treat as key=value. Otherwise if step.transposed, treat as key=value from row 0.
        const bool useKV = def->transposed || step.transposed;
        if (useKV) {
            // key=value rows: each row is [FieldName, Value].
            // If def->transposed: row 0 is the "Attribute/Name" header — skip it.
            // Otherwise: row 0 is the first data row — start from 0.
            const int startIdx = def->transposed ? 1 : 0;
            QStringList row(fieldCount);
            for (int ri = startIdx; ri < def->tableRows.size(); ++ri) {
                const QStringList& r = def->tableRows[ri];
                if (r.size() < 2) continue;
                QString key = r[0].toLower();
                if (fieldIdx.contains(key))
                    row[fieldIdx[key]] = resolveCell(r[1]);
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
                    if (colMap[ci] >= 0) row[colMap[ci]] = resolveCell(dr[ci]);
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
                if (colMap[ci] >= 0) row[colMap[ci]] = dr[ci];
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

QString CSharpGenerator::genStringClass(const AttrSet& as, const QString& ns) const
{
    const QString cn = as.name + "String";
    QString out;
    QTextStream s(&out);

    bool needsList = false;
    for (const Field& f : as.fields)
        if (f.multiples) { needsList = true; break; }

    s << "namespace " << ns << "\n{\n";
    if (needsList) s << "using System.Collections.Generic;\n";
    for (const QString& u : m_extraImports) s << u << "\n";
    if (needsList || !m_extraImports.isEmpty()) s << "\n";
    s << "    public class " << cn << "\n    {\n";

    // Fields
    for (const Field& f : as.fields)
        s << "        public string " << toCamelCase(f.name) << ";\n";
    s << "\n";

    // Constructor
    s << "        public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << "string " << toCamelCase(as.fields[i].name);
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
        const QString expr = parseExpr(toCamelCase(f.name), f.type);
        if (f.multiples)
            s << "                new List<" << csharpType(f.type) << "> { " << expr << " }";
        else
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
// Typed class generator
// ---------------------------------------------------------------------------

QString CSharpGenerator::genTypedClass(const AttrSet& as, const QString& ns) const
{
    const QString cn = as.name + "Typed";
    QString out;
    QTextStream s(&out);

    bool needsList = false;
    for (const Field& f : as.fields)
        if (f.multiples) { needsList = true; break; }

    s << "namespace " << ns << "\n{\n";
    if (needsList) s << "using System.Collections.Generic;\n";
    for (const QString& u : m_extraImports) s << u << "\n";
    if (needsList || !m_extraImports.isEmpty()) s << "\n";
    s << "    public class " << cn << "\n    {\n";

    // Fields
    for (const Field& f : as.fields) {
        if (f.multiples)
            s << "        public List<" << csharpType(f.type) << "> " << toCamelCase(f.name) << ";\n";
        else
            s << "        public " << csharpType(f.type) << " " << toCamelCase(f.name) << ";\n";
    }
    s << "\n";

    // Constructor
    s << "        public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        if (f.multiples)
            s << "List<" << csharpType(f.type) << "> " << toCamelCase(f.name);
        else
            s << csharpType(f.type) << " " << toCamelCase(f.name);
    }
    s << ")\n        {\n";
    for (const Field& f : as.fields)
        s << "            this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
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

            const AttrSet* as = findAttrSet(step.attrSetName, file);

            if (!step.attrSetName.isEmpty() && as == nullptr) {
                if (!isDataType(step.attrSetName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined — add an 'Attributes %2' block")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
                // DataType grid step — fall through to the List<List<string>> branch below
            }

            if (!step.attrSetName.isEmpty() && as) {
                // Typed list
                ++objectCounter;
                const QString listType  = step.attrSetName + "String";
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
                int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.transposed) ? 1 : 0;

                s << "         List<List<string>> " << listVar
                  << " = new List<List<string>>{\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "            new List<string>{ ";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << resolveCell(r[ci]) << "\"";
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
        if (!TagFilter::matches(m_tagFilter, sc.generatorTags)) continue;
        const QString meth = "Test_Scenario_" + toClassName(sc.name);
        const QString glueClass = className + "_glue";
        const QString glueVar   = glueClass[0].toLower() + glueClass.mid(1) + "_object";

        emitCsCategories(sc.tags);
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
    for (const QString& kind : namedKinds) {
        bool hasKind = false;
        for (const NamedBlock& nb : file.namedBlocks)
            if (nb.hasExamples && nb.kind == kind) { hasKind = true; break; }
        if (!hasKind) continue;

        s << "// -------------------------\n";
        s << "// " << kind << " Tests\n";
        s << "// -------------------------\n";

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind) continue;
            if (!TagFilter::matches(m_tagFilter, nb.generatorTags)) continue;

            const QString meth      = kind + "_" + toClassName(nb.name);
            const QString glueMeth  = "Examples" + kind + "_" + toClassName(nb.name);
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
                        s << "\"" << resolveCell(row[ci]) << "\"";
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
                sigs.push_back({ meth, step.attrSetName + "String", true });
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
        if (!nb.hasExamples) continue;
        const QString meth = "Examples" + nb.kind + "_" + toClassName(nb.name);
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

    QString stubs;
    for (const GlueSig& sig : sigs) {
        const QString signature = QStringLiteral("public void %1(").arg(sig.method);
        if (!content.contains(signature))
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
                    f.type = (isValidValues && c.compare("valid", Qt::CaseInsensitive) == 0)
                             ? "Boolean" : "String";
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

    // 1. String + Typed classes for each AttrSet → go into common/
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }

        writeFile(commonDir.filePath(as.name + "String.cs"), genStringClass(as, commonNs), msgs);
        writeFile(commonDir.filePath(as.name + "Typed.cs"),  genTypedClass(as, commonNs),  msgs);
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

    return msgs;
}
