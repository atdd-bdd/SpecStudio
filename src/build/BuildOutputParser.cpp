#include "BuildOutputParser.h"

#include <QRegularExpression>

QList<Diagnostic> BuildOutputParser::parse(const QString& output)
{
    QList<Diagnostic> diags;

    // Format 1 — MSVC / GCC style: <file>:<line>:<col>: <severity>: <message>
    static QRegularExpression reCompiler(
        R"(^(.+?):(\d+):(\d+):\s*(error|warning|info|note):\s*(.+)$)",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);

    auto it = reCompiler.globalMatch(output);
    while (it.hasNext()) {
        auto m = it.next();
        Diagnostic d;
        d.filePath = m.captured(1);
        d.line     = m.captured(2).toInt();
        d.column   = m.captured(3).toInt();
        d.message  = m.captured(5);

        QString sev = m.captured(4).toLower();
        d.severity = (sev == "error")   ? Diagnostic::Severity::Error
                   : (sev == "warning") ? Diagnostic::Severity::Warning
                                        : Diagnostic::Severity::Info;
        diags.append(d);
    }

    // Format 2 — SpecTableConverter: ERROR|WARNING|INFO:LINE:message
    static QRegularExpression reConverter(
        R"(^(ERROR|WARNING|INFO):(\d+):(.+)$)",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);

    auto it2 = reConverter.globalMatch(output);
    while (it2.hasNext()) {
        auto m = it2.next();
        Diagnostic d;
        d.filePath = {};           // no file path in this format
        d.line     = m.captured(2).toInt();
        d.column   = 0;
        d.message  = m.captured(3).trimmed();

        QString sev = m.captured(1).toUpper();
        d.severity = (sev == "ERROR")   ? Diagnostic::Severity::Error
                   : (sev == "WARNING") ? Diagnostic::Severity::Warning
                                        : Diagnostic::Severity::Info;
        diags.append(d);
    }

    return diags;
}
