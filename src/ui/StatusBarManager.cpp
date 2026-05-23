#include "StatusBarManager.h"

#include <QLabel>
#include <QStatusBar>

StatusBarManager::StatusBarManager(QStatusBar* statusBar, QObject* parent)
    : QObject(parent)
    , m_statusBar(statusBar)
{
    m_solutionLabel = new QLabel(tr("No solution open"), statusBar);
    m_branchLabel   = new QLabel(statusBar);
    m_posLabel      = new QLabel(statusBar);

    statusBar->addWidget(m_solutionLabel, 1);
    statusBar->addPermanentWidget(m_branchLabel);
    statusBar->addPermanentWidget(m_posLabel);
}

void StatusBarManager::setSolutionName(const QString& name)
{
    m_solutionLabel->setText(name.isEmpty() ? tr("No solution open") : name);
}

void StatusBarManager::setGitBranch(const QString& branch)
{
    m_branchLabel->setText(branch.isEmpty() ? QString() : QStringLiteral("[%1]").arg(branch));
}

void StatusBarManager::setLineColumn(int line, int column)
{
    m_posLabel->setText(QStringLiteral("Ln %1, Col %2").arg(line).arg(column));
}

void StatusBarManager::clearAll()
{
    setSolutionName(QString());
    setGitBranch(QString());
    m_posLabel->clear();
}
