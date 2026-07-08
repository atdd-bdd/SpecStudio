#include "TagFilter.h"

#include <QRegularExpression>

// ---------------------------------------------------------------------------
// Tokenizer: splits "smoke AND (NOT wip OR draft)" into tokens,
// treating '(' and ')' as standalone tokens.
// ---------------------------------------------------------------------------
QStringList TagFilter::tokenize(const QString& expr)
{
    QStringList tokens;
    static QRegularExpression re(R"(\(|\)|[^\s()]+)");
    auto it = re.globalMatch(expr.trimmed());
    while (it.hasNext())
        tokens << it.next().captured(0);
    return tokens;
}

// ---------------------------------------------------------------------------
// Recursive descent: or_expr → and_expr ('OR' and_expr)*
// ---------------------------------------------------------------------------
bool TagFilter::Parser::parseOr()
{
    bool result = parseAnd();
    while (peek() == "OR") {
        consume();
        bool rhs = parseAnd();
        result = result || rhs;
    }
    return result;
}

bool TagFilter::Parser::parseAnd()
{
    bool result = parseNot();
    while (peek() == "AND") {
        consume();
        bool rhs = parseNot();
        result = result && rhs;
    }
    return result;
}

bool TagFilter::Parser::parseNot()
{
    if (peek() == "NOT") {
        consume();
        return !parseNot();
    }
    return parsePrimary();
}

bool TagFilter::Parser::parsePrimary()
{
    if (atEnd()) return false;

    if (peek() == "(") {
        consume();              // consume '('
        bool result = parseOr();
        if (peek() == ")") consume();   // consume ')'
        return result;
    }

    QString tag = tokens[pos].toUpper();
    consume();
    if (tag.startsWith('$')) tag = tag.mid(1);
    return tagsUpper.contains(tag);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
bool TagFilter::matches(const QString& filter, const QStringList& generatorTags)
{
    const QString trimmed = filter.trimmed();
    if (trimmed.isEmpty()) return true;

    Parser p;
    p.tokens = tokenize(trimmed);
    p.pos    = 0;
    for (const QString& t : generatorTags)
        p.tagsUpper << t.toUpper();

    return p.parseOr();
}
