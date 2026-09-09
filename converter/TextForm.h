#pragma once

#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// The text form of an Entity, as it appears in a table cell.
//
// The attribute values as space separated tokens, in the order the attributes
// are declared. A value containing a space is wrapped in double quotes; a
// nested Entity's own text form is wrapped in single quotes. A run of spaces
// separates one token from the next exactly as a single space does.
//
//     Money    Amount, Currency        25 USD
//     Place    with a spaced value     "1 Penny Lane" Liverpool NY
//     Holding  with a nested Money     IBM 1000 '25 USD'
//
// Generators that build a nested String literal at generation time split the
// cell with this; the languages that read it at run time carry the same rules
// in their own tokens module. The two must agree, so this is the one written
// description of them.
// ---------------------------------------------------------------------------

namespace textform {

// The closing quote is the next one of the same kind that ends the token --
// one followed by whitespace or by the end of the text. Scanning for that
// rather than for the first quote is what lets a nested Entity, itself single
// quoted, sit inside a single quoted value. Returns -1 when there is none.
inline int closingQuote(const QString& text, int open, QChar quote)
{
    for (int j = open + 1; j < text.size(); ++j) {
        if (text[j] != quote) continue;
        if (j + 1 == text.size() || text[j + 1].isSpace()) return j;
    }
    return -1;
}

// Splits a cell into one string per attribute. An unterminated quote is taken
// to run to the end of the text rather than reported: a generator has nowhere
// good to raise, and the mismatch shows up as a wrong field count instead.
inline QStringList split(const QString& text)
{
    QStringList out;
    const int n = text.size();
    int i = 0;
    while (i < n) {
        while (i < n && text[i].isSpace()) ++i;
        if (i >= n) break;
        const QChar c = text[i];
        if (c == '"' || c == '\'') {
            const int close = closingQuote(text, i, c);
            if (close < 0) { out << text.mid(i + 1); break; }
            out << text.mid(i + 1, close - i - 1);
            i = close + 1;
        } else {
            int j = i;
            while (j < n && !text[j].isSpace()) ++j;
            out << text.mid(i, j - i);
            i = j;
        }
    }
    return out;
}

} // namespace textform
