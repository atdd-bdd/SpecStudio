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

    // Format 2 — SpecTableConverter: FILE:<path> markers + ERROR|WARNING|INFO:LINE:message
    {
        static QRegularExpression reFile(R"(^FILE:(.+)$)");
        static QRegularExpression reMsg(R"(^(ERROR|WARNING|INFO):(\d+):(.+)$)",
                                        QRegularExpression::CaseInsensitiveOption);
        QString currentFile;
        for (const QString& line : output.split('\n')) {
            auto fm = reFile.match(line.trimmed());
            if (fm.hasMatch()) { currentFile = fm.captured(1).trimmed(); continue; }
            auto m = reMsg.match(line.trimmed());
            if (!m.hasMatch()) continue;
            Diagnostic d;
            d.filePath = currentFile;
            d.line     = m.captured(2).toInt();
            d.column   = 0;
            d.message  = m.captured(3).trimmed();
            const QString sev = m.captured(1).toUpper();
            d.severity = (sev == "ERROR")   ? Diagnostic::Severity::Error
                       : (sev == "WARNING") ? Diagnostic::Severity::Warning
                                            : Diagnostic::Severity::Info;
            diags.append(d);
        }
    }

    return diags;
}
