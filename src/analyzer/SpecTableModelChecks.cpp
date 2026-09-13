// PROTOTYPE — Analyze reading the converter's own parse tree
//
// Every other check in SpecTableAnalyzer works by regular expression over raw
// lines. The converter does not: it parses the same files into a SpectableFile
// and generates from that. So Analyze is a second, weaker implementation of the
// same reading, and the two can disagree. They already have -- SpecTableIndex.h
// records a release where Analyze reported an undeclared type for a
// specification that built and ran perfectly, because its built-in type list had
// fallen behind the generators'.
//
// This file asks what it costs to stop doing that: link converter/SpectableParser
// and check the model. The answer is two files and no new dependency -- the
// parser needs only Qt and SpectableModel.h.
//
// The two checks below are chosen because they are awkward in regex and obvious
// in the model.

#include "SpecTableAnalyzer.h"

#include "SpectableParser.h"

#include <QFileInfo>
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
// A step table with no column for a field that has no default
// ---------------------------------------------------------------------------
//
// This one the converter does catch, as an error, and it suppresses the whole
// test file when it fires:
//
//     ERROR:22:Table is missing column 'Wanted' and 'Wanted' has no default value
//
// So the cost of not knowing is every test in the specification, not one. Worth
// saying before the build rather than during it.

void SpecTableAnalyzer::checkStepTableRequiredColumns(const QString& filePath,
                                                       const SpectableFile& file,
                                                       QList<Diagnostic>& out) const
{
    // Every attribute set the file can see, including those merged in by Import.
    QMap<QString, const AttrSet*> byName;
    for (const AttrSet& as : file.attrSets) byName.insert(as.name.toLower(), &as);

    auto checkStep = [&](const Step& step) {
        if (!step.hasTable || step.attrSetName.isEmpty()) return;
        if (step.compareOnly) return;   // CompareOnly names only what it cares about
        if (step.table.rows.isEmpty()) return;

        const AttrSet* as = byName.value(step.attrSetName.toLower(), nullptr);
        if (!as || as->fields.isEmpty()) return;   // unknown or empty: reported elsewhere

        // Only a horizontal table. The generator makes this an error in its
        // horizontal branch alone -- a vertical table names the attributes it
        // sets and says nothing about the rest, which is how every Request in
        // API.spectable omits Parameter and Body and still generates. Checking
        // vertical tables here reported three errors against a specification
        // whose tests all pass.
        if (step.vertical) return;
        if (!step.table.hasHeader) return;

        QSet<QString> present;
        for (const QString& h : step.table.rows.first()) present.insert(h.toLower());

        for (const Field& f : as->fields) {
            if (f.name.isEmpty()) continue;
            if (present.contains(f.name.toLower())) continue;
            if (!f.defaultValue.trimmed().isEmpty()) continue;   // a default fills it

            out.append(makeDiag(filePath, step.line,
                QStringLiteral("This table has no column '%1', and '%1' on '%2' has no default "
                               "value — the generator refuses this and writes no test file for "
                               "the whole specification")
                    .arg(f.name, as->name)));
        }
    };

    for (const Scenario& s : file.scenarios)
        for (const Step& step : s.steps) checkStep(step);
    for (const Step& step : file.backgroundSteps) checkStep(step);
    for (const Step& step : file.cleanupSteps)    checkStep(step);
}

// ---------------------------------------------------------------------------
// Entry point — parse once, run every model check
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::runModelChecks(const QString& filePath,
                                        QList<Diagnostic>& out) const
{
    SpectableParser parser;
    const SpectableFile file = parser.parse(filePath);

    checkParseMessages          (filePath, file, out);
    checkEmptyScenarios         (filePath, file, out);
    checkDuplicateFieldNames    (filePath, file, out);
    checkDuplicateScenarioNames (filePath, file, out);
    checkEmptyAttrSets          (filePath, file, out);
    checkStepTableRequiredColumns(filePath, file, out);
}
