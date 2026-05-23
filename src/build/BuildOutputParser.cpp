#include "BuildOutputParser.h"

#include <QRegularExpression>

QList<Diagnostic> BuildOutputParser::parse(const QString& output)
{
    QList<Diagnostic> diags;

    // Pattern: <file>:<line>:<col>: <severity>: <message>
    static QRegularExpression re(
        R"(^(.+?):(\d+):(\d+):\s*(error|warning|info|note):\s*(.+)$)",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);

    auto it = re.globalMatch(output);
    while (it.hasNext()) {
        auto m = it.next();
        Diagnostic d;
        d.filePath = m.captured(1);
        d.line     = m.captured(2).toInt();
        d.column   = m.captured(3).toInt();
        d.message  = m.captured(5);

        QString sev = m.captured(4).toLower();
        if (sev == "error")
            d.severity = Diagnostic::Severity::Error;
        else if (sev == "warning")
            d.severity = Diagnostic::Severity::Warning;
        else
            d.severity = Diagnostic::Severity::Info;

        diags.append(d);
    }

    return diags;
}
