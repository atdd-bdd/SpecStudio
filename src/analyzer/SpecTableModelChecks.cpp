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
#include "SpecTableIndex.h"

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
// The visible symbols still come from the index: a type may be declared in a
// sibling file, and the parse tree here is one file.

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

            // A DomainTerm names a thing the domain talks about, and is a fine
            // name for an attribute. It is not a type: no generator has a
            // conversion for one, so every language emits a class declaring a
            // field of a type nothing writes, and the build fails in a file the
            // author never opened. Saying so here puts the failure where it can
            // be acted on.
            //
            // This used to be an error and a warning at the same time: the
            // converter printed "no parse conversion available" and generated
            // the broken class anyway, while Analyze called the type legal.
            if (visible.domainTerms.contains(declared)) {
                const QString underlying = m_index ? m_index->domainTermTypes().value(declared)
                                                   : QString();
                QString message = QStringLiteral(
                    "Attribute '%1' has type '%2', which is a DomainTerm. A DomainTerm "
                    "names an attribute, not a type -- nothing is generated for one, so "
                    "the generated class would declare a field of a type that does not exist")
                        .arg(f.name, declared);
                if (!underlying.isEmpty())
                    message += QStringLiteral(". Use '%1', which is what '%2' is declared as")
                                   .arg(underlying, declared);
                out.append(makeDiag(filePath, f.line, message, Diagnostic::Severity::Error));
                continue;
            }

            if (visible.hasDataType(declared) || visible.hasAttributeSet(declared))
                continue;

            out.append(makeDiag(filePath, f.line,
                QStringLiteral("Attribute '%1' has unknown type '%2' — not a built-in, "
                               "DataType, Entity/Attributes, or Collection")
                    .arg(f.name, declared),
                Diagnostic::Severity::Warning));
        }
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

    checkParseMessages          (filePath, file, out);
    checkEmptyScenarios         (filePath, file, out);
    checkDuplicateFieldNames    (filePath, file, out);
    checkDuplicateScenarioNames (filePath, file, out);
    checkEmptyAttrSets          (filePath, file, out);
    checkAttributeFieldTypes    (filePath, file, m_index->projectSymbols(), out);
    // The same reading the converter uses, rather than a second one that can
    // disagree with it.
    for (const ParseMessage& m : validateStepTables(file))
        out.append(makeDiag(filePath, m.line, m.text,
                            m.warning ? Diagnostic::Severity::Warning
                                      : Diagnostic::Severity::Error));
}
