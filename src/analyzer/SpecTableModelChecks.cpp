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
// Entry point — parse once, run every model check
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::runModelChecks(const QString& filePath,
                                        QList<Diagnostic>& out) const
{
    SpectableParser parser;
    const SpectableFile file = parser.parse(filePath);

    checkParseMessages (filePath, file, out);
    checkEmptyScenarios(filePath, file, out);
}
