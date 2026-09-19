#pragma once

#include <QDockWidget>
#include <QFont>
#include "../analyzer/AnalysisResult.h"

class DiffView;
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
    // A message in the Diff tab -- no history, identical, and the like.
    void showDiff(const QString& message, const QString& title);
    // Two whole versions of a file, compared. revertCommit/revertRelPath
    // enable the Revert button.
    void showComparison(const QString& oldText, const QString& newText,
                        const QString& oldTitle, const QString& newTitle,
                        const QString& title,
                        const QString& revertCommit, const QString& revertRelPath,
                        const QString& revertLabel);
    void setCoverageData(const QList<CoverageEntry>& entries);
    void clearBuildOutput();

    void showBuildTab();
    void showAnalysisTab();
    void showFindResultsTab();
    void showDiffTab();
    // Discard the displayed comparison and disable Revert. `reason` is shown in
    // place of the patch, so an empty pane never looks like a failure.
    void clearDiff(const QString& reason = {});
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
    DiffView*     m_diffView      = nullptr;
    QPushButton*  m_revertButton  = nullptr;
    QLabel*       m_revertLabel   = nullptr;
    QString       m_revertCommit;
    QString       m_revertRelPath;
    QTableWidget* m_coverageTable = nullptr;

    QList<Diagnostic> m_diagnostics;
    QList<Diagnostic> m_findResults;
};
