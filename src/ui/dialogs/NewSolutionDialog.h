#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;
class QRadioButton;

class NewSolutionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewSolutionDialog(QWidget* parent = nullptr);

    QString solutionName() const;
    QString rootFolder()   const;
    bool    useGitHub()    const;

private:
    QLineEdit*    m_nameEdit     = nullptr;
    QLineEdit*    m_folderEdit   = nullptr;
    QPushButton*  m_browseBtn    = nullptr;
    bool          m_folderEdited = false;
    QRadioButton* m_sharedFilesRadio = nullptr;
    QRadioButton* m_gitHubRadio      = nullptr;
};
