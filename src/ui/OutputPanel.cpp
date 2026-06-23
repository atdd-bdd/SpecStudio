#include "OutputPanel.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QListWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextCursor>
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

    m_diffView = new QTextEdit(m_tabs);
    m_diffView->setReadOnly(true);
    m_diffView->setFontFamily("Courier New");

    m_coverageTable = new QTableWidget(0, 7, m_tabs);
    m_coverageTable->setHorizontalHeaderLabels({
        tr("File"), tr("Specification"), tr("Scenarios"),
        tr("Business Rules"), tr("Calculations"), tr("AttributeSets"), tr("Tests Generated")
    });
    m_coverageTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_coverageTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c <= 6; ++c)
        m_coverageTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    m_coverageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_coverageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_coverageTable->verticalHeader()->setVisible(false);

    m_tabs->addTab(m_buildOut,       tr("Build"));
    m_tabs->addTab(m_analysisList,   tr("Analysis"));
    m_tabs->addTab(m_findList,       tr("Find Results"));
    m_tabs->addTab(m_diffView,       tr("Diff"));
    m_tabs->addTab(m_coverageTable,  tr("Coverage"));

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

void OutputPanel::setOutputFont(const QFont& font)
{
    m_buildOut->setFont(font);
    m_diffView->setFont(font);
    m_analysisList->setFont(font);
    m_findList->setFont(font);
    m_coverageTable->setFont(font);
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

void OutputPanel::showDiff(const QString& diffText, const QString& title)
{
    m_diffView->clear();
    m_tabs->setTabText(m_tabs->indexOf(m_diffView),
                       title.isEmpty() ? tr("Diff") : tr("Diff – %1").arg(title));

    QTextCursor c(m_diffView->document());
    QTextCharFormat fmt;
    fmt.setFontFamily("Courier New");

    for (const QString& line : diffText.split('\n')) {
        if (line.startsWith("+++") || line.startsWith("---"))
            fmt.setForeground(QColor(100, 100, 100));
        else if (line.startsWith('+'))
            fmt.setForeground(QColor(0, 140, 0));
        else if (line.startsWith('-'))
            fmt.setForeground(QColor(200, 0, 0));
        else if (line.startsWith("@@"))
            fmt.setForeground(QColor(0, 80, 180));
        else
            fmt.setForeground(Qt::black);

        c.insertText(line + '\n', fmt);
    }

    showDiffTab();
}

void OutputPanel::showDiffTab()
{
    show();
    m_tabs->setCurrentWidget(m_diffView);
}

void OutputPanel::setCoverageData(const QList<CoverageEntry>& entries)
{
    m_coverageTable->setRowCount(0);

    int totalScenarios = 0, totalRules = 0, totalCalcs = 0, totalAttrs = 0;
    int totalGenerated = 0;

    for (const CoverageEntry& e : entries) {
        const int row = m_coverageTable->rowCount();
        m_coverageTable->insertRow(row);

        auto* fileItem = new QTableWidgetItem(QFileInfo(e.filePath).fileName());
        fileItem->setToolTip(e.filePath);
        m_coverageTable->setItem(row, 0, fileItem);
        m_coverageTable->setItem(row, 1, new QTableWidgetItem(e.specName));

        auto numItem = [](int n) {
            auto* it = new QTableWidgetItem(n > 0 ? QString::number(n) : QString());
            it->setTextAlignment(Qt::AlignCenter);
            return it;
        };
        m_coverageTable->setItem(row, 2, numItem(e.scenarios));
        m_coverageTable->setItem(row, 3, numItem(e.businessRules));
        m_coverageTable->setItem(row, 4, numItem(e.calculations));
        m_coverageTable->setItem(row, 5, numItem(e.attrSets));

        auto* genItem = new QTableWidgetItem(e.testsGenerated ? tr("Yes") : tr("No"));
        genItem->setTextAlignment(Qt::AlignCenter);
        genItem->setForeground(e.testsGenerated ? QColor("#4EC9B0") : QColor("#F48771"));
        m_coverageTable->setItem(row, 6, genItem);

        totalScenarios += e.scenarios;
        totalRules     += e.businessRules;
        totalCalcs     += e.calculations;
        totalAttrs     += e.attrSets;
        if (e.testsGenerated) ++totalGenerated;
    }

    // Summary row
    if (!entries.isEmpty()) {
        const int row = m_coverageTable->rowCount();
        m_coverageTable->insertRow(row);

        auto* sumLabel = new QTableWidgetItem(tr("TOTAL (%1 files)").arg(entries.size()));
        QFont bold = sumLabel->font(); bold.setBold(true);
        sumLabel->setFont(bold);
        m_coverageTable->setItem(row, 0, sumLabel);
        m_coverageTable->setItem(row, 1, new QTableWidgetItem());

        auto boldNum = [&bold](int n) {
            auto* it = new QTableWidgetItem(QString::number(n));
            it->setTextAlignment(Qt::AlignCenter);
            it->setFont(bold);
            return it;
        };
        m_coverageTable->setItem(row, 2, boldNum(totalScenarios));
        m_coverageTable->setItem(row, 3, boldNum(totalRules));
        m_coverageTable->setItem(row, 4, boldNum(totalCalcs));
        m_coverageTable->setItem(row, 5, boldNum(totalAttrs));

        const QString genStr = tr("%1 / %2").arg(totalGenerated).arg(entries.size());
        auto* genSum = new QTableWidgetItem(genStr);
        genSum->setTextAlignment(Qt::AlignCenter);
        genSum->setFont(bold);
        const bool allGen = (totalGenerated == entries.size());
        genSum->setForeground(allGen ? QColor("#4EC9B0") : QColor("#CE9178"));
        m_coverageTable->setItem(row, 6, genSum);
    }
}

void OutputPanel::showCoverageTab()
{
    show();
    m_tabs->setCurrentWidget(m_coverageTable);
}
