#include "OutputPanel.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QHeaderView>
#include <QListWidget>
#include <QMenu>
#include <QShortcut>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>

static void addCopySupport(QListWidget* list)
{
    auto copySelected = [list]() {
        QStringList lines;
        for (auto* item : list->selectedItems())
            lines << item->text();
        if (!lines.isEmpty())
            QApplication::clipboard()->setText(lines.join('\n'));
    };

    auto* shortcut = new QShortcut(QKeySequence::Copy, list);
    QObject::connect(shortcut, &QShortcut::activated, list, copySelected);

    list->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(list, &QListWidget::customContextMenuRequested,
                     list, [list, copySelected](const QPoint& pos) {
        QMenu menu(list);
        auto* act = menu.addAction(QObject::tr("Copy"));
        act->setShortcut(QKeySequence::Copy);
        act->setEnabled(!list->selectedItems().isEmpty());
        QObject::connect(act, &QAction::triggered, list, copySelected);
        menu.exec(list->mapToGlobal(pos));
    });
}

static void addTreeCopySupport(QTreeWidget* tree)
{
    auto copySelected = [tree]() {
        QStringList lines;
        for (auto* item : tree->selectedItems())
            lines << item->text(0);
        if (!lines.isEmpty())
            QApplication::clipboard()->setText(lines.join('\n'));
    };

    auto* shortcut = new QShortcut(QKeySequence::Copy, tree);
    QObject::connect(shortcut, &QShortcut::activated, tree, copySelected);

    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(tree, &QTreeWidget::customContextMenuRequested,
                     tree, [tree, copySelected](const QPoint& pos) {
        QMenu menu(tree);
        auto* act = menu.addAction(QObject::tr("Copy"));
        act->setEnabled(!tree->selectedItems().isEmpty());
        QObject::connect(act, &QAction::triggered, tree, copySelected);
        menu.exec(tree->mapToGlobal(pos));
    });
}

OutputPanel::OutputPanel(QWidget* parent)
    : QDockWidget(tr("Output"), parent)
{
    setObjectName("OutputPanel");
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    m_tabs = new QTabWidget(this);

    m_buildOut = new QTextEdit(m_tabs);
    m_buildOut->setReadOnly(true);
    m_buildOut->setFontFamily("Courier New");

    m_analysisTree = new QTreeWidget(m_tabs);
    m_analysisTree->setHeaderHidden(true);
    m_analysisTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_analysisTree->setRootIsDecorated(true);
    connect(m_analysisTree, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem* item, int) {
                const int idx = item->data(0, Qt::UserRole).toInt();
                if (idx >= 0 && idx < m_diagnostics.size())
                    emit diagnosticActivated(m_diagnostics[idx].filePath,
                                             m_diagnostics[idx].line);
            });
    addTreeCopySupport(m_analysisTree);

    m_findList = new QListWidget(m_tabs);
    m_findList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_findList, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
                int idx = m_findList->row(item);
                if (idx >= 0 && idx < m_findResults.size())
                    emit diagnosticActivated(m_findResults[idx].filePath,
                                             m_findResults[idx].line);
            });
    addCopySupport(m_findList);

    // The diff tab is a container, not a bare view: it carries a Revert button
    // above the patch. The button is enabled only while a specific earlier
    // version is on show, so it always has something definite to revert to.
    m_diffPage = new QWidget(m_tabs);
    auto* diffLayout = new QVBoxLayout(m_diffPage);
    diffLayout->setContentsMargins(0, 0, 0, 0);
    diffLayout->setSpacing(2);

    auto* diffBar = new QHBoxLayout();
    diffBar->setContentsMargins(4, 2, 4, 0);
    m_revertButton = new QPushButton(tr("Revert to This Version"), m_diffPage);
    m_revertButton->setEnabled(false);
    m_revertButton->setToolTip(
        tr("Load this version into the editor. Nothing is written to disk or "
           "committed until you save, and Undo puts it back."));
    m_revertLabel = new QLabel(m_diffPage);
    m_revertLabel->setEnabled(false);
    diffBar->addWidget(m_revertButton);
    diffBar->addWidget(m_revertLabel, 1);
    diffLayout->addLayout(diffBar);

    m_diffView = new QTextEdit(m_diffPage);
    m_diffView->setReadOnly(true);
    m_diffView->setFontFamily("Courier New");
    diffLayout->addWidget(m_diffView, 1);

    connect(m_revertButton, &QPushButton::clicked, this, [this] {
        if (!m_revertCommit.isEmpty() && !m_revertRelPath.isEmpty())
            emit revertRequested(m_revertCommit, m_revertRelPath);
    });

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
    m_tabs->addTab(m_analysisTree,   tr("Analysis"));
    m_tabs->addTab(m_findList,       tr("Find Results"));
    m_tabs->addTab(m_diffPage,       tr("Diff"));
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
    m_analysisTree->clear();

    auto makeLabel = [](const Diagnostic& d) {
        const QString prefix = d.severity == Diagnostic::Severity::Error   ? "E" :
                               d.severity == Diagnostic::Severity::Warning  ? "W" : "I";
        const QString file = QFileInfo(d.filePath).fileName();
        return d.line > 0
            ? QStringLiteral("[%1] %2 (%3:%4)").arg(prefix, d.message, file).arg(d.line)
            : QStringLiteral("[%1] %2 (%3)").arg(prefix, d.message, file);
    };

    // Collect unique project names in insertion order
    QStringList projectOrder;
    QMap<QString, QList<int>> byProject;
    for (int i = 0; i < diagnostics.size(); ++i) {
        const QString proj = diagnostics[i].projectName.isEmpty()
            ? tr("(Project)") : diagnostics[i].projectName;
        if (!byProject.contains(proj)) projectOrder << proj;
        byProject[proj].append(i);
    }

    const bool multiProject = byProject.size() > 1;

    if (multiProject) {
        for (const QString& projName : projectOrder) {
            auto* projItem = new QTreeWidgetItem(m_analysisTree);
            projItem->setText(0, projName);
            projItem->setData(0, Qt::UserRole, -1); // project node — not navigable
            projItem->setFlags(projItem->flags() & ~Qt::ItemIsSelectable);
            QFont bold = projItem->font(0);
            bold.setBold(true);
            projItem->setFont(0, bold);

            for (int idx : byProject[projName]) {
                auto* item = new QTreeWidgetItem(projItem);
                item->setText(0, makeLabel(diagnostics[idx]));
                item->setData(0, Qt::UserRole, idx);
            }
            projItem->setExpanded(true);
        }
    } else {
        for (int i = 0; i < diagnostics.size(); ++i) {
            auto* item = new QTreeWidgetItem(m_analysisTree);
            item->setText(0, makeLabel(diagnostics[i]));
            item->setData(0, Qt::UserRole, i);
        }
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
    m_analysisTree->setFont(font);
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
    m_tabs->setCurrentWidget(m_analysisTree);
}

void OutputPanel::showFindResultsTab()
{
    show();
    m_tabs->setCurrentWidget(m_findList);
}

void OutputPanel::showDiff(const QString& diffText, const QString& title,
                            const QString& revertCommit, const QString& revertRelPath,
                            const QString& revertLabel)
{
    // Set on every call, so a plain diff clears a revert target left by an
    // earlier one and the button can never act on a version no longer shown.
    m_revertCommit  = revertCommit;
    m_revertRelPath = revertRelPath;
    m_revertButton->setEnabled(!revertCommit.isEmpty() && !revertRelPath.isEmpty());
    m_revertLabel->setText(revertLabel);

    m_diffView->clear();
    m_tabs->setTabText(m_tabs->indexOf(m_diffPage),
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
    m_tabs->setCurrentWidget(m_diffPage);
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
