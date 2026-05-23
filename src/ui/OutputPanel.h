#pragma once

#include <QDockWidget>
#include "../analyzer/AnalysisResult.h"

class QTabWidget;
class QTextEdit;
class QListWidget;

class OutputPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit OutputPanel(QWidget* parent = nullptr);

    void appendBuildOutput(const QString& text);
    void setDiagnostics(const QList<Diagnostic>& diagnostics);
    void clearBuildOutput();

    void showBuildTab();
    void showAnalysisTab();

signals:
    void diagnosticActivated(const QString& filePath, int line);

private:
    QTabWidget*  m_tabs        = nullptr;
    QTextEdit*   m_buildOut    = nullptr;
    QListWidget* m_analysisList = nullptr;

    QList<Diagnostic> m_diagnostics;
};
