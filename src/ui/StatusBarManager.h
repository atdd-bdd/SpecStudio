#pragma once

#include <QObject>

class QStatusBar;
class QLabel;

class StatusBarManager : public QObject
{
    Q_OBJECT

public:
    explicit StatusBarManager(QStatusBar* statusBar, QObject* parent = nullptr);

    void setSolutionName(const QString& name);
    void setGitBranch(const QString& branch);
    void setLineColumn(int line, int column);
    void clearAll();

private:
    QStatusBar* m_statusBar  = nullptr;
    QLabel*     m_solutionLabel = nullptr;
    QLabel*     m_branchLabel   = nullptr;
    QLabel*     m_posLabel      = nullptr;
};
