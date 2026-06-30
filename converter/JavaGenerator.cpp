#include "JavaGenerator.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>
#include <algorithm>

static QString resolveCell(const QString& cell)
{
    QString s = cell;
    return s.replace('~', ' ');
}

// ---------------------------------------------------------------------------
// Type mapping
// ---------------------------------------------------------------------------

QString JavaGenerator::javaType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")          return "int";
    if (t == "float"   || t == "decimal")      return "double";
    if (t == "boolean" || t == "yesno"
     || t == "bool")                           return "boolean";
    if (t == "date")                           return "LocalDate";
    if (t == "time")                           return "LocalTime";
    if (t == "datetime")                       return "LocalDateTime";
    if (t == "duration")                       return "Duration";
    if (t == "string" || t == "text"
     || t == "character" || t == "char")       return "String";
    return specType.trimmed();
}

static QString javaBoxedType(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")          return "Integer";
    if (t == "float"   || t == "decimal")      return "Double";
    if (t == "boolean" || t == "yesno"
     || t == "bool")                           return "Boolean";
    if (t == "date")                           return "LocalDate";
    if (t == "time")                           return "LocalTime";
    if (t == "datetime")                       return "LocalDateTime";
    if (t == "duration")                       return "Duration";
    if (t == "string" || t == "text"
     || t == "character" || t == "char")       return "String";
    return specType.trimmed();
}

QString JavaGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")     return QString("Integer.parseInt(this.%1)").arg(field);
    if (t == "float"   || t == "decimal") return QString("Double.parseDouble(this.%1)").arg(field);
    if (t == "boolean" || t == "yesno"
     || t == "bool")                      return QString(
        "(this.%1.equalsIgnoreCase(\"true\") || this.%1.equalsIgnoreCase(\"t\") "
        "|| this.%1.equalsIgnoreCase(\"yes\") || this.%1.equalsIgnoreCase(\"y\") "
        "|| this.%1.equals(\"1\"))").arg(field);
    if (t == "date")                      return QString("LocalDate.parse(this.%1)").arg(field);
    if (t == "time")                      return QString("LocalTime.parse(this.%1)").arg(field);
    if (t == "datetime")                  return QString("LocalDateTime.parse(this.%1)").arg(field);
    if (t == "duration")                  return QString("Duration.parse(this.%1)").arg(field);
    if (t == "string" || t == "text"
     || t == "character" || t == "char")  return QString("this.%1").arg(field);
    return QString("new %1(this.%2)").arg(specType.trimmed()).arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString JavaGenerator::toClassName(const QString& name)
{
    QString s = name;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s = s.remove(QRegularExpression("^_+|_+$"));
    if (!s.isEmpty() && s[0].isDigit()) s.prepend("_");
    return s;
}

QString JavaGenerator::toMethodName(const QString& keyword, const QString& stepText)
{
    QString s = keyword + "_" + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s = s.remove(QRegularExpression("^_+|_+$"));
    return s;
}

QString JavaGenerator::toCamelCase(const QString& fieldName)
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
// AttrSet / Define lookup (identical logic to CSharpGenerator)
// ---------------------------------------------------------------------------

const AttrSet* JavaGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0)
            return &as;
    return nullptr;
}

const Define* JavaGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0)
            return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Table resolution (same logic as CSharpGenerator)
// ---------------------------------------------------------------------------

QVector<QStringList> JavaGenerator::resolveStepRows(
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

        const bool useKV = def->transposed || step.transposed;
        if (useKV) {
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

// ---------------------------------------------------------------------------
// String class
// ---------------------------------------------------------------------------

QString JavaGenerator::genStringClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports) const
{
    const QString cn = as.name + "String";
    const QString tn = as.name + "Typed";
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << ";\n\n";

    // Check if we need time imports
    bool needsDate = false, needsTime = false, needsDt = false, needsDur = false;
    bool needsCollections = false;
    for (const Field& f : as.fields) {
        const QString t = f.type.toLower();
        if (t == "date")     needsDate = true;
        if (t == "time")     needsTime = true;
        if (t == "datetime") needsDt   = true;
        if (t == "duration") needsDur  = true;
        if (f.multiples)     needsCollections = true;
    }
    if (needsDate) s << "import java.time.LocalDate;\n";
    if (needsTime) s << "import java.time.LocalTime;\n";
    if (needsDt)   s << "import java.time.LocalDateTime;\n";
    if (needsDur)  s << "import java.time.Duration;\n";
    if (needsCollections) s << "import java.util.Collections;\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    if (needsDate || needsTime || needsDt || needsDur || needsCollections || !extraImports.isEmpty())
        s << "\n";

    s << "public class " << cn << " {\n";

    for (const Field& f : as.fields)
        s << "    public String " << toCamelCase(f.name) << ";\n";
    s << "\n";

    // Constructor
    s << "    public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << "String " << toCamelCase(as.fields[i].name);
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "        this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "    }\n\n";

    // toTyped()
    s << "    public " << tn << " to" << tn << "() {\n";
    s << "        return new " << tn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f = as.fields[i];
        const QString expr = parseExpr(toCamelCase(f.name), f.type);
        if (f.multiples)
            s << "            Collections.singletonList(" << expr << ")";
        else
            s << "            " << expr;
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "        );\n    }\n\n";

    // toString() — label uses original name, value reference uses camelCase identifier
    s << "    @Override\n    public String toString() {\n        return ";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << " + \", \" + ";
        s << "\"" << as.fields[i].name << "=\" + " << toCamelCase(as.fields[i].name);
    }
    s << ";\n    }\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed class
// ---------------------------------------------------------------------------

QString JavaGenerator::genTypedClass(const AttrSet& as, const QString& pkg, const QStringList& extraImports) const
{
    const QString cn = as.name + "Typed";
    QString out;
    QTextStream s(&out);

    s << "package " << pkg << ";\n\n";

    bool needsDate = false, needsTime = false, needsDt = false, needsDur = false;
    bool needsList = false;
    for (const Field& f : as.fields) {
        const QString t = f.type.toLower();
        if (t == "date")     needsDate = true;
        if (t == "time")     needsTime = true;
        if (t == "datetime") needsDt   = true;
        if (t == "duration") needsDur  = true;
        if (f.multiples)     needsList = true;
    }
    if (needsDate) s << "import java.time.LocalDate;\n";
    if (needsTime) s << "import java.time.LocalTime;\n";
    if (needsDt)   s << "import java.time.LocalDateTime;\n";
    if (needsDur)  s << "import java.time.Duration;\n";
    if (needsList) s << "import java.util.List;\n";
    for (const QString& imp : extraImports) s << imp << "\n";
    if (needsDate || needsTime || needsDt || needsDur || needsList || !extraImports.isEmpty())
        s << "\n";

    s << "public class " << cn << " {\n";

    for (const Field& f : as.fields) {
        if (f.multiples)
            s << "    public List<" << javaBoxedType(f.type) << "> " << toCamelCase(f.name) << ";\n";
        else
            s << "    public " << javaType(f.type) << " " << toCamelCase(f.name) << ";\n";
    }
    s << "\n";

    s << "    public " << cn << "(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        if (f.multiples)
            s << "List<" << javaBoxedType(f.type) << "> " << toCamelCase(f.name);
        else
            s << javaType(f.type) << " " << toCamelCase(f.name);
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "        this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "    }\n}\n";

    return out;
}

// ---------------------------------------------------------------------------
// Test file
// ---------------------------------------------------------------------------

QString JavaGenerator::genTestFile(const SpectableFile& file, const QString& testPkg,
                                    const QString& specPkg, const QString& domainPkg,
                                    const QString& className, QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    s << "package " << testPkg << ";\n\n";
    s << "import java.util.List;\n";
    s << "import java.util.ArrayList;\n";
    s << "import " << domainPkg << ".*;\n";
    s << "import " << specPkg << "." << className << "_glue;\n";
    for (const QString& imp : file.configJava) s << imp << "\n";

    const bool isJUnit5 = m_framework.compare("TestNG", Qt::CaseInsensitive) != 0
                       && (m_framework.compare("JUnit", Qt::CaseInsensitive) == 0
                           || m_framework.toLower().startsWith("junit5")
                           || m_framework.toLower() == "junit 5");
    const bool isTestNG = m_framework.compare("TestNG", Qt::CaseInsensitive) == 0;

    if (isTestNG) {
        s << "import org.testng.annotations.Test;\n";
    } else if (isJUnit5) {
        s << "import org.junit.jupiter.api.Test;\n";
        // Emit Tag import only if any block has tags
        bool hasTags = false;
        for (const Scenario& sc : file.scenarios) if (!sc.tags.isEmpty()) { hasTags = true; break; }
        if (!hasTags)
            for (const NamedBlock& nb : file.namedBlocks) if (!nb.tags.isEmpty()) { hasTags = true; break; }
        if (hasTags) s << "import org.junit.jupiter.api.Tag;\n";
    } else {
        // JUnit 4
        s << "import org.junit.Test;\n";
    }

    // Helper: emit tag annotations before @Test
    auto emitTags = [&](const QStringList& tags) {
        if (isJUnit5)
            for (const QString& t : tags) s << "    @Tag(\"" << t << "\")\n";
        else if (!tags.isEmpty())
            s << "    // Tags: " << tags.join(", ") << "\n";
    };

    s << "\npublic class Test_" << className << " {\n\n";

    int objectCounter = 0;

    auto emitSteps = [&](const QVector<Step>& steps, const QString& glueVar) {
        for (const Step& step : steps) {
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                // Bare step — call with no arguments
                const QString meth = toMethodName(step.keyword, step.text);
                s << "        " << glueVar << "." << meth << "();\n\n";
                continue;
            }

            const AttrSet* as = findAttrSet(step.attrSetName, file);

            if (!step.attrSetName.isEmpty() && as == nullptr) {
                if (!isDataType(step.attrSetName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined — add an 'Attributes %2' block")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
                // DataType grid step — fall through to the List<List<String>> branch below
            }

            if (!step.attrSetName.isEmpty() && as) {
                ++objectCounter;
                const QString listType = step.attrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);

                QStringList localErrs;
                QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;

                s << "        List<" << listType << "> " << listVar
                  << " = new ArrayList<>();\n";
                for (const QStringList& row : rows) {
                    s << "        " << listVar << ".add(new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << row[ci] << "\"";
                    }
                    s << "));\n";
                }
                const QString meth = toMethodName(step.keyword, step.text);
                s << "        " << glueVar << "." << meth << "(" << listVar << ");\n\n";

            } else if (step.hasTable && as == nullptr) {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const StepTable& tbl = step.table;
                int startRow = (tbl.hasHeader && !tbl.transposed) ? 1 : 0;

                s << "        List<List<String>> " << listVar
                  << " = new ArrayList<>();\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "        " << listVar << ".add(List.of(";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << resolveCell(r[ci]) << "\"";
                    }
                    s << "));\n";
                }
                const QString meth = toMethodName(step.keyword, step.text);
                s << "        " << glueVar << "." << meth << "(" << listVar << ");\n\n";
            }
        }
    };

    // ── Scenario tests ──────────────────────────────────────────────────────
    if (!file.scenarios.isEmpty()) {
        s << "    // -------------------------\n";
        s << "    // Scenario Tests\n";
        s << "    // -------------------------\n";
    }
    for (const Scenario& sc : file.scenarios) {
        const QString meth      = "Scenario_" + toClassName(sc.name);
        const QString glueClass = className + "_glue";

        emitTags(sc.tags);
        s << "    @Test\n";
        s << "    public void " << meth << "() {\n";
        s << "        " << glueClass << " glue = new " << glueClass << "();\n\n";

        emitSteps(file.backgroundSteps, "glue");
        emitSteps(sc.steps, "glue");

        s << "    }\n\n";
    }

    // ── BusinessRule / Calculation / DataType tests ──────────────────────────
    static const QStringList namedKinds = { "BusinessRule", "Calculation", "DataType" };
    for (const QString& kind : namedKinds) {
        bool hasKind = false;
        for (const NamedBlock& nb : file.namedBlocks)
            if (nb.hasExamples && nb.kind == kind) { hasKind = true; break; }
        if (!hasKind) continue;

        s << "    // -------------------------\n";
        s << "    // " << kind << " Tests\n";
        s << "    // -------------------------\n";

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind) continue;

            const QString meth      = kind + "_" + toClassName(nb.name);
            const QString glueMeth  = "Examples" + kind + "_" + toClassName(nb.name);
            const QString glueClass = className + "_glue";
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            emitTags(nb.tags);
            s << "    @Test\n";
            s << "    public void " << meth << "() {\n";
            s << "        " << glueClass << " glue = new " << glueClass << "();\n";

            if (as) {
                ++objectCounter;
                const QString listType = nb.examples.attrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                s << "        List<" << listType << "> " << listVar
                  << " = new ArrayList<>();\n";
                for (const QStringList& row : rows) {
                    s << "        " << listVar << ".add(new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << row[ci] << "\"";
                    }
                    s << "));\n";
                }
                s << "        glue." << glueMeth << "(" << listVar << ");\n";
            } else {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "        List<List<String>> " << listVar << " = new ArrayList<>();\n";
                for (const QStringList& row : rows) {
                    s << "        " << listVar << ".add(List.of(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << resolveCell(row[ci]) << "\"";
                    }
                    s << "));\n";
                }
                s << "        glue." << glueMeth << "(" << listVar << ");\n";
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

QVector<JavaGenerator::GlueSig> JavaGenerator::collectGlueSigs(const SpectableFile& file)
{
    QVector<GlueSig> sigs;
    QSet<QString> seen;

    auto collectSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            const QString meth = toMethodName(step.keyword, step.text);
            if (seen.contains(meth)) continue;
            seen.insert(meth);
            if (step.attrSetName.isEmpty() && !step.hasTable)
                sigs.push_back({ meth, "" });                  // void / no parameter
            else if (!step.attrSetName.isEmpty() && !isDataType(step.attrSetName, file))
                sigs.push_back({ meth, step.attrSetName + "String" });
            else if (!step.attrSetName.isEmpty() && isDataType(step.attrSetName, file))
                sigs.push_back({ meth, "List<List<String>>" });  // grid
            else
                sigs.push_back({ meth, "List<String>" });
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
            sigs.push_back({ meth, nb.examples.attrSetName + "String" });
        else
            sigs.push_back({ meth, "List<List<String>>" });
    }

    return sigs;
}

QString JavaGenerator::genStubMethod(const GlueSig& sig)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "    public void " << sig.method << "() {\n";
        s << "        fail(\"Not implemented: " << sig.method << "\");\n";
        s << "    }\n";
        return out;
    }
    const QString paramType = sig.paramType.contains('<')
        ? sig.paramType
        : QString("List<%1>").arg(sig.paramType);
    const QString iterType = sig.paramType.contains('<') ? "List<String>" : sig.paramType;
    s << "    public void " << sig.method << "(" << paramType << " values) {\n";
    s << "        for (" << iterType << " value : values) {\n";
    s << "            System.out.println(value);\n";
    s << "        }\n";
    s << "        fail(\"Not implemented: " << sig.method << "\");\n";
    s << "    }\n";
    return out;
}

bool JavaGenerator::appendMissingStubs(const QString& gluePath,
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

    // Insert before the final closing "}\n" of the class
    const int closingClass = content.lastIndexOf("\n}");
    if (closingClass < 0) {
        msgs << QString("WARNING:0:Could not locate class closing brace in %1 — stubs not added")
                .arg(gluePath);
        return false;
    }

    content.insert(closingClass, "\n" + stubs);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

QString JavaGenerator::genGlueFile(const SpectableFile& file, const QString& specPkg,
                                    const QString& domainPkg, const QString& className) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    const QString glueClass = className + "_glue";
    QString out;
    QTextStream s(&out);

    s << "package " << specPkg << ";\n\n";
    s << "import java.util.List;\n";
    s << "import " << domainPkg << ".*;\n";
    for (const QString& imp : file.configJava) s << imp << "\n";
    const bool glueJUnit5 = m_framework.compare("TestNG", Qt::CaseInsensitive) != 0
                         && (m_framework.compare("JUnit", Qt::CaseInsensitive) == 0
                             || m_framework.toLower().startsWith("junit5")
                             || m_framework.toLower() == "junit 5");
    if (m_framework.compare("TestNG", Qt::CaseInsensitive) == 0)
        s << "import static org.testng.Assert.fail;\n\n";
    else if (glueJUnit5)
        s << "import static org.junit.jupiter.api.Assertions.fail;\n\n";
    else
        s << "import static org.junit.Assert.fail;\n\n";
    s << "public class " << glueClass << " {\n";
    s << "    private static final String DNCString = \"?DNC?\";\n\n";

    for (const GlueSig& sig : sigs)
        s << genStubMethod(sig) << "\n";

    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool JavaGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList JavaGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_framework = opts.framework;

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString className   = toClassName(file.specName);
    const QString domainPkg   = opts.packagePrefix + ".common";

    QString specPkg;
    QString specSubDir;
    if (!opts.sourceRoot.isEmpty() && !file.filePath.isEmpty()) {
        const QDir    srcDir(QFileInfo(opts.sourceRoot).absoluteFilePath());
        const QString fileAbsDir = QFileInfo(file.filePath).absoluteDir().absolutePath();
        QString relPath = srcDir.relativeFilePath(fileAbsDir);
        if (relPath == "." || relPath.isEmpty()) {
            specPkg = opts.packagePrefix;
        } else {
            QStringList parts;
            for (const QString& p : relPath.split('/'))
                if (!p.isEmpty() && p != "..") parts << p.toLower().replace('-', '_');
            specPkg = parts.isEmpty() ? opts.packagePrefix
                                      : opts.packagePrefix + "." + parts.join('.');
            specSubDir = parts.join('/');
        }
    } else {
        specPkg = opts.packagePrefix + ".specifications." + className.toLower();
    }
    const QString testPkg = specPkg + ".tests";

    QDir dir(specSubDir.isEmpty() ? opts.outputDir : opts.outputDir + "/" + specSubDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create output directory: %1").arg(dir.path());
        return msgs;
    }

    // Domain classes go into outputDir/common/
    QDir domainDir(opts.outputDir + "/common");
    if (!domainDir.exists() && !domainDir.mkpath(".")) {
        msgs << QString("ERROR:0:Cannot create common directory: %1").arg(domainDir.path());
        return msgs;
    }

    // Copy the source .spectable file into the output folder
    if (!file.filePath.isEmpty()) {
        const QString destPath = dir.filePath(QFileInfo(file.filePath).fileName());
        QFile::remove(destPath);
        if (!QFile::copy(file.filePath, destPath))
            msgs << QString("WARNING:0:Could not copy %1 to %2").arg(file.filePath, destPath);
    }

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

    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        writeFile(domainDir.filePath(as.name + "String.java"), genStringClass(as, domainPkg, file.configJava), msgs);
        writeFile(domainDir.filePath(as.name + "Typed.java"),  genTypedClass(as, domainPkg, file.configJava),  msgs);
    }

    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, testPkg, specPkg, domainPkg, className, testErrs);
        msgs << testErrs;
        const bool testHasErrors = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!testHasErrors)
            writeFile(dir.filePath("Test_" + className + ".java"), testContent, msgs);
    }

    {
        const QString gluePath = dir.filePath(className + "_glue.java");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, specPkg, domainPkg, className), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    return msgs;
}
