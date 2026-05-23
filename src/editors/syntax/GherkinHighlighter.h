#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class GherkinHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit GherkinHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };

    QVector<Rule> m_rules;

    void addRule(const QString& pattern, const QTextCharFormat& format);
    void buildRules();
};
