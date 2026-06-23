#include "CSharpGenerator.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>

// ---------------------------------------------------------------------------
// Cell value helpers
// ---------------------------------------------------------------------------

// Replace ~ with space (tilde is the space placeholder in table cells)
static QString resolveCell(const QString& cell)
{
    QString s = cell;
    return s.replace('~', ' ');
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
     || t == "bool")                      return QString("bool.Parse(this.%1)").arg(field);
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

QString CSharpGenerator::paramName(const QString& fieldName)
{
    // Lowercase first letter to make a constructor parameter name
    if (fieldName.isEmpty()) return fieldName;
    return fieldName[0].toLower() + fieldName.mid(1);
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
        // Each row = [AttrName, Value] → one data row in field order.
        // The "Attribute/Value" header row was consumed but NOT stored in rows,
        // so all entries in step.table.rows are data rows — always start from 0.
        QStringList row(fieldCount);
        for (int ri = 0; ri < step.table.rows.size(); ++ri) {
            const QStringList& r = step.table.rows[ri];
            if (r.size() < 2) continue;
            QString key = r[0].toLower();
            if (fieldIdx.contains(key))
                row[fieldIdx[key]] = r[1];
        }
        result << row;
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
// String class generator
// ---------------------------------------------------------------------------

QString CSharpGenerator::genStringClass(const AttrSet& as, const QString& ns) const
{
    const QString cn = as.name + "String";
    QString out;
    QTextStream s(&out);

    s << "namespace " << ns << "\n{\n";
    s << "    public class " << cn << "\n    {\n";

    // Fields
    for (const Field& f : as.fields)
        s << "        public string " << f.name << ";\n";
    s << "\n";

    // Constructor
    s << "        public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << "string " << paramName(as.fields[i].name);
    }
    s << ")\n        {\n";
    for (const Field& f : as.fields)
        s << "            this." << f.name << " = " << paramName(f.name) << ";\n";
    s << "        }\n\n";

    // To<Name>Typed()
    const QString tn = as.name + "Typed";
    s << "        public " << tn << " To" << tn << "()\n        {\n";
    s << "            return new " << tn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        s << "                " << parseExpr(as.fields[i].name, as.fields[i].type);
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "            );\n        }\n\n";

    // ToString()
    s << "        public override string ToString()\n        {\n";
    s << "            return $\"";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << as.fields[i].name << "={" << as.fields[i].name << "}";
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

    s << "namespace " << ns << "\n{\n";
    s << "    public class " << cn << "\n    {\n";

    // Fields
    for (const Field& f : as.fields)
        s << "        public " << csharpType(f.type) << " " << f.name << ";\n";
    s << "\n";

    // Constructor
    s << "        public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << csharpType(as.fields[i].type) << " " << paramName(as.fields[i].name);
    }
    s << ")\n        {\n";
    for (const Field& f : as.fields)
        s << "            this." << f.name << " = " << paramName(f.name) << ";\n";
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
    s << "using System.Collections.Generic;\n\n";
    s << "[TestClass]\n";
    s << "public class " << className << "{\n\n";

    // Collect all steps (background + per-scenario) into one method
    int objectCounter = 0;

    auto emitSteps = [&](const QVector<Step>& steps, const QString& glueVar) {
        for (const Step& step : steps) {
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable)
                continue; // bare step — no data object generated

            const AttrSet* as = findAttrSet(step.attrSetName, file);

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
                // For transposed tables the "Attribute/Value" header is consumed but not stored,
                // so rows always starts at 0. For normal tables, rows[0] is the header.
                int startRow = (tbl.hasHeader && !tbl.transposed) ? 1 : 0;

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

    for (const Scenario& sc : file.scenarios) {
        const QString meth = "Test_Scenario_" + toClassName(sc.name);
        const QString glueClass = className + "_glue";
        const QString glueVar   = glueClass[0].toLower() + glueClass.mid(1) + "_object";

        s << "[TestMethod]\n";
        s << "public void " << meth << "(){\n";
        s << "     " << glueClass << " " << glueVar << " = new " << glueClass << "();\n\n";

        // Background steps first
        emitSteps(file.backgroundSteps, glueVar);
        // Then scenario-specific steps
        emitSteps(sc.steps, glueVar);

        s << "}\n\n";
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
            if (step.attrSetName.isEmpty() && !step.hasTable) continue;
            const QString meth = toMethodName(step.keyword, step.text);
            if (seen.contains(meth)) continue;
            seen.insert(meth);
            if (!step.attrSetName.isEmpty())
                sigs.push_back({ meth, step.attrSetName + "String", true });
            else
                sigs.push_back({ meth, "List<string>", true });
        }
    };

    collectSteps(file.backgroundSteps);
    collectSteps(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios)
        collectSteps(sc.steps);

    return sigs;
}

QString CSharpGenerator::genStubMethod(const GlueSig& sig)
{
    const QString paramType = sig.isList
        ? QString("List<%1>").arg(sig.paramType)
        : sig.paramType;
    QString out;
    QTextStream s(&out);
    s << "        public void " << sig.method << "(" << paramType << " values)\n";
    s << "        {\n";
    s << "            Console.WriteLine(\"---  \" + \"" << sig.method << "\");\n";
    s << "            foreach (var value in values)\n";
    s << "            {\n";
    s << "                Console.WriteLine(value);\n";
    s << "                // TODO: implement\n";
    s << "            }\n";
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
    s << "    using static Microsoft.VisualStudio.TestTools.UnitTesting.Assert;\n\n";
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

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString className = toClassName(file.specName);
    const QString ns        = opts.nsPrefix + "." + className;

    QDir dir(opts.outputDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create output directory: %1").arg(opts.outputDir);
        return msgs;
    }

    // 1. String + Typed classes for each AttrSet
    for (const AttrSet& as : file.attrSets) {
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }

        const QString stringPath = dir.filePath(as.name + "String.cs");
        const QString typedPath  = dir.filePath(as.name + "Typed.cs");

        writeFile(stringPath, genStringClass(as, ns), msgs);
        writeFile(typedPath,  genTypedClass(as, ns),  msgs);
    }

    // 2. Unit test file (always overwritten)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(file, ns, className, testErrs);
        msgs << testErrs;
        writeFile(dir.filePath(className + "_Tests.cs"), testContent, msgs);
    }

    // 3. Glue file: write fresh if absent/overwrite; otherwise append any missing stubs
    {
        const QString gluePath = dir.filePath(className + "_glue.cs");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(file, ns, className), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(file);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    return msgs;
}
