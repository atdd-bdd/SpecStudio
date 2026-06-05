#include "OutputPanel.h"

#include <QFileInfo>
#include <QListWidget>
#include <QTabWidget>
#include <QTextEdit>

OutputPanel::OutputPanel(QWidget* parent)
    : QDockWidget(tr("Output"), parent)
{
    setObjectName("OutputPanel");
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    m_tabs = new QTabWidget(this);

    m_buildOut = new QTextEdit(m_tabs);
    m_buildOut->setReadOnly(true);
    m_buildOut->setFontFamily("Courier New");

    m_analysisList = new QListWidget(m_tabs);
    connect(m_analysisList, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
                int idx = m_analysisList->row(item);
                if (idx >= 0 && idx < m_diagnostics.size())
                    emit diagnosticActivated(m_diagnostics[idx].filePath,
                                             m_diagnostics[idx].line);
            });

    m_findList = new QListWidget(m_tabs);
    connect(m_findList, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
                int idx = m_findList->row(item);
                if (idx >= 0 && idx < m_findResults.size())
                    emit diagnosticActivated(m_findResults[idx].filePath,
                                             m_findResults[idx].line);
            });

    m_tabs->addTab(m_buildOut,     tr("Build"));
    m_tabs->addTab(m_analysisList, tr("Analysis"));
    m_tabs->addTab(m_findList,     tr("Find Results"));

    setWidget(m_tabs);
}

void OutputPanel::appendBuildOutput(const QString& text)
{
    m_buildOut->append(text);
}

void OutputPanel::setDiagnostics(const QList<Diagnostic>& diagnostics)
{
    m_diagnostics = diagnostics;
    m_analysisList->clear();

    for (const auto& d : diagnostics) {
        QString prefix = d.severity == Diagnostic::Severity::Error ? "E" :
                         d.severity == Diagnostic::Severity::Warning ? "W" : "I";
        QString label = QStringLiteral("[%1] %2 (%3)")
            .arg(prefix, d.message, QFileInfo(d.filePath).fileName());
        m_analysisList->addItem(label);
    }
}

void OutputPanel::clearBuildOutput()
{
    m_buildOut->clear();
}

void OutputPanel::setFindResults(const QList<Diagnostic>& results, const QString& term)
{
    m_findResults = results;
    m_findList->clear();
    m_tabs->setTabText(m_tabs->indexOf(m_findList),
                       tr("Find Results – \"%1\" (%2)").arg(term).arg(results.size()));
    for (const auto& d : results) {
        const QString label = QStringLiteral("%1 (%2):  %3")
            .arg(QFileInfo(d.filePath).fileName())
            .arg(d.line)
            .arg(d.message);
        m_findList->addItem(label);
    }
}

void OutputPanel::showBuildTab()
{
    m_tabs->setCurrentWidget(m_buildOut);
}

void OutputPanel::showAnalysisTab()
{
    m_tabs->setCurrentWidget(m_analysisList);
}

void OutputPanel::showFindResultsTab()
{
    show();
    m_tabs->setCurrentWidget(m_findList);
}
