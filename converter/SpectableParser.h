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
