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
    addRule(R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Examples)\b)",
            declFmt);

    // --- Named comment keywords (Description, Details, Constraint) ---
    QTextCharFormat namedCommentFmt;
    namedCommentFmt.setForeground(QColor("#6A9955")); // VS green
    namedCommentFmt.setFontItalic(true);
    addRule(R"(^\s*(Description|Details|Constraint)\b)", namedCommentFmt);

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
    attrRefFmt.setForeground(QColor("#808080")); // dark grey
    addRule(R"(:\s*(\w+)\s*$)", attrRefFmt);

    // --- Line continuation marker \ at end of Details lines ---
    QTextCharFormat contFmt;
    contFmt.setForeground(QColor("#808080"));
    addRule(R"(\\$)", contFmt);

    // --- Legacy description lines starting with * ---
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

    // --- Unnamed comments # ... (inline or full-line) — must be last ---
    QTextCharFormat commentFmt;
    commentFmt.setForeground(QColor("#6A9955")); // VS green
    commentFmt.setFontItalic(true);
    addRule(R"(#.*$)", commentFmt);
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
