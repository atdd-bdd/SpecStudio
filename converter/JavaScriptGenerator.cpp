#include "JavaScriptGenerator.h"
#include "TagFilter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

// ---------------------------------------------------------------------------
// Cell value helpers
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

static QString jsEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('`',  "\\`");
    r.replace('$',  "\\$");
    return r;
}

static QString jsStringEscape(const QString& s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('"',  "\\\"");
    return r;
}

// ---------------------------------------------------------------------------
// Type helpers
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::jsDefaultValue(const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")        return "0";
    if (t == "float"   || t == "decimal" || t == "scientific")    return "0.0";
    if (t == "boolean" || t == "yesno"
     || t == "bool")                         return "false";
    // strings, date/time, user-defined
    return "\"\"";
}

QString JavaScriptGenerator::parseExpr(const QString& field, const QString& specType)
{
    const QString t = specType.trimmed().toLower();
    if (t == "integer" || t == "int")
        return QString("s.%1 !== \"\" ? Number(s.%1) : 0").arg(field);
    if (t == "float" || t == "decimal" || t == "scientific")
        return QString("s.%1 !== \"\" ? Number(s.%1) : 0.0").arg(field);
    if (t == "boolean" || t == "yesno" || t == "bool")
        return QString("[\"true\",\"t\",\"yes\",\"y\",\"1\"].includes(s.%1.toLowerCase())").arg(field);
    if (t == "string" || t == "text" || t == "character" || t == "char"
     || t == "date"   || t == "time" || t == "datetime"  || t == "duration")
        return QString("s.%1").arg(field);
    // User-defined type
    return QString("new %1(s.%2)").arg(specType.trimmed()).arg(field);
}

// ---------------------------------------------------------------------------
// Identifier helpers
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::toCamelCase(const QString& name)
{
    const QStringList parts = name.split(QRegularExpression(R"([\s_]+)"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return name;
    QString result = parts[0][0].toLower() + parts[0].mid(1);
    for (int i = 1; i < parts.size(); ++i)
        if (!parts[i].isEmpty())
            result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

QString JavaScriptGenerator::toPascalCase(const QString& name)
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

QString JavaScriptGenerator::toMethodName(const QString& keyword, const QString& stepText)
{
    // "Given" + "check account balance" → "givenCheckAccountBalance"
    QString combined = keyword + "_" + stepText;
    combined.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    combined.remove(QRegularExpression("^_+|_+$"));
    const QStringList parts = combined.split('_', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return combined.toLower();
    QString result = parts[0].toLower();
    for (int i = 1; i < parts.size(); ++i)
        if (!parts[i].isEmpty())
            result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

QString JavaScriptGenerator::toFileName(const QString& name)
{
    // "MySpecName" → "mySpecName"
    if (name.isEmpty()) return name;
    return name[0].toLower() + name.mid(1);
}

// ---------------------------------------------------------------------------
// Collection helpers
// ---------------------------------------------------------------------------

bool JavaScriptGenerator::isCollectionType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

QString JavaScriptGenerator::collectionElementType(const QString& name, const SpectableFile& file)
{
    for (const Collection& c : file.collections)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return c.elementType;
    return {};
}

// ---------------------------------------------------------------------------
// DataType detection
// ---------------------------------------------------------------------------

bool JavaScriptGenerator::isDataType(const QString& name, const SpectableFile& file)
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

const AttrSet* JavaScriptGenerator::findAttrSet(const QString& name, const SpectableFile& file)
{
    for (const AttrSet& as : file.attrSets)
        if (as.name.compare(name, Qt::CaseInsensitive) == 0) return &as;
    return nullptr;
}

const Define* JavaScriptGenerator::findDefine(const QString& name, const SpectableFile& file)
{
    for (const Define& d : file.defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Row resolution
// ---------------------------------------------------------------------------

QVector<QStringList> JavaScriptGenerator::resolveStepRows(
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

QVector<QStringList> JavaScriptGenerator::resolveExamplesRows(
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
// String class generator
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genStringClass(const AttrSet& as) const
{
    const QString cn = as.name + "String";
    QString out;
    QTextStream s(&out);

    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";
    s << "export class " << cn << " {\n";
    s << "  constructor(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        s << toCamelCase(as.fields[i].name) << " = \"\"";
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "    this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "  }\n\n";

    s << "  static fromList(values) {\n";
    s << "    const v = Array.from(values);\n";
    s << "    return new " << cn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        s << "      v[" << i << "] ?? \"\"";
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "    );\n  }\n\n";

    s << "  toString() {\n";
    s << "    return `";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const QString fn = toCamelCase(as.fields[i].name);
        s << as.fields[i].name << "=${this." << fn << "}";
    }
    s << "`;\n  }\n";
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Typed class generator
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genTypedClass(const AttrSet& as) const
{
    const QString cn   = as.name + "Typed";
    const QString scn  = as.name + "String";
    const QString sFile = as.name + "String.js";
    QString out;
    QTextStream s(&out);

    s << "import { " << scn << " } from \"./" << sFile << "\";\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";
    s << "export class " << cn << " {\n";
    s << "  constructor(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        const QString fn = toCamelCase(f.name);
        s << fn << " = " << jsDefaultValue(f.type);
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "    this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "  }\n\n";

    s << "  static fromStringObj(s) {\n";
    s << "    return new " << cn << "(\n";
    for (int i = 0; i < as.fields.size(); ++i) {
        const Field& f  = as.fields[i];
        const QString fn = toCamelCase(f.name);
        s << "      " << parseExpr(fn, f.type);
        if (i < as.fields.size() - 1) s << ",";
        s << "\n";
    }
    s << "    );\n  }\n";
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Common index.js
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genCommonIndex(const QVector<AttrSet>& attrSets) const
{
    QString out;
    QTextStream s(&out);
    for (const AttrSet& as : attrSets) {
        s << "export * from \"./" << as.name << "String.js\";\n";
        s << "export * from \"./" << as.name << "Typed.js\";\n";
    }
    return out;
}

// ---------------------------------------------------------------------------
// Test file generator
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genTestFile(const SpectableFile& file, const QString& specName,
                                          const QString& glueClass, const QString& commonRelPath,
                                          QStringList& errors) const
{
    const QString glueFile = toFileName(specName) + "_glue.js";
    QString out;
    QTextStream s(&out);

    s << "import { ";
    // collect all String types used
    QSet<QString> usedTypes;
    auto collectUsedTypes = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            if (step.attrSetName.isEmpty()) continue;
            const QString effectiveName = isCollectionType(step.attrSetName, file)
                ? collectionElementType(step.attrSetName, file)
                : step.attrSetName;
            if (!effectiveName.isEmpty() && !isDataType(effectiveName, file))
                usedTypes.insert(effectiveName + "String");
        }
    };
    collectUsedTypes(file.backgroundSteps);
    collectUsedTypes(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios) collectUsedTypes(sc.steps);
    for (const NamedBlock& nb : file.namedBlocks) {
        if (!nb.examples.attrSetName.isEmpty() && !isDataType(nb.examples.attrSetName, file))
            usedTypes.insert(nb.examples.attrSetName + "String");
    }

    QStringList typeList = usedTypes.values();
    std::sort(typeList.begin(), typeList.end());
    s << typeList.join(", ");
    s << " } from \"" << commonRelPath << "/index.js\";\n";
    s << "import { " << glueClass << " } from \"./" << glueFile << "\";\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";

    int objectCounter = 0;

    auto emitSteps = [&](const QVector<Step>& steps) {
        for (const Step& step : steps) {
            const QString meth = toMethodName(step.keyword, step.text);

            if (step.hasDocString) {
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
                    QString esc = def->docString;
                    esc.replace("\\", "\\\\");
                    esc.replace("\"", "\\\"");
                    esc.replace("\n", "\\n");
                    s << "    glue." << meth << "(\"" << esc << "\");\n";
                    continue;
                }
            }
            if (step.attrSetName.isEmpty() && step.defineRef.isEmpty() && !step.hasTable) {
                s << "    glue." << meth << "();\n";
                continue;
            }

            const QString effectiveAttrSetName = (!step.attrSetName.isEmpty() && isCollectionType(step.attrSetName, file))
                ? collectionElementType(step.attrSetName, file)
                : step.attrSetName;
            const AttrSet* as = findAttrSet(effectiveAttrSetName, file);

            if (!step.attrSetName.isEmpty() && as == nullptr) {
                if (!isDataType(effectiveAttrSetName, file)) {
                    errors << QString("ERROR:%1:AttributeSet '%2' not defined")
                              .arg(step.line).arg(step.attrSetName);
                    continue;
                }
            }

            if (!step.attrSetName.isEmpty() && as) {
                ++objectCounter;
                const QString listType = effectiveAttrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                QStringList localErrs;
                QVector<QStringList> rows = resolveStepRows(step, as, file, localErrs);
                errors << localErrs;

                s << "    const " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "      new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << jsStringEscape(row[ci]) << "\"";
                    }
                    s << "),\n";
                }
                s << "    ];\n";
                s << "    glue." << meth << "(" << listVar << ");\n";

            } else if (step.hasTable && as == nullptr) {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const StepTable& tbl  = step.table;
                const bool isTypedGrid = !step.attrSetName.isEmpty()
                                      && isDataType(step.attrSetName, file);
                const int startRow = (!isTypedGrid && tbl.hasHeader && !tbl.vertical) ? 1 : 0;

                s << "    const " << listVar << " = [\n";
                for (int ri = startRow; ri < tbl.rows.size(); ++ri) {
                    s << "      [";
                    const QStringList& r = tbl.rows[ri];
                    for (int ci = 0; ci < r.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << jsStringEscape(resolveValue(r[ci], file)) << "\"";
                    }
                    s << "],\n";
                }
                s << "    ];\n";
                s << "    glue." << meth << "(" << listVar << ");\n";
            }
        }
    };

    s << "describe(\"" << jsStringEscape(specName) << "\", () => {\n\n";

    for (const Scenario& sc : file.scenarios) {
        const QStringList effectiveGenTags = file.generatorTags + sc.generatorTags;
        if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;

        const QStringList allTags = file.tags + sc.tags;
        for (const QString& tag : allTags) s << "  // @tag: " << tag << "\n";

        s << "  test(\"Scenario " << jsStringEscape(sc.name) << "\", () => {\n";
        s << "    const glue = new " << glueClass << "();\n";
        emitSteps(file.backgroundSteps);
        emitSteps(sc.steps);
        s << "  });\n\n";
    }

    // ── BusinessRule / Calculation / DataType tests ──────────────────────────
    static const QStringList namedKinds = { "BusinessRule", "Calculation", "DataType" };
    QSet<QString> seenNamedBlocks;
    for (const QString& kind : namedKinds) {
        bool hasKind = false;
        for (const NamedBlock& nb : file.namedBlocks)
            if (nb.hasExamples && nb.kind == kind
                && !seenNamedBlocks.contains(kind + ":" + nb.name.toLower()))
                { hasKind = true; break; }
        if (!hasKind) continue;

        for (const NamedBlock& nb : file.namedBlocks) {
            if (!nb.hasExamples || nb.kind != kind) continue;
            const QString blockKey = kind + ":" + nb.name.toLower();
            if (seenNamedBlocks.contains(blockKey)) {
                errors << QString("WARNING:%1:%2 '%3' declared in multiple files — only first is tested")
                              .arg(nb.line).arg(kind).arg(nb.name);
                continue;
            }
            seenNamedBlocks.insert(blockKey);
            const QStringList effectiveGenTags = file.generatorTags + nb.generatorTags;
            if (!TagFilter::matches(m_tagFilter, effectiveGenTags)) continue;

            const QString glueMeth = toMethodName("Examples_" + kind, nb.name);
            const AttrSet* as = nb.examples.attrSetName.isEmpty()
                ? nullptr
                : findAttrSet(nb.examples.attrSetName, file);

            for (const QString& tag : nb.tags) s << "  // @tag: " << tag << "\n";
            s << "  test(\"" << kind << " " << jsStringEscape(nb.name) << "\", () => {\n";
            s << "    const glue = new " << glueClass << "();\n";

            if (as) {
                ++objectCounter;
                const QString listType = nb.examples.attrSetName + "String";
                const QString listVar  = QString("objectList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, as);
                s << "    const " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "      new " << listType << "(";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << jsStringEscape(row[ci]) << "\"";
                    }
                    s << "),\n";
                }
                s << "    ];\n";
                s << "    glue." << glueMeth << "(" << listVar << ");\n";
            } else {
                ++objectCounter;
                const QString listVar = QString("stringListList%1").arg(objectCounter);
                const QVector<QStringList> rows = resolveExamplesRows(nb, nullptr);
                s << "    const " << listVar << " = [\n";
                for (const QStringList& row : rows) {
                    s << "      [";
                    for (int ci = 0; ci < row.size(); ++ci) {
                        if (ci) s << ", ";
                        s << "\"" << jsStringEscape(resolveValue(row.size() > ci ? row[ci] : QString(), file)) << "\"";
                    }
                    s << "],\n";
                }
                s << "    ];\n";
                s << "    glue." << glueMeth << "(" << listVar << ");\n";
            }
            s << "  });\n\n";
        }
    }

    s << "});\n";
    return out;
}

// ---------------------------------------------------------------------------
// Glue file generator
// ---------------------------------------------------------------------------

QVector<JavaScriptGenerator::GlueSig> JavaScriptGenerator::collectGlueSigs(const SpectableFile& file)
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
            } else if (!step.attrSetName.isEmpty() && isDataType(step.attrSetName, file)) {
                sigs.push_back({ meth, "grid" });
            } else {
                sigs.push_back({ meth, "list" });
            }
        }
    };

    collectSteps(file.backgroundSteps);
    collectSteps(file.cleanupSteps);
    for (const Scenario& sc : file.scenarios) collectSteps(sc.steps);

    for (const NamedBlock& nb : file.namedBlocks) {
        if (!nb.hasExamples) continue;
        const QString meth = toMethodName("Examples_" + nb.kind, nb.name);
        if (seen.contains(meth)) continue;
        seen.insert(meth);
        const AttrSet* as = nb.examples.attrSetName.isEmpty()
            ? nullptr
            : findAttrSet(nb.examples.attrSetName, file);
        sigs.push_back({ meth, as ? (nb.examples.attrSetName + "String") : "grid" });
    }
    return sigs;
}

QString JavaScriptGenerator::genStubMethod(const GlueSig& sig)
{
    QString out;
    QTextStream s(&out);
    if (sig.paramType.isEmpty()) {
        s << "\n  " << sig.method << "() {\n";
        s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    } else if (sig.paramType == "docstring") {
        s << "\n  " << sig.method << "(value) {\n";
        s << "    console.log(value);\n";
        s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    } else if (sig.paramType == "grid" || sig.paramType == "list") {
        s << "\n  " << sig.method << "(values) {\n";
        s << "    values.forEach(row => console.log(Array.isArray(row) ? row.join(\", \") : String(row)));\n";
        s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    } else {
        s << "\n  " << sig.method << "(values) {\n";
        s << "    values.forEach(v => console.log(v.toString()));\n";
        s << "    throw new Error(\"Not implemented: " << sig.method << "\");\n";
        s << "  }";
    }
    return out;
}

bool JavaScriptGenerator::appendMissingStubs(const QString& gluePath,
                                              const QVector<GlueSig>& sigs,
                                              QStringList& msgs)
{
    QFile f(gluePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString content = QTextStream(&f).readAll();
    f.close();

    QString stubs;
    for (const GlueSig& sig : sigs) {
        // Check for "methodName(" in the file
        if (!content.contains(sig.method + "("))
            stubs += "\n" + genStubMethod(sig);
    }
    if (stubs.isEmpty()) return false;

    // Insert before the last closing "}" of the class
    const int closingBrace = content.lastIndexOf("\n}");
    if (closingBrace < 0) {
        msgs << QString("WARNING:0:Could not locate class closing brace in %1 — stubs not added")
                .arg(gluePath);
        return false;
    }
    content.insert(closingBrace, stubs);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        msgs << QString("ERROR:0:Cannot update glue file: %1").arg(gluePath);
        return false;
    }
    QTextStream(&f) << content;
    return true;
}

QString JavaScriptGenerator::genGlueFile(const SpectableFile& file,
                                          const QString& specName,
                                          const QString& commonRelPath) const
{
    const QVector<GlueSig> sigs = collectGlueSigs(file);
    const QString glueClass = toPascalCase(specName) + "Glue";
    QString out;
    QTextStream s(&out);

    s << "import { } from \"" << commonRelPath << "/index.js\";\n";
    for (const QString& imp : m_extraImports) s << imp << "\n";
    s << "\n";
    s << "export class " << glueClass << " {\n";
    s << "  static DNC_STRING = \"?DNC?\";\n";

    for (const GlueSig& sig : sigs)
        s << genStubMethod(sig) << "\n";

    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// Production class generators
// ---------------------------------------------------------------------------

QString JavaScriptGenerator::genProductionEntity(const AttrSet& as)
{
    QString out;
    QTextStream s(&out);

    s << "export class " << as.name << " {\n";
    s << "  constructor(";
    for (int i = 0; i < as.fields.size(); ++i) {
        if (i) s << ", ";
        const Field& f = as.fields[i];
        const QString fn = toCamelCase(f.name);
        if (!f.defaultValue.isEmpty())
            s << fn << " = " << jsStringEscape(f.defaultValue);
        else
            s << fn;
    }
    s << ") {\n";
    for (const Field& f : as.fields)
        s << "    this." << toCamelCase(f.name) << " = " << toCamelCase(f.name) << ";\n";
    s << "  }\n";
    s << "}\n";
    return out;
}

QString JavaScriptGenerator::genProductionCollection(const Collection& col)
{
    const QString elem = col.elementType;
    QString out;
    QTextStream s(&out);

    s << "import { " << elem << " } from \"./" << elem << ".js\";\n\n";
    s << "export class " << col.name << " {\n";
    if (!col.minimum.isEmpty())
        s << "  static MINIMUM = " << col.minimum << ";\n";
    if (!col.maximum.isEmpty())
        s << "  static MAXIMUM = " << col.maximum << ";\n";
    if (!col.minimum.isEmpty() || !col.maximum.isEmpty()) s << "\n";
    s << "  #items = [];\n\n";
    s << "  add(item) { this.#items.push(item); }\n\n";
    s << "  delete(item) {\n";
    s << "    const idx = this.#items.indexOf(item);\n";
    s << "    if (idx < 0) return false;\n";
    s << "    this.#items.splice(idx, 1);\n";
    s << "    return true;\n";
    s << "  }\n\n";
    s << "  read() { return [...this.#items]; }\n\n";
    s << "  update(oldItem, newItem) {\n";
    s << "    const idx = this.#items.indexOf(oldItem);\n";
    s << "    if (idx < 0) return false;\n";
    s << "    this.#items[idx] = newItem;\n";
    s << "    return true;\n";
    s << "  }\n\n";
    s << "  size() { return this.#items.length; }\n";
    s << "}\n";
    return out;
}

// ---------------------------------------------------------------------------
// File write helper
// ---------------------------------------------------------------------------

bool JavaScriptGenerator::writeFile(const QString& path, const QString& content, QStringList& msgs)
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

QStringList JavaScriptGenerator::generate(const SpectableFile& file, const Options& opts)
{
    QStringList msgs;
    m_extraImports = opts.extraImports;
    m_tagFilter    = opts.tagFilter;

    if (file.specName.isEmpty()) {
        msgs << "ERROR:0:No Specification declaration found";
        return msgs;
    }

    const QString specName  = file.specName;
    const QString glueClass = toPascalCase(specName) + "Glue";
    const QString specSnake = toFileName(specName);

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

    // Relative import path from the (possibly mirrored) output dir back to common/
    QString commonRelPath = dir.relativeFilePath(commonDir.path());
    if (!commonRelPath.startsWith('.')) commonRelPath.prepend("./");

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

    // 1. Common String + Typed classes
    QVector<AttrSet> domainSets;
    for (const AttrSet& as : augmented.attrSets) {
        if (as.isContext) continue;
        if (as.fields.isEmpty()) {
            msgs << QString("WARNING:%1:AttrSet '%2' has no fields — skipped")
                    .arg(as.line).arg(as.name);
            continue;
        }
        writeFile(commonDir.filePath(as.name + "String.js"), genStringClass(as), msgs);
        writeFile(commonDir.filePath(as.name + "Typed.js"),  genTypedClass(as),  msgs);
        domainSets.push_back(as);
    }
    writeFile(commonDir.filePath("index.js"), genCommonIndex(domainSets), msgs);

    // 2. Test file (always overwritten)
    {
        QStringList testErrs;
        const QString testContent = genTestFile(augmented, specName, glueClass, commonRelPath, testErrs);
        msgs << testErrs;
        const bool hasErr = std::any_of(testErrs.begin(), testErrs.end(),
            [](const QString& m){ return m.startsWith("ERROR"); });
        if (!hasErr)
            writeFile(dir.filePath("test_" + specSnake + ".test.js"), testContent, msgs);
    }

    // 3. Glue file
    {
        const QString gluePath = dir.filePath(specSnake + "_glue.js");
        if (opts.overwriteGlue || !QFile::exists(gluePath)) {
            writeFile(gluePath, genGlueFile(augmented, specName, commonRelPath), msgs);
        } else {
            const QVector<GlueSig> sigs = collectGlueSigs(augmented);
            if (appendMissingStubs(gluePath, sigs, msgs))
                msgs << QString("INFO:0:Added missing glue stubs to %1").arg(gluePath);
        }
    }

    // 4. Production classes
    if (opts.createProductionClasses && !opts.productionClassesDir.isEmpty()) {
        QDir prodDir(opts.productionClassesDir);
        if (!prodDir.exists() && !prodDir.mkpath(".")) {
            msgs << QString("ERROR:0:Cannot create production directory: %1").arg(prodDir.path());
        } else {
            for (const AttrSet& as : file.attrSets) {
                if (as.isContext || as.kind.compare("Entity", Qt::CaseInsensitive) != 0) continue;
                const QString prodPath = prodDir.filePath(as.name + ".js");
                if (!QFile::exists(prodPath))
                    writeFile(prodPath, genProductionEntity(as), msgs);
            }
            for (const Collection& col : file.collections) {
                if (col.isContext || col.name.isEmpty() || col.elementType.isEmpty()) continue;
                const QString prodPath = prodDir.filePath(col.name + ".js");
                if (!QFile::exists(prodPath))
                    writeFile(prodPath, genProductionCollection(col), msgs);
            }
        }
    }

    return msgs;
}
