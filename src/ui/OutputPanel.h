#pragma once

#include <QDockWidget>
#include <QFont>
#include "../analyzer/AnalysisResult.h"

class QTabWidget;
class QTextEdit;
class QPushButton;
class QLabel;
class QListWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

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
    // revertCommit/revertRelPath enable the Revert button; omit them for a
    // diff with nothing specific to revert to.
    void showDiff(const QString& diffText, const QString& title,
                  const QString& revertCommit = {}, const QString& revertRelPath = {},
                  const QString& revertLabel = {});
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
    void revertRequested(const QString& commit, const QString& relativeFilePath);

private:
    QTabWidget*   m_tabs          = nullptr;
    QTextEdit*    m_buildOut      = nullptr;
    QTreeWidget*  m_analysisTree  = nullptr;
    QListWidget*  m_findList      = nullptr;
    QWidget*      m_diffPage      = nullptr;
    QTextEdit*    m_diffView      = nullptr;
    QPushButton*  m_revertButton  = nullptr;
    QLabel*       m_revertLabel   = nullptr;
    QString       m_revertCommit;
    QString       m_revertRelPath;
    QTableWidget* m_coverageTable = nullptr;

    QList<Diagnostic> m_diagnostics;
    QList<Diagnostic> m_findResults;
};
