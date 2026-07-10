#pragma once

#include "SpectableModel.h"
#include <QString>
#include <QSet>

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
    static bool        isStepLine(const QString& trimmed, QString& kw, QString& text,
                                  QString& attrSet, bool& transposed, bool& compareOnly);
    static bool        isContinuation(const QString& line);
    static bool        isSkipKeyword(const QString& firstWord);
};
