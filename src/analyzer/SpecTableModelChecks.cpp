// Analyze reading the converter's own parse tree
//
// Every check about the contents of one file lives here and reads a
// SpectableFile -- the same structure the generators read -- rather than the
// file's lines. It began on 2026-09-13 as a prototype of two checks, chosen
// because they were awkward in regex and obvious in the model; by 2026-09-16
// the last regex check was gone. The checks that remain in SpecTableAnalyzer.cpp
// are about names across files and read the index.
//
// The file arrives from SpecTableIndex::fileWithContext with every sibling
// merged in and DomainTerms and Define references resolved, which is what the
// converter does before it generates. So a name declared elsewhere in the
// project is visible here exactly as it is to the generators, and the shared
// validators at the end of runModelChecks say the same thing in both places.

#include "SpecTableAnalyzer.h"

#include "SpectableParser.h"
#include "SpecTableIndex.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

// ---------------------------------------------------------------------------
// Whatever the parser itself noticed
// ---------------------------------------------------------------------------
//
// The converter has always collected these and printed them at build time. They
// are the parser's own reading of the file, so surfacing them costs nothing and
// cannot drift from what the generator saw.

void SpecTableAnalyzer::checkParseMessages(const QString& filePath,
                                            const SpectableFile& file,
                                            QList<Diagnostic>& out) const
{
    for (const ParseMessage& m : file.messages) {
        // A parse message carries no file path: it is always about the file that
        // was parsed, and imports are merged into that same result.
        out.append(makeDiag(filePath, m.line, m.text,
                            m.warning ? Diagnostic::Severity::Warning
                                      : Diagnostic::Severity::Error));
    }
}

// ---------------------------------------------------------------------------
// A block that generates a test with nothing in it
// ---------------------------------------------------------------------------
//
// This is the check that motivated the exercise. A Scenario carrying only an
// Examples: table produces a test body with no glue call in it -- the table is
// never read by anything, the test passes, and nothing warns. Nine rows of
// expected error messages sat that way in Formatted Dollar until 2026-09-13.
//
// In regex this means finding the Scenario, finding where it ends, and deciding
// whether what lies between holds a step. Against the model it is one question
// asked of one vector.

void SpecTableAnalyzer::checkEmptyScenarios(const QString& filePath,
                                             const SpectableFile& file,
                                             QList<Diagnostic>& out) const
{
    for (const Scenario& s : file.scenarios) {
        if (!s.steps.isEmpty()) continue;

        out.append(makeDiag(filePath, s.line,
            QStringLiteral("Scenario '%1' has no Given/When/Then step, so it generates a test "
                           "with nothing in it. A table here is read by no one and the test "
                           "passes regardless -- if it carries an Examples: table, declare its "
                           "attribute set and make this a BusinessRule.").arg(s.name),
            Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// Two names that become one name in the generated code
// ---------------------------------------------------------------------------
//
// Both of these were confirmed by generating Java and compiling it. Neither
// produces so much as a warning from the converter; the first sign of either is
// the compiler.
//
//     common\TwiceString.java:9: error: variable amount is already defined
//     Probe2_Test.java:23: error: method Scenario_Same_Name() is already defined
//
// The model makes both a question about one vector. In regex they would mean
// finding each block, finding its extent, and collecting its rows -- which is
// the work the parser has already done.

void SpecTableAnalyzer::checkDuplicateFieldNames(const QString& filePath,
                                                  const SpectableFile& file,
                                                  QList<Diagnostic>& out) const
{
    for (const AttrSet& as : file.attrSets) {
        if (as.isContext) continue;   // declared elsewhere; reported at its own file

        QSet<QString> seen;
        for (const Field& f : as.fields) {
            const QString key = f.name.toLower();
            if (key.isEmpty()) continue;

            if (seen.contains(key))
                out.append(makeDiag(filePath, as.line,
                    QStringLiteral("%1 '%2' declares '%3' twice — the generated class gets two "
                                   "members of that name and will not compile")
                        .arg(as.kind, as.name, f.name)));
            seen.insert(key);
        }
    }
}

void SpecTableAnalyzer::checkDuplicateScenarioNames(const QString& filePath,
                                                     const SpectableFile& file,
                                                     QList<Diagnostic>& out) const
{
    QMap<QString, int> firstSeenAt;

    for (const Scenario& s : file.scenarios) {
        const QString key = s.name.trimmed().toLower();
        if (key.isEmpty()) continue;

        if (firstSeenAt.contains(key)) {
            out.append(makeDiag(filePath, s.line,
                QStringLiteral("A Scenario named '%1' is already declared at line %2 — both "
                               "generate a test method of the same name, and the file will not "
                               "compile").arg(s.name).arg(firstSeenAt.value(key))));
            continue;
        }
        firstSeenAt.insert(key, s.line);
    }
}

// ---------------------------------------------------------------------------
// An attribute set that generates no class
// ---------------------------------------------------------------------------
//
// The converter says "AttrSet 'Empty' has no fields — skipped" and carries on,
// so nothing is generated for it and every step naming it refers to a class that
// does not exist.

void SpecTableAnalyzer::checkEmptyAttrSets(const QString& filePath,
                                            const SpectableFile& file,
                                            QList<Diagnostic>& out) const
{
    for (const AttrSet& as : file.attrSets) {
        if (as.isContext || !as.fields.isEmpty()) continue;

        out.append(makeDiag(filePath, as.line,
            QStringLiteral("%1 '%2' declares no attributes, so no class is generated for it — "
                           "any step naming it refers to a type that does not exist")
                .arg(as.kind, as.name),
            Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// The type each attribute declares
// ---------------------------------------------------------------------------
//
// Moved off regular expressions on 2026-09-15. The old version reopened the
// file, matched `^\s*(Attributes|Entity)\s+\w+`, hunted for the header row,
// worked out which column was called Type, and walked the rows — a second
// reading of what the parser had already done, and the kind of reading that
// disagrees with the generator the moment the table syntax grows. Vertical
// tables had already shown that happening.
//
// Against the model it is two nested loops over fields that are already parsed,
// and `f.line` points at the row the field came from, so the diagnostic lands
// where the old one did.
//
// The visible symbols come from the index rather than the merged parse tree,
// so that the message can name what the index knows, which is what the editor shows.

void SpecTableAnalyzer::checkAttributeFieldTypes(const QString& filePath,
                                                  const SpectableFile& file,
                                                  const SpecTableSymbols& visible,
                                                  QList<Diagnostic>& out) const
{
    for (const AttrSet& as : file.attrSets) {
        if (as.isContext) continue;

        for (const Field& f : as.fields) {
            const QString declared = f.type.trimmed();
            if (declared.isEmpty()) continue;

            // A DomainTerm is a legal field type again as of 2026-09-16: it is
            // resolved to the type it stands for before any generator sees it,
            // so the field no longer names a class nothing writes. Anything that
            // resolves to nothing is reported by validateFieldTypes, as an error.
            if (visible.hasDataType(declared) || visible.hasAttributeSet(declared)
             || visible.domainTerms.contains(declared))
                continue;

            // An error, not a warning, and the converter agrees: the
            // generators emit the class regardless, declaring a field of a type
            // that does not exist and assigning a String to it, so the build
            // fails in a file the author never opened.
            //
            // Asked of the index rather than of this file's parse tree, because
            // a DataType is often declared in a sibling specification --
            // validateFieldTypes in the parser has the context merge behind it
            // and is the converter's half of the same rule.
            out.append(makeDiag(filePath, f.line,
                QStringLiteral("Attribute '%1' has unknown type '%2' — not a built-in, "
                               "DataType, Entity/Attributes, Collection, or DomainTerm")
                    .arg(f.name, declared)));
        }
    }
}

// ---------------------------------------------------------------------------
// An attribute named after a DomainTerm, typed as something else
// ---------------------------------------------------------------------------
//
// A DomainTerm says what a word means in this domain, including the type it
// stands for. An attribute that borrows the word and then declares a different
// type is saying two things at once, and the specification no longer agrees
// with itself.
//
// Moved off regular expressions on 2026-09-15, the same walk as
// checkAttributeFieldTypes: find the declaration, find the header row, work out
// which column is called Type, walk the rows. The parse tree has all of it, and
// f.line points at the row.

void SpecTableAnalyzer::checkDomainTermColumnTypes(const QString& filePath,
                                                    const SpectableFile& file,
                                                    const QMap<QString, QString>& dtTypes,
                                                    QList<Diagnostic>& out) const
{
    if (dtTypes.isEmpty()) return;

    for (const AttrSet& as : file.attrSets) {
        if (as.isContext) continue;

        for (const Field& f : as.fields) {
            const QString declared = f.type.trimmed();
            if (f.name.isEmpty() || declared.isEmpty()) continue;
            if (!dtTypes.contains(f.name)) continue;

            const QString termType = dtTypes.value(f.name);
            if (termType.compare(declared, Qt::CaseInsensitive) == 0) continue;

            out.append(makeDiag(filePath, f.line,
                QStringLiteral("DomainTerm '%1' has type '%2' but is declared as '%3' here")
                    .arg(f.name, termType, declared),
                Diagnostic::Severity::Warning));
        }
    }
}

// ---------------------------------------------------------------------------
// One name, two kinds
// ---------------------------------------------------------------------------
//
// The duplicate-declaration check is keyed by kind, so it catches two DataTypes
// called Score and says nothing about a DataType and an Attributes block that
// share the name. That one is worse. The generators decide what a step's table
// means by asking isDataType and isAttrSetType in turn, and they ask in
// different orders in different places -- so a step naming ": Score" is a typed
// grid in one branch and an attribute-set table in another, and which one wins
// depends on which branch runs first.
//
// Only the kinds a generator tells apart are in play: Entity and Attributes
// (one namespace between them, both attribute sets), DataType, Collection and
// DomainTerm. A Define lives behind "=" and a Scenario behind its own keyword,
// so neither can be confused with a type.
//
// Asked of the index rather than this file's parse tree, because the two
// declarations are very likely in different files -- that is how it happens
// without anyone noticing.

void SpecTableAnalyzer::checkNameDeclaredAsTwoKinds(const QString& filePath,
                                                     const SpecTableSymbols& visible,
                                                     QList<Diagnostic>& out) const
{
    struct Kind { const char* label; const QMap<QString, SymbolLocation>* map; };
    const Kind kinds[] = {
        { "Entity",     &visible.entities    },
        { "Attributes", &visible.attributes  },
        { "DataType",   &visible.dataTypes   },
        { "Collection", &visible.collections },
        { "DomainTerm", &visible.domainTerms },
    };

    // name -> every (kind, where) it is declared as
    struct Decl { QString name; QString kind; SymbolLocation where; };
    QMap<QString, QVector<Decl>> byName;   // keyed lower-case; Decl keeps the author's casing
    for (const Kind& k : kinds)
        for (auto it = k.map->cbegin(); it != k.map->cend(); ++it)
            byName[it.key().toLower()].append({ it.key(), QString::fromLatin1(k.label), it.value() });

    for (auto it = byName.cbegin(); it != byName.cend(); ++it) {
        const QVector<Decl>& decls = it.value();
        if (decls.size() < 2) continue;

        // Report once per file that holds one of the declarations, at that
        // declaration, naming the others -- so opening either file shows it.
        for (const Decl& here : decls) {
            if (here.where.filePath != filePath) continue;

            QStringList others;
            for (const Decl& d : decls) {
                if (&d == &here) continue;
                others << QStringLiteral("%1 at %2:%3")
                              .arg(d.kind, QFileInfo(d.where.filePath).fileName())
                              .arg(d.where.line);
            }
            out.append(makeDiag(filePath, here.where.line,
                QStringLiteral("'%1' is declared here as %2 and also as %3. A name can be "
                               "one kind of thing: the generators decide what a step's table "
                               "means by asking which kind this is, and with two answers the "
                               "result depends on which they ask first")
                    .arg(here.name, here.kind, others.join(", "))));
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point — parse once, run every model check
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::runModelChecks(const QString& filePath,
                                        QList<Diagnostic>& out) const
{
    // Parsed once per index rebuild, merged with its siblings, resolved --
    // the converter's view of the file just before it generates.
    const SpectableFile file = m_index->fileWithContext(filePath);
    const SpecTableSymbols& visible = m_index->projectSymbols();

    checkParseMessages          (filePath, file, out);
    checkStepRefs               (filePath, file, visible, out);
    checkDescriptions           (filePath, file, out);
    checkExamples               (filePath, file, visible, out);
    checkDefineRefs             (filePath, file, visible, out);
    checkEmptyScenarios         (filePath, file, out);
    checkDuplicateFieldNames    (filePath, file, out);
    checkDuplicateScenarioNames (filePath, file, out);
    checkEmptyAttrSets          (filePath, file, out);
    checkAttributeFieldTypes    (filePath, file, m_index->projectSymbols(), out);
    checkDomainTermColumnTypes  (filePath, file, m_index->domainTermTypes(), out);
    checkNameDeclaredAsTwoKinds (filePath, m_index->projectSymbols(), out);

    // The same reading the converter uses, rather than a second one that can
    // disagree with it: every table against its attribute set, and every cell
    // against its column's type.
    QVector<ParseMessage> shared;
    shared += validateStepTables(file);
    shared += validateExamplesTables(file);
    shared += validateAttributeDefaults(file);
    for (const ParseMessage& m : shared)
        out.append(makeDiag(filePath, m.line, m.text,
                            m.warning ? Diagnostic::Severity::Warning
                                      : Diagnostic::Severity::Error));
}

// ---------------------------------------------------------------------------
// What a step names
// ---------------------------------------------------------------------------
//
// Moved off regular expressions on 2026-09-16. The old pattern matched a step
// line ending in ": Name", optionally followed by one of two modifiers -- so a
// step carrying EveryCell, or two modifiers, was never checked at all. The
// parser has already read the step, whatever it carries.
//
// "applying BusinessRule X" and "applying Calculation X" are Analyze's own
// reading of the step text: no generator treats them specially.

void SpecTableAnalyzer::checkStepRefs(const QString& filePath,
                                       const SpectableFile& file,
                                       const SpecTableSymbols& visible,
                                       QList<Diagnostic>& out) const
{
    static const QRegularExpression reRule(R"(\bapplying\s+BusinessRule\s+(\w+)\s*$)",
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reCalc(R"(\bapplying\s+Calculation\s+(\w+)\s*$)",
                                           QRegularExpression::CaseInsensitiveOption);

    auto check = [&](const Step& step) {
        if (step.attrSetName.isEmpty()) return;

        auto m = reRule.match(step.text);
        if (m.hasMatch()) {
            if (!visible.hasBusinessRule(m.captured(1)))
                out.append(makeDiag(filePath, step.line,
                    QStringLiteral("Unknown BusinessRule '%1'").arg(m.captured(1))));
            if (!visible.hasAttributeSet(step.attrSetName))
                out.append(makeDiag(filePath, step.line,
                    QStringLiteral("Unknown AttributeSet '%1'").arg(step.attrSetName)));
            return;
        }
        m = reCalc.match(step.text);
        if (m.hasMatch()) {
            if (!visible.hasCalculation(m.captured(1)))
                out.append(makeDiag(filePath, step.line,
                    QStringLiteral("Unknown Calculation '%1'").arg(m.captured(1))));
            if (!visible.hasAttributeSet(step.attrSetName))
                out.append(makeDiag(filePath, step.line,
                    QStringLiteral("Unknown AttributeSet '%1'").arg(step.attrSetName)));
            return;
        }
        if (!visible.hasAttributeSet(step.attrSetName) && !visible.hasDataType(step.attrSetName))
            out.append(makeDiag(filePath, step.line,
                QStringLiteral("Unknown AttributeSet, Entity, or DataType '%1'")
                    .arg(step.attrSetName)));
    };

    for (const Scenario& s : file.scenarios)
        for (const Step& step : s.steps) check(step);
    for (const Step& step : file.backgroundSteps) check(step);
    for (const Step& step : file.cleanupSteps)    check(step);
}

// ---------------------------------------------------------------------------
// A block that does not say what it is
// ---------------------------------------------------------------------------
//
// The parser keeps a named block's Description. The old check looked for one
// within three lines of the declaration and accepted a legacy "* text" form;
// now a Description anywhere in the block counts, and the legacy form does not
// -- the parser has never read it, and reports it as an unrecognised keyword.

void SpecTableAnalyzer::checkDescriptions(const QString& filePath,
                                           const SpectableFile& file,
                                           QList<Diagnostic>& out) const
{
    for (const NamedBlock& nb : file.namedBlocks) {
        if (nb.isContext || !nb.description.trimmed().isEmpty()) continue;
        out.append(makeDiag(filePath, nb.line,
            QStringLiteral("%1 '%2' has no Description").arg(nb.kind, nb.name),
            Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// A block with nothing to test it by
// ---------------------------------------------------------------------------
//
// A BusinessRule or Calculation is only tested through its Examples: table,
// and a DataType through the table of values it accepts. The set an Examples:
// names has to exist, or the rows have no shape.

void SpecTableAnalyzer::checkExamples(const QString& filePath,
                                       const SpectableFile& file,
                                       const SpecTableSymbols& visible,
                                       QList<Diagnostic>& out) const
{
    for (const NamedBlock& nb : file.namedBlocks) {
        if (nb.isContext) continue;
        const bool hasExamplesLine = nb.examples.line != 0;

        if (hasExamplesLine && !nb.examples.attrSetName.isEmpty()
                && !visible.hasAttributeSet(nb.examples.attrSetName))
            out.append(makeDiag(filePath, nb.examples.line,
                QStringLiteral("Unknown AttributeSet '%1' in Examples:")
                    .arg(nb.examples.attrSetName)));

        if (hasExamplesLine) continue;
        if (nb.kind.compare("DataType", Qt::CaseInsensitive) == 0)
            out.append(makeDiag(filePath, nb.line,
                QStringLiteral("DataType '%1' has no data table or Examples: section").arg(nb.name),
                Diagnostic::Severity::Warning));
        else
            out.append(makeDiag(filePath, nb.line,
                QStringLiteral("%1 '%2' has no Examples: section").arg(nb.kind, nb.name),
                Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// A reference to a Define that does not exist
// ---------------------------------------------------------------------------
//
// Everywhere "=Name" may stand: in place of a step's table, in a cell of a
// step table, an Examples: table or a table-form Define, and in a Default
// column. A Default or Examples cell that resolved is no longer written =Name
// by the time this runs, so what remains is what did not resolve. The old
// pattern matched "=word" anywhere on any line, docstrings included.

void SpecTableAnalyzer::checkDefineRefs(const QString& filePath,
                                         const SpectableFile& file,
                                         const SpecTableSymbols& visible,
                                         QList<Diagnostic>& out) const
{
    static const QRegularExpression reRef(R"(^=\s*([A-Za-z_]\w*))");

    auto check = [&](int line, const QString& cell) {
        auto m = reRef.match(cell.trimmed());
        if (!m.hasMatch() || visible.hasDefine(m.captured(1))) return;
        out.append(makeDiag(filePath, line,
            QStringLiteral("Undefined value reference '=%1'").arg(m.captured(1))));
    };
    auto checkStep = [&](const Step& step) {
        if (!step.defineRef.isEmpty())
            check(step.defineRefLine ? step.defineRefLine : step.line, "=" + step.defineRef);
        for (const QStringList& row : step.table.rows)
            for (const QString& cell : row) check(step.line, cell);
    };

    for (const Scenario& s : file.scenarios)
        for (const Step& step : s.steps) checkStep(step);
    for (const Step& step : file.backgroundSteps) checkStep(step);
    for (const Step& step : file.cleanupSteps)    checkStep(step);

    for (const NamedBlock& nb : file.namedBlocks) {
        if (nb.isContext) continue;
        for (const QStringList& row : nb.examples.rows)
            for (const QString& cell : row) check(nb.examples.line, cell);
    }
    for (const AttrSet& as : file.attrSets) {
        if (as.isContext) continue;
        for (const Field& f : as.fields) check(f.line, f.defaultValue);
    }
    for (const Define& d : file.defines) {
        if (d.isContext) continue;
        for (const QStringList& row : d.tableRows)
            for (const QString& cell : row) check(d.line, cell);
    }
}
