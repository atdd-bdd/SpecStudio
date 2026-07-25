#pragma once

#include <QString>

// ---------------------------------------------------------------------------
// Helpers for scanning an existing generated file.
//
// The generators decide whether a glue method still needs a stub by searching
// the file for its signature. A plain substring search is fooled by a
// commented-out definition: someone who comments a method out has removed it
// as far as the compiler is concerned, and expects it to be regenerated.
// Blanking the comments first makes the search agree with the compiler.
//
// Comment characters are replaced with spaces rather than deleted so that
// offsets into the stripped text still line up with the original.
// ---------------------------------------------------------------------------

namespace sourcescan {

// C-style comments: // to end of line, and /* ... */.
// String and character literals are honoured, so a "//" inside a literal is
// not mistaken for the start of a comment.
inline QString stripCStyleComments(const QString& src)
{
    enum class State { Code, LineComment, BlockComment, DoubleQuote, SingleQuote };
    State state = State::Code;

    QString out = src;
    const int n = src.size();

    auto blank = [&out](int i) {
        if (out[i] != '\n') out[i] = ' ';
    };

    for (int i = 0; i < n; ++i) {
        const QChar c    = src[i];
        const QChar next = (i + 1 < n) ? src[i + 1] : QChar();

        switch (state) {
        case State::Code:
            if (c == '/' && next == '/') {
                state = State::LineComment;
                blank(i); blank(i + 1);
                ++i;
            } else if (c == '/' && next == '*') {
                state = State::BlockComment;
                blank(i); blank(i + 1);
                ++i;
            } else if (c == '"') {
                state = State::DoubleQuote;
            } else if (c == '\'') {
                state = State::SingleQuote;
            }
            break;

        case State::LineComment:
            if (c == '\n') state = State::Code;
            else           blank(i);
            break;

        case State::BlockComment:
            blank(i);
            if (c == '*' && next == '/') {
                blank(i + 1);
                ++i;
                state = State::Code;
            }
            break;

        case State::DoubleQuote:
            if (c == '\\')      ++i;             // skip the escaped character
            else if (c == '"')  state = State::Code;
            break;

        case State::SingleQuote:
            if (c == '\\')      ++i;
            else if (c == '\'') state = State::Code;
            break;
        }
    }
    return out;
}

// Hash comments: # to end of line (Python, and shell-style files).
// Quotes are honoured so a '#' inside a string is left alone.
inline QString stripHashComments(const QString& src)
{
    enum class State { Code, LineComment, DoubleQuote, SingleQuote };
    State state = State::Code;

    QString out = src;
    const int n = src.size();

    for (int i = 0; i < n; ++i) {
        const QChar c = src[i];

        switch (state) {
        case State::Code:
            if (c == '#') {
                state = State::LineComment;
                out[i] = ' ';
            } else if (c == '"') {
                state = State::DoubleQuote;
            } else if (c == '\'') {
                state = State::SingleQuote;
            }
            break;

        case State::LineComment:
            if (c == '\n') state = State::Code;
            else           out[i] = ' ';
            break;

        case State::DoubleQuote:
            if (c == '\\')     ++i;
            else if (c == '"') state = State::Code;
            break;

        case State::SingleQuote:
            if (c == '\\')      ++i;
            else if (c == '\'') state = State::Code;
            break;
        }
    }
    return out;
}

} // namespace sourcescan
