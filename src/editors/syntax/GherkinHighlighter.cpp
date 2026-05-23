#include "GherkinHighlighter.h"

GherkinHighlighter::GherkinHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    buildRules();
}

void GherkinHighlighter::addRule(const QString& pattern, const QTextCharFormat& format)
{
    m_rules.append({QRegularExpression(pattern), format});
}

void GherkinHighlighter::buildRules()
{
    // Feature/background/scenario headings
    QTextCharFormat headingFmt;
    headingFmt.setForeground(QColor("#569CD6"));
    headingFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*(Feature:|Background:|Scenario(?: Outline)?:|Examples:).*)", headingFmt);

    // Step keywords
    QTextCharFormat stepFmt;
    stepFmt.setForeground(QColor("#4EC9B0"));
    stepFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*(Given|When|Then|And|But)\b)", stepFmt);

    // Tags
    QTextCharFormat tagFmt;
    tagFmt.setForeground(QColor("#C586C0"));
    addRule(R"(@\w+)", tagFmt);

    // Docstring delimiters
    QTextCharFormat docstringFmt;
    docstringFmt.setForeground(QColor("#CE9178"));
    addRule(R"(^\s*\"\"\")", docstringFmt);

    // Table cell separators
    QTextCharFormat tableFmt;
    tableFmt.setForeground(QColor("#808080"));
    addRule(R"(\|)", tableFmt);

    // Comments — must be last so they override other rules
    QTextCharFormat commentFmt;
    commentFmt.setForeground(QColor("#6A9955"));
    commentFmt.setFontItalic(true);
    addRule(R"(#.*$)", commentFmt);
}

void GherkinHighlighter::highlightBlock(const QString& text)
{
    for (const auto& rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }
}
