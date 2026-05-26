#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

class NewSolutionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewSolutionDialog(QWidget* parent = nullptr);

    QString solutionName() const;
    QString rootFolder()   const;

private:
    QLineEdit*   m_nameEdit     = nullptr;
    QLineEdit*   m_folderEdit   = nullptr;
    QPushButton* m_browseBtn    = nullptr;
    bool         m_folderEdited = false;
};
