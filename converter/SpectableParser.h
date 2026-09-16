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
