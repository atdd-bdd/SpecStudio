#pragma once

#include <QStringList>

// Where one .spectable block starts and ends.
//
// Deliberately free functions over a plain QStringList rather than methods on the
// editor: the rules below are fiddly enough to be worth testing without a widget,
// a document or an event loop.
//
// Blocks are delimited by the *next block keyword*, not by indentation.
// LineNumberEdit::foldEnd uses indentation, which does not work here -- real
// specifications are written flush left, so a Scenario and the steps under it
// share column 0 and an indentation rule would find every block empty.
namespace SpecTableBlocks {

// Whether line `index` opens a block, accounting for docstrings.
bool isBlockHeader(const QStringList& lines, int index);

// The columns spanned by the leading keyword of a block header -- `Scenario` in
// `  Scenario Add two numbers` -- as [startCol, endCol). False when the line does
// not open a block.
//
// Selecting the block is bound to the keyword alone, not the whole header line, so
// that double-clicking the block's *name* still selects a word the way it does
// everywhere else.
bool blockKeywordRange(const QStringList& lines, int index, int* startCol, int* endCol);

// The extent of the block containing line `index`, as inclusive line indices.
// Trailing blank lines are excluded -- they separate blocks rather than belong to
// one. Returns false when `index` sits above the first block, so a caller can
// leave the default behaviour alone instead of selecting something arbitrary.
bool rangeForLine(const QStringList& lines, int index, int* firstLine, int* lastLine);

} // namespace SpecTableBlocks
