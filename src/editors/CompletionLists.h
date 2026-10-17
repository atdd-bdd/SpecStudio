#pragma once

// The suggestion lists the .spectable editor offers, built away from the widget
// so they can be printed from a terminal (tools/complete_cli) instead of only
// seen by typing in the IDE.
//
// Three of the four lists come from the project index, so they are only as
// current as the last rebuild -- Analyze and File > Save both rebuild and then
// call SpecTableEditor::refreshDynamicCompletions(). The fourth, the step
// phrases, is harvested from the document text itself, so it is always current
// and is scoped to one Specification per file.

#include "../analyzer/SpecTableIndex.h"

#include <QStringList>

// Everything the general-purpose popup offers: the static keywords plus every
// declared name in the project, with "=" in front of each Define.
QStringList specTableCompletionWords(const SpecTableSymbols& syms,
                                     const QStringList&      staticKeywords);

// The step-colon dropdown (Given/When/Then/Examples: <name>): attribute sets
// and entities only. DataTypes belong to the Type column, not here.
QStringList specTableAttrSetWords(const SpecTableSymbols& syms);

// The Type column dropdown in an Attributes/Entity table: built-in DataTypes
// plus the project's own.
QStringList specTableTypeWords(const SpecTableSymbols& syms);

// The step phrases already written in one document, in the order they appear:
// what the editor offers once two characters of a step have been typed.
QStringList harvestStepPhrases(const QString& documentText,
                               const QStringList& alreadyOffered = {});
