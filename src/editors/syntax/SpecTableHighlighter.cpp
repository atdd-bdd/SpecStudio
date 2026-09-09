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
    addRule(R"(^\s*(Specification|Entity|Collection|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
            declFmt);

    // --- Examples: <AttributeSet> — highlighted as declaration keyword ---
    addRule(R"(^\s*(Examples:))", declFmt);

    // --- Named comment keywords (Description, Details, Constraint, Uses) ---
    QTextCharFormat namedCommentFmt;
    namedCommentFmt.setForeground(QColor("#6A9955")); // VS green
    namedCommentFmt.setFontItalic(true);
    addRule(R"(^\s*(Description|Details|Constraint|Uses)\b)", namedCommentFmt);

    // --- Step keywords ---
    QTextCharFormat stepFmt;
    stepFmt.setForeground(QColor("#4EC9B0")); // VS teal
    stepFmt.setFontWeight(QFont::Bold);
    addRule(R"(^\s*(Given|When|Then|And|WhenThen)\b)", stepFmt);

    // --- "applying BusinessRule" / "applying Calculation" modifier ---
    QTextCharFormat applyFmt;
    applyFmt.setForeground(QColor("#4EC9B0"));
    addRule(R"(\bapplying\b)", applyFmt);

    // --- Vertical modifier keyword ---
    QTextCharFormat modFmt;
    modFmt.setForeground(QColor("#C586C0")); // VS purple
    modFmt.setFontWeight(QFont::Bold);
    addRule(R"(\b(Vertical|CompareOnly)\b)", modFmt);

    // --- Built-in DataType names ---
    QTextCharFormat builtinFmt;
    builtinFmt.setForeground(QColor("#4FC1FF")); // light blue
    addRule(R"(\b(Character|String|Text|Integer|Float|Scientific|Decimal|Boolean|Date|Time|DateTime|Duration|YesNo)\b)",
            builtinFmt);

    // --- Built-in AttributeSet names ---
    QTextCharFormat builtinAttrFmt;
    builtinAttrFmt.setForeground(QColor("#4EC9B0")); // teal
    builtinAttrFmt.setFontWeight(QFont::Bold);
    addRule(R"(\b(EnumerationValues|ValidValues)\b)", builtinAttrFmt);

    // --- AttributeSet reference after colon  ": SomeName [Vertical]" ---
    QTextCharFormat attrRefFmt;
    attrRefFmt.setForeground(QColor("#808080")); // dark grey
    addRule(R"(:\s*(\w+)(?:\s+(?:Vertical|CompareOnly))?\s*$)", attrRefFmt);

    // --- Value references  =Name ---
    QTextCharFormat valueRefFmt;
    valueRefFmt.setForeground(QColor("#CE9178")); // VS orange
    addRule(R"(=[A-Za-z_]\w*)", valueRefFmt);

    // --- Line continuation marker \ at end of Details lines ---
    QTextCharFormat contFmt;
    contFmt.setForeground(QColor("#808080"));
    addRule(R"(\\$)", contFmt);

    // --- Legacy description lines starting with * ---
    QTextCharFormat descFmt;
    descFmt.setForeground(QColor("#6A9955")); // VS green
    descFmt.setFontItalic(true);
    addRule(R"(^\s*\*.*$)", descFmt);

    // --- Quoted strings (file paths in Import/Insert/Define) ---
    // Deliberately not matched on a """ line: this pattern would take the first
    // two quotes as an empty string and leave the third uncoloured. Docstrings
    // are handled by block state in highlightBlock instead.
    QTextCharFormat stringFmt;
    stringFmt.setForeground(QColor("#CE9178")); // VS orange
    addRule(R"("[^"]*")", stringFmt);

    m_docStringFmt.setForeground(QColor("#CE9178")); // the same orange

    // --- Table pipe separators ---
    QTextCharFormat pipeFmt;
    pipeFmt.setForeground(QColor("#808080"));
    addRule(R"(\|)", pipeFmt);

    // --- In / Out / In-Out markers in attribute tables ---
    QTextCharFormat dirFmt;
    dirFmt.setForeground(QColor("#C586C0")); // VS purple
    addRule(R"(\b(In-Out|In|Out)\b)", dirFmt);

    // --- Valid / Yes / No / True / False in tables ---
    QTextCharFormat validFmt;
    validFmt.setForeground(QColor("#4EC9B0"));
    addRule(R"(\b(Valid|Yes|No|True|False|true|false)\b)", validFmt);

    // --- ~ space placeholder in table cells ---
    QTextCharFormat tildeFmt;
    tildeFmt.setForeground(QColor("#C586C0")); // VS purple
    tildeFmt.setFontWeight(QFont::Bold);
    addRule(R"(~)", tildeFmt);

    // --- Unnamed comments # ... (inline or full-line) — must be last ---
    QTextCharFormat commentFmt;
    commentFmt.setForeground(QColor("#6A9955")); // VS green
    commentFmt.setFontItalic(true);
    addRule(R"(#.*$)", commentFmt);
}

void SpecTableHighlighter::highlightBlock(const QString& text)
{
    // A docstring runs from a line that is exactly """ to the next such line.
    // Everything between is literal text, so no keyword inside it is coloured,
    // and the delimiters are coloured whole — the quoted-string rule would
    // otherwise take two of the three quotes and leave the third bare.
    static const QRegularExpression reDocDelimiter(R"(^\s*"""\s*$)");
    const bool inDocString = (previousBlockState() == InDocString);
    const bool isDelimiter = reDocDelimiter.match(text).hasMatch();

    if (inDocString || isDelimiter) {
        setFormat(0, text.length(), m_docStringFmt);
        // A delimiter closes the docstring it is in, and opens one otherwise.
        setCurrentBlockState(isDelimiter ? (inDocString ? Normal : InDocString)
                                         : InDocString);
        return;
    }
    setCurrentBlockState(Normal);

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
