#pragma once

#include <QDialog>

class QLineEdit;
class Solution;

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(Solution* solution, QWidget* parent = nullptr);

    QString projectName() const;

private:
    QLineEdit* m_nameEdit = nullptr;
};
