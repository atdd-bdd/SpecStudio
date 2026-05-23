#include "FeatureXHighlighter.h"

FeatureXHighlighter::FeatureXHighlighter(QTextDocument* parent)
    : GherkinHighlighter(parent)
{
    // Data keyword
    QTextCharFormat dataFmt;
    dataFmt.setForeground(QColor("#DCDCAA"));
    dataFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*Data\b)", dataFmt);

    // import keyword
    QTextCharFormat importFmt;
    importFmt.setForeground(QColor("#C586C0"));
    importFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*import\s+\"[^\"]*\")", importFmt);

    // String literals
    QTextCharFormat stringFmt;
    stringFmt.setForeground(QColor("#CE9178"));
    addRule(R"(\"[^\"]*\")", stringFmt);
}
