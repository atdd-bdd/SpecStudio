#pragma once

#include <QString>
#include <QStringList>

// Evaluates boolean tag-filter expressions against a block's generator tags.
//
// Syntax:  smoke AND NOT wip
//          (smoke OR regression) AND NOT draft
// Keywords AND, OR, NOT are case-insensitive. Tag names are case-insensitive.
// Empty filter always returns true (generate everything).
class TagFilter
{
public:
    static bool matches(const QString& filter, const QStringList& generatorTags);

private:
    struct Parser {
        QStringList tokens;
        int         pos;
        QStringList tagsUpper;  // block's generator tags, uppercased for comparison

        bool parseOr();
        bool parseAnd();
        bool parseNot();
        bool parsePrimary();

        bool   atEnd()  const { return pos >= tokens.size(); }
        QString peek()  const { return atEnd() ? QString() : tokens[pos].toUpper(); }
        void   consume()      { if (!atEnd()) ++pos; }
    };

    static QStringList tokenize(const QString& expr);
};
