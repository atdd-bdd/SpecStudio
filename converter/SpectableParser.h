#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QSet>

// Checks every step table against the attribute set its step names, and returns
// what is wrong with them. Shared so that one reading serves both callers: the
// converter runs it after merging context files, and the IDE's Analyze runs it
// on the file it just parsed. It used to live in JavaGenerator, where the other
// eight languages could not benefit and Analyze had to reimplement it.
QVector<ParseMessage> validateStepTables(const SpectableFile& file);

// Rewrites every field type that names a DomainTerm to the type the term stands
// for. "DomainTerm Roll : Pins" says a Roll is a Pins, and nothing is generated
// for a DomainTerm itself, so a field left declaring Roll would name a class
// nothing writes.
//
// Called after the context merge, not during parsing: a term may be declared in
// a sibling specification, and the parser sees one file. Both the converter and
// Analyze call it, so they resolve alike.
void resolveDomainTermTypes(SpectableFile& file);

// Every field type that is not a built-in, a DataType, an Entity or Attributes
// block, a Collection, or a DomainTerm resolved by the call above. An error, not
// a warning: the generators emit the class anyway, declaring a field of a type
// that does not exist and assigning a String to it, so the build fails in a file
// the author never opened.
QVector<ParseMessage> validateFieldTypes(const SpectableFile& file);

// Expands "=Name" to the scalar Define it names, in the two places the
// generators do not: a Default column, and a cell of an Examples table. Step
// tables already resolve at generation time, in every language, so a cell there
// is left alone.
//
// A docstring Define expands to its text; a table-form Define is not one value
// and is left as written, for the same reason EveryCell refuses it. Called after
// the context merge, because the Define may be a sibling's.
void resolveDefineReferences(SpectableFile& file);

// An Examples: table against the attribute set it names, and the Default
// column of each Attributes or Entity block against the type beside it. Both
// ask what validateStepTables asks of a step table -- columns and cells --
// and both used to be Analyze's alone, so the converter accepted what the
// editor flagged. Called after the context merge, like the rest.
QVector<ParseMessage> validateExamplesTables(const SpectableFile& file);
QVector<ParseMessage> validateAttributeDefaults(const SpectableFile& file);

// Works out which way a step table runs when the step did not say. The step
// names its attribute set, so the field names are known before the table is
// read: a table whose header row is all field names is horizontal; one whose
// first column is all field names, while its first row is not, is a
// transposed one, and is marked Vertical as if the word had been written.
// Anything else is left as written. Called after the context merge, since
// the set may be declared in a sibling, and before validateStepTables.
void inferTableOrientation(SpectableFile& file);

// Makes a sibling specification's declarations visible to this one, marked as
// context so nothing is generated for them here. The converter merges every
// other file in the project this way, and Analyze does the same, so the two
// resolve a name identically.
void mergeContext(SpectableFile& file, const SpectableFile& ctx);

// The built-in DataType names, as written: Integer, String, Decimal and the
// rest. One list, here, for the parser's own type check, for Analyze's symbol
// table and for the editor's completions. Analyze kept its own copy once, and
// it fell behind: Decimal was accepted by every generator while Analyze called
// it undeclared.
const QStringList& builtinDataTypeNames();
// True for a built-in, case-insensitively, and for the aliases the generators
// also accept (int, long, bool).
bool isBuiltinDataType(const QString& name);

class SpectableParser
{
public:
    // Parse a .spectable file by path; errors are embedded in SpectableFile::messages.
    // Import statements are followed and their AttrSets/Defines are merged in.
    SpectableFile parse(const QString& filePath);

private:
    SpectableFile parseImpl(const QString& filePath, QSet<QString>& visited);
    // Helpers
    static QStringList splitPipeRow(const QString& line);
    static QString     normalizeKeyword(const QString& kw, const QString& last);
    static QString     toMethodName(const QString& stepText);
    static bool        isPipeRow(const QString& trimmed);
    static bool        isDefineLine(const QString& trimmed, QString& defineName);
    // badModifier is set when a second word follows the type and is neither
    // Vertical nor CompareOnly, so the caller can name the real mistake.
    static bool        isStepLine(const QString& trimmed, QString& kw, QString& text,
                                  QString& attrSet, bool& vertical, bool& compareOnly,
                                  bool& everyCell, QString& badModifier);
    static bool        isContinuation(const QString& line);
    static bool        isNamedComment(const QString& firstWord);
    static bool        isSkipKeyword(const QString& firstWord);
};
