#pragma once

#include <QString>

struct Diagnostic
{
    enum class Severity { Info, Warning, Error };

    QString  filePath;
    int      line       = 0;
    int      column     = 0;
    QString  message;
    Severity severity   = Severity::Error;
    QString  projectName; // set by AppController; used for grouping in the Analysis tree

    QString severityString() const {
        switch (severity) {
        case Severity::Info:    return "info";
        case Severity::Warning: return "warning";
        default:                return "error";
        }
    }

    QString toString() const {
        return QStringLiteral("%1:%2:%3: %4: %5")
            .arg(filePath).arg(line).arg(column)
            .arg(severityString(), message);
    }
};
