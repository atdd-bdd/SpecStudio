#pragma once

#include <QDockWidget>
#include <QFont>
#include "../analyzer/AnalysisResult.h"

class QTabWidget;
class QTextEdit;
class QListWidget;
class QTableWidget;

struct CoverageEntry {
    QString filePath;
    QString specName;
    int     scenarios     = 0;
    int     businessRules = 0;
    int     calculations  = 0;
    int     attrSets      = 0;
    bool    testsGenerated = false;
};

class OutputPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit OutputPanel(QWidget* parent = nullptr);

    void appendBuildOutput(const QString& text);
    void setDiagnostics(const QList<Diagnostic>& diagnostics);
    void setFindResults(const QList<Diagnostic>& results, const QString& term);
    void showDiff(const QString& diffText, const QString& title);
    void setCoverageData(const QList<CoverageEntry>& entries);
    void clearBuildOutput();

    void showBuildTab();
    void showAnalysisTab();
    void showFindResultsTab();
    void showDiffTab();
    void showCoverageTab();

    void setOutputFont(const QFont& font);

signals:
    void diagnosticActivated(const QString& filePath, int line);

private:
    QTabWidget*   m_tabs          = nullptr;
    QTextEdit*    m_buildOut      = nullptr;
    QListWidget*  m_analysisList  = nullptr;
    QListWidget*  m_findList      = nullptr;
    QTextEdit*    m_diffView      = nullptr;
    QTableWidget* m_coverageTable = nullptr;

    QList<Diagnostic> m_diagnostics;
    QList<Diagnostic> m_findResults;
};
