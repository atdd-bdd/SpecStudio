#pragma once

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

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

// ---------------------------------------------------------------------------
// ProductionScan — is a type already implemented somewhere under the
// production folder?
//
// A production file is only ever created, never overwritten, so the developer's
// work survives. That check used to ask "does <TypeName>.<ext> exist?", which
// assumes one class per file named after the class. That holds in Java (the
// compiler enforces it) and is the norm in C#, but grouping related types in a
// single file is idiomatic Go, normal Rust, and mainstream Python. A developer
// who consolidates gets a second declaration of the same type written beside
// their own, and the build dies on a duplicate definition.
//
// So look for a *declaration of the type* anywhere in the folder instead. The
// keyword list covers all nine target languages:
//
//   class      Java, C#, C++, Python, JS, TS, Swift
//   struct     C++, Rust, Swift, Go (as "type X struct")
//   enum       Java, C#, Rust, Swift, TS
//   interface  Java, C#, TS, Go
//   record     Java, C#
//   protocol   Swift
//   type       Go, TS (also catches Go's "type X struct")
//
// Comments are blanked first, so a mention of the type in prose does not count
// as an implementation, and the keyword prefix means a *use* of the type
// (a field, a List<T>, a parameter) does not match either.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// dropEntriesForMissingFiles — keep a merged index honest.
//
// The index files (common/mod.rs, common/index.js, common/index.ts,
// common/__init__.py, common/common.h, production/mod.rs) are merged with
// whatever is already on disk, so that converting one .spectable does not erase
// the entries belonging to the others. The cost of merging is that nothing ever
// leaves: rename an AttributeSet and the old entry stays forever, naming a file
// that is no longer there.
//
// That is not cosmetic. An index that names a missing module is a hard error in
// every language that has one -- renaming ItemPriceInpt to ItemPriceInput left
// common/index.js exporting ./ItemPriceInptString.js and every Jest suite died
// with "Cannot find module" before a single test ran.
//
// So a carried-over entry is kept only while its file still exists. Apply this
// to the lines read from the existing index, never to the ones about to be
// written -- those name files this run may not have created yet.
//
// `pattern` must capture the file's base name in group 1. A module implemented
// as a directory (base/mod.rs, base/index.js) counts as present.
// ---------------------------------------------------------------------------

inline QStringList dropEntriesForMissingFiles(const QStringList& lines,
                                              const QString& dir,
                                              const QRegularExpression& pattern,
                                              const QString& ext)
{
    if (dir.isEmpty()) return lines;      // nothing to check against

    QStringList kept;
    const QDir d(dir);
    for (const QString& line : lines) {
        const QRegularExpressionMatch m = pattern.match(line);
        if (!m.hasMatch()) { kept << line; continue; }   // not an entry we own
        const QString base = m.captured(1);
        if (QFile::exists(d.filePath(base + ext)) || QDir(d.filePath(base)).exists())
            kept << line;
    }
    return kept;
}

class ProductionScan {
public:
    // nameFilters are the language's source globs, e.g. {"*.cs"}. hashComments
    // selects Python-style stripping; everything else is C-style.
    ProductionScan(const QString& dir, const QStringList& nameFilters,
                   bool hashComments = false)
    {
        if (dir.isEmpty() || !QDir(dir).exists()) return;

        // Folders that only ever hold build output; scanning them is wasted
        // work and could match a copy of a source file.
        static const QStringList skip = {
            "node_modules", "target", "build", "dist", ".build", "obj", "bin",
            "__pycache__", ".git"
        };

        QDirIterator it(dir, nameFilters, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString rel  = QDir(dir).relativeFilePath(path);
            bool skipped = false;
            for (const QString& part : rel.split('/'))
                if (skip.contains(part)) { skipped = true; break; }
            if (skipped) continue;

            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString raw = QString::fromUtf8(f.readAll());
            m_files.append({ path, hashComments ? stripHashComments(raw)
                                                : stripCStyleComments(raw) });
        }
    }

    // Path of the file that declares typeName, or an empty string. Any file
    // whose own name matches the type is reported as-is, so an existing
    // <TypeName>.<ext> still wins and the answer does not depend on scan order.
    QString declaredIn(const QString& typeName) const
    {
        if (typeName.isEmpty()) return QString();

        const QRegularExpression re(
            "\\b(?:class|struct|enum|interface|record|protocol|type)\\s+"
            + QRegularExpression::escape(typeName) + "\\b");

        for (const auto& entry : m_files)
            if (re.match(entry.second).hasMatch())
                return entry.first;
        return QString();
    }

private:
    QVector<QPair<QString, QString>> m_files;   // path, comment-stripped text
};

} // namespace sourcescan
