#include "CompletionLists.h"

#include <QSet>

QStringList specTableCompletionWords(const SpecTableSymbols& syms,
                                     const QStringList&      staticKeywords)
{
    QStringList words = staticKeywords;

    for (const QString& n : syms.entities.keys())      words << n;
    for (const QString& n : syms.attributes.keys())    words << n;
    for (const QString& n : syms.dataTypes.keys())     words << n;
    for (const QString& n : syms.businessRules.keys()) words << n;
    for (const QString& n : syms.calculations.keys())  words << n;
    for (const QString& n : syms.domainTerms.keys())   words << n;
    for (const QString& n : syms.defines.keys())       words << ("=" + n);

    words.removeDuplicates();
    return words;
}

QStringList specTableAttrSetWords(const SpecTableSymbols& syms)
{
    QStringList attrSets;
    for (const QString& n : syms.entities.keys())   attrSets << n;
    for (const QString& n : syms.attributes.keys()) attrSets << n;
    attrSets.sort(Qt::CaseInsensitive);
    attrSets.removeDuplicates();
    return attrSets;
}

QStringList specTableTypeWords(const SpecTableSymbols& syms)
{
    static const QStringList builtInTypes = {
        "Boolean", "Character", "Date", "DateTime", "Decimal", "Duration",
        "Float", "Integer", "Scientific", "String", "Text", "Time", "YesNo"
    };
    QStringList typeWords = builtInTypes;
    for (const QString& n : syms.dataTypes.keys()) typeWords << n;
    typeWords.sort(Qt::CaseInsensitive);
    typeWords.removeDuplicates();
    return typeWords;
}

QStringList harvestStepPhrases(const QString& documentText,
                               const QStringList& alreadyOffered)
{
    // "whenthen " is listed separately because "when " does not prefix it;
    // "but " is here for .feature/.featurex, which do have But.
    static const QStringList stepPrefixes = {
        "given ", "when ", "whenthen ", "then ", "and ", "but "
    };

    QSet<QString> seen;
    for (const QString& w : alreadyOffered)
        seen.insert(w.toLower());

    QStringList phrases;
    const QStringList lines = documentText.split('\n');
    for (const QString& raw : lines) {
        const QString line  = raw.trimmed();
        const QString lower = line.toLower();
        if (line.isEmpty() || seen.contains(lower)) continue;
        for (const QString& p : stepPrefixes) {
            if (lower.startsWith(p)) {
                seen.insert(lower);
                phrases.append(line);
                break;
            }
        }
    }
    return phrases;
}
