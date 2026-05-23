#pragma once

#include "../analyzer/AnalysisResult.h"
#include <QList>
#include <QString>

class BuildOutputParser
{
public:
    // Parses lines matching "file:line:col: error/warning/info: message"
    static QList<Diagnostic> parse(const QString& output);
};
