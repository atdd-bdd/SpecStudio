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
    void setFindResults(const QList<Diagnostic>& results, const QString& term);
    void clearBuildOutput();

    void showBuildTab();
    void showAnalysisTab();
    void showFindResultsTab();

signals:
    void diagnosticActivated(const QString& filePath, int line);

private:
    QTabWidget*  m_tabs         = nullptr;
    QTextEdit*   m_buildOut     = nullptr;
    QListWidget* m_analysisList = nullptr;
    QListWidget* m_findList     = nullptr;

    QList<Diagnostic> m_diagnostics;
    QList<Diagnostic> m_findResults;
};
