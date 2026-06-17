#include "SpecTableHighlighter.h"

SpecTableHighlighter::SpecTableHighlighter(QTextDocument* parent)
    : GherkinHighlighter(parent)
{
    m_rules.clear();
    buildRules();
}

void SpecTableHighlighter::buildRules()
{
    // --- Block-level declaration keywords ---
    QTextCharFormat declFmt;
    declFmt.setForeground(QColor("#569CD6")); // VS blue
    declFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*(Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Constraint|Import|Insert)\b)",
            declFmt);

    // --- Step keywords ---
    QTextCharFormat stepFmt;
    stepFmt.setForeground(QColor("#4EC9B0")); // VS teal
    stepFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*(Given|When|Then|And|But)\b)", stepFmt);

    // --- "applying BusinessRule" / "applying Calculation" modifier ---
    QTextCharFormat applyFmt;
    applyFmt.setForeground(QColor("#4EC9B0"));
    addRule(R"(\bapplying\b)", applyFmt);

    // --- AttributeSet reference after colon  ": SomeName" ---
    QTextCharFormat attrRefFmt;
    attrRefFmt.setForeground(QColor("#DCDCAA")); // VS yellow
    addRule(R"(:\s*(\w+)\s*$)", attrRefFmt);

    // --- Description / annotation lines starting with * ---
    QTextCharFormat descFmt;
    descFmt.setForeground(QColor("#6A9955")); // VS green
    descFmt.setFontItalic(true);
    addRule(R"(^\s*\*.*$)", descFmt);

    // --- Quoted strings (file paths in Import/Insert) ---
    QTextCharFormat stringFmt;
    stringFmt.setForeground(QColor("#CE9178")); // VS orange
    addRule(R"("[^"]*")", stringFmt);

    // --- Table pipe separators ---
    QTextCharFormat pipeFmt;
    pipeFmt.setForeground(QColor("#808080"));
    addRule(R"(\|)", pipeFmt);

    // --- In / Out / In-Out markers in attribute tables ---
    QTextCharFormat dirFmt;
    dirFmt.setForeground(QColor("#C586C0")); // VS purple
    addRule(R"(\b(In-Out|In|Out)\b)", dirFmt);

    // --- Valid / Yes / No in DataType tables ---
    QTextCharFormat validFmt;
    validFmt.setForeground(QColor("#4EC9B0"));
    addRule(R"(\b(Valid|Yes|No)\b)", validFmt);

    // --- Section headers  # ... ---
    QTextCharFormat sectionFmt;
    sectionFmt.setForeground(QColor("#C586C0"));
    sectionFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*#.*$)", sectionFmt);
}

void SpecTableHighlighter::highlightBlock(const QString& text)
{
    // Apply rules in order (later rules can override earlier ones for same span)
    for (const auto& rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            // Use captured group 1 if present (to colour only the keyword, not leading whitespace)
            int start  = m.capturedStart(m.lastCapturedIndex() > 0 ? 1 : 0);
            int length = m.capturedLength(m.lastCapturedIndex() > 0 ? 1 : 0);
            if (start >= 0 && length > 0)
                setFormat(start, length, rule.format);
        }
    }
}
