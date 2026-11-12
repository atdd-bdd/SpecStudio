#include "SpecTableHighlighter.h"

#include "../../spell/SpellChecker.h"

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
    // Not a rule: a rule sees the whole line and would colour the word
    // wherever it stands. A built-in type name is only a type where a type can
    // be written, so highlightBuiltinTypes decides that from the shape of the
    // line instead.
    m_builtinFmt.setForeground(QColor("#4FC1FF")); // light blue

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

    highlightBuiltinTypes(text);

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

    checkSpelling(text);
}

// ---------------------------------------------------------------------------
// Built-in type names
// ---------------------------------------------------------------------------
//
// "String" is a type in a column of a table, and in the attribute set a step
// names after its colon. Anywhere else it is an ordinary English word:
// "Scenario Define with a String", "Calculation Empty String Returns Zero",
// "Calculation Heading Text Is Parsed", "Define aString = a String". Colouring
// it there told the reader it meant something it did not, so the line has to
// be read for its shape before any word in it is coloured.
//
// Only these two shapes, then: a table row, and the type reference at the end
// of a line, where the modifiers may follow it.

void SpecTableHighlighter::highlightBuiltinTypes(const QString& text)
{
    const QString builtins =
        R"((Character|String|Text|Integer|Float|Scientific|Decimal|Boolean|Date|Time|DateTime|Duration|YesNo))";
    static const QRegularExpression inCell(R"(\b)" + builtins + R"(\b)");
    static const QRegularExpression afterColon(
        R"(:\s*)" + builtins + R"(\b(?:\s+(?:Vertical|CompareOnly|EveryCell))*\s*$)");

    if (text.trimmed().startsWith('|')) {
        QRegularExpressionMatchIterator it = inCell.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(1), m.capturedLength(1), m_builtinFmt);
        }
        return;
    }

    const QRegularExpressionMatch m = afterColon.match(text);
    if (m.hasMatch())
        setFormat(m.capturedStart(1), m.capturedLength(1), m_builtinFmt);
}

// ---------------------------------------------------------------------------
// Spelling
// ---------------------------------------------------------------------------
//
// Every run of letters is a candidate: step text, names, comments, cells.
// What is left out is what is not English by nature -- a file name inside
// quotes, an abbreviation in capitals, anything with a digit in it -- and a
// docstring, which never reaches here. A camel-cased name is checked one word
// at a time, so TotalScore passes and TotlScore is marked at "Totl".

void SpecTableHighlighter::checkSpelling(const QString& text)
{
    SpellChecker* checker = SpellChecker::instance();
    if (!checker->isEnabled() || !checker->isAvailable()) return;

    static const QRegularExpression reQuoted(R"("[^"]*")");
    static const QRegularExpression reToken(R"([A-Za-z][A-Za-z']*)");

    QVector<QPair<int, int>> quoted;
    auto qit = reQuoted.globalMatch(text);
    while (qit.hasNext()) {
        const auto m = qit.next();
        quoted.append({ m.capturedStart(), m.capturedEnd() });
    }
    auto inQuotes = [&](int pos) {
        for (const auto& q : quoted)
            if (pos >= q.first && pos < q.second) return true;
        return false;
    };

    auto it = reToken.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const int start = m.capturedStart();
        if (inQuotes(start)) continue;
        const QString token = m.captured();
        if (!SpellChecker::isCheckable(token)) continue;
        // A declared name is spelled the way it is declared.
        if (checker->isKnownName(token)) continue;

        for (const SpellChecker::Part& part : SpellChecker::splitCamelCase(token)) {
            QString word = part.text;
            // A possessive or a trailing quote is not part of the word.
            if (word.endsWith("'s")) word.chop(2);
            while (word.endsWith(QLatin1Char('\''))) word.chop(1);
            if (!SpellChecker::isCheckable(word) || checker->isCorrect(word)) continue;

            QTextCharFormat f = format(start + part.offset);
            f.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
            f.setUnderlineColor(Qt::red);
            setFormat(start + part.offset, part.length, f);
        }
    }
}
