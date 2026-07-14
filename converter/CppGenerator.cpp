#include "CppGenerator.h"
#include "TagFilter.h"

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
    if (t == "float"   || t == "decimal")                                return "double";
    if (t == "boolean" || t == "yesno" || t == "bool")                   return "bool";
    if (t == "string"  || t == "text"  || t == "character" || t == "char") return "std::string";
    if (t == "date"    || t == "time"  || t == "datetime"  || t == "duration") return "std::string";
    return specType.trimmed();
}

QString CppGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("!s.%1.empty() ? std::stoi(s.%1) : 0").arg(field);
    if (t == "float" || t == "decimal")
        return QString("!s.%1.empty() ? std::stod(s.%1) : 0.0").arg(field);
    if (t == "boolean" || t == "yesno" || t == "bool")
        return QString("(s.%1 == \"true\" || s.%1 == \"t\" || s.%1 == \"yes\" || s.%1 == \"y\" || s.%1 == \"1\")").arg(field);
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
        "Character", "String", "Text", "Integer", "Float", "Boolean",
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

QString CppGenerator::genStringHeader(const AttrSet& as) const
{
    const QString typeName = toTypeName(as.name) + "String";
    QString out;
    QTextStream s(&out);

    s << "#pragma once\n";
    s << "#include <string>\n";
    s << "#include <vector>\n";
    s << "#include <sstream>\n";
    for (const QString& inc : m_extraIncludes) s << inc << "\n";
    s << "\n";

    s << "struct " << typeName << " {\n";
    for (const Field& f : as.fields)
        s << "    std::string " << toIdentifier(f.name) << ";\n";
    s << "\n";

    // from_vec factory
    s << "    static " << typeName << " from_vec(const std::vector<std::string>& v) {\n";
    s << "        " << typeName << " obj;\n";
    for (int i = 0; i < as.fields.size(); ++i)
        s << "        if (v.size() > " << i << ") obj." << toIdentifier(as.fields[i].name)
          << " = v[" << i << "];\n";
    s << "        return obj;\n";
    s << "    }\n\n";

    // to_string
    s << "    std::string to_string() const {\n";
    s << "        std::ostringstream ss;\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << "        ss << \", \";\n";
        s << "        ss << \"" << as.fields[i].name << "=\" << " << toIdentifier(as.fields[i].name) << ";\n";
    }
    s << "        return ss.str();\n";
    s << "    }\n";
    s << "};\n";

    return out;
}

// ---------------------------------------------------------------------------
// Typed header generator
// ---------------------------------------------------------------------------

QString CppGenerator::genTypedHeader(const AttrSet& as) const
{
    const QString strName   = toTypeName(as.name) + "String";
    const QString typedName = toTypeName(as.name) + "Typed";
    const QString strFile   = toIdentifier(as.name) + "_string.h";
    QString out;
    QTextStream s(&out);

    s << "#pragma once\n";
    s << "#include <string>\n";
    s << "#include <vector>\n";
    s << "#include \"" << strFile << "\"\n";
    for (const QString& inc : m_extraIncludes) s << inc << "\n";
    s << "\n";

    s << "struct " << typedName << " {\n";
    for (const Field& f : as.fields) {
        const QString ct = cppType(f.type);
        if (f.multiples)
            s << "    std::vector<" << ct << "> " << toIdentifier(f.name) << ";\n";
        else
            s << "    " << ct << " " << toIdentifier(f.name);
        // default value
        if (!f.multiples) {
            const QString tl = f.type.trimmed().toLower();
            if (tl == "integer" || tl == "int")
                s << " = 0";
            else if (tl == "float" || tl == "decimal")
                s << " = 0.0";
            else if (tl == "boolean" || tl == "yesno" || tl == "bool")
                s << " = false";
            s << ";\n";
        }
    }
    s << "\n";

    // from_string_struct
    s << "    static " << typedName << " from_string_struct(const " << strName << "& s) {\n";
    s << "        " << typedName << " t;\n";
    for (const Field& f : as.fields) {
        const QString fid  = toIdentifier(f.name);
        const QString expr = parseExpr(fid, f.type);
        if (f.multiples)
            s << "        t." << fid << " = {" << expr << "};\n";
        else
            s << "        t." << fid << " = " << expr << ";\n";
    }
    s << "        return t;\n";
    s << "    }\n";
    s << "};\n";

    return out;
}

// ---------------------------------------------------------------------------
// Common aggregate header
// ---------------------------------------------------------------------------

QString CppGenerator::genCommonHeader(const QVector<AttrSet>& attrSets) const
{
    QString out;
    QTextStream s(&out);
    s << "#pragma once\n";
    for (const AttrSet& as : attrSets) {
        const QString id = toIdentifier(as.name);
        s << "#include \"" << id << "_string.h\"\n";
        s << "#include \"" << id << "_typed.h\"\n";
    }
    return out;
}

// ---------------------------------------------------------------------------
// Test file
// ---------------------------------------------------------------------------

QString CppGenerator::genTestFile(const SpectableFile& file, const QString& specSnake,
                                   const QString& glueClass, QStringList& errors) const
{
    QString out;
    QTextStream s(&out);

    s << "#include <gtest/gtest.h>\n";
    s << "#include <iostream>\n";
    s << "#include \"common/common.h\"\n";
    s << "#include \"" << specSnake << "_glue.h\"\n";
    for (const QString& inc : m_extraIncludes) s << inc << "\n";
    s << "\n";

    int objectCounter = 0;
    const QString specClass = toTypeName(file.specName);

    auto emitStrVec = [&](const QString& listType, const QVector<QStringList>& rows) {
        s << "    std::vector<" << listType << "> objectList" << (++objectCounter) << " = {\n";
        for (const QStringList& row : rows) {
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
                emitStrVec(listType, rows);
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
            if (nb.hasExamples && nb.kind == kind
                && !seenBlocks.contains(kind + ":" + nb.name.toLower()))
                { hasKind = true; break; }
        if (!hasKind) continue;

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind) continue;
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
                emitStrVec(listType, rows);
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
        if (!nb.hasExamples) continue;
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

    QString stubs;
    for (const GlueSig& sig : sigs) {
        if (!content.contains(QString("void %1(").arg(sig.method)))
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

QString CppGenerator::genGlueFile(const SpectableFile& file, const QString& glueClass) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    QString out;
    QTextStream s(&out);

    s << "#pragma once\n";
    s << "#include <gtest/gtest.h>\n";
    s << "#include <iostream>\n";
    s << "#include <string>\n";
    s << "#include <vector>\n";
    s << "#include \"common/common.h\"\n";
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
        else if (tl == "float"   || tl == "decimal")                                ctype = "double";
        else if (tl == "boolean" || tl == "yesno" || tl == "bool")                  ctype = "bool";
        else if (tl == "string"  || tl == "text"  || tl == "character" || tl == "char"
              || tl == "date"    || tl == "time"  || tl == "datetime"  || tl == "duration")
            ctype = "std::string";
        else
            ctype = f.type.trimmed();

        s << "    " << ctype << " " << f.name;
        if (!f.defaultValue.isEmpty()) {
            if (tl == "integer" || tl == "int" || tl == "float" || tl == "decimal")
                s << " = " << f.defaultValue;
            else if (tl == "boolean" || tl == "yesno" || tl == "bool")
                s << " = " << (f.defaultValue.toLower() == "true" || f.defaultValue == "1" || f.defaultValue.toLower() == "yes" ? "true" : "false");
            else
                s << " = \"" << f.defaultValue << "\"";
        } else {
            if (tl == "integer" || tl == "int") s << " = 0";
            else if (tl == "float" || tl == "decimal") s << " = 0.0";
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
        else if (tl == "float"   || tl == "decimal") ctype = "double";
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

    // Copy source .spectable
    if (opts.copySpectable && !file.filePath.isEmpty()) {
        const QString dest = outDir.filePath(QFileInfo(file.filePath).fileName());
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
        writeFile(commonDir.filePath(id + "_string.h"), genStringHeader(as), msgs);
        writeFile(commonDir.filePath(id + "_typed.h"),  genTypedHeader(as),  msgs);
        domainSets.push_back(as);
    }
    writeFile(commonDir.filePath("common.h"), genCommonHeader(domainSets), msgs);

    // Test file (always overwritten)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specSnake, glueClass, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(outDir.filePath("test_" + specSnake + ".cpp"), testContent, msgs);
    }

    // Glue file
    {
        const QString gluePath = outDir.filePath(specSnake + "_glue.h");
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
            // Entities
            for (const AttrSet& as : file.attrSets) {
                if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
                const QString prodPath = prodDir.filePath(as.name + ".h");
                if (!QFile::exists(prodPath))
                    writeFile(prodPath, genProductionEntityCpp(as), msgs);
            }
            // Collections
            for (const Collection& col : file.collections) {
                if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
                const QString prodPath = prodDir.filePath(col.name + ".h");
                if (!QFile::exists(prodPath))
                    writeFile(prodPath, genProductionCollectionCpp(col), msgs);
            }
        }
    }

    return msgs;
}
