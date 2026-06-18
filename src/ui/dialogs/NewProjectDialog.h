#pragma once

#include <QDialog>

class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class Solution;

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    // defaultLocation comes from AppSettings::defaultProjectLocation()
    explicit NewProjectDialog(Solution*      currentSolution,
                              const QString& defaultLocation,
                              QWidget*       parent = nullptr);

    QString projectName()        const;

    bool    isStandalone()       const;  // project lives in its own folder, no solution wrapper
    QString standaloneLocation() const;  // valid when isStandalone()

    bool    addToCurrentSolution() const; // valid when !isStandalone() and solution was open
    QString newSolutionName()    const;   // valid when !isStandalone() && !addToCurrentSolution()
    QString newSolutionFolder()  const;   // valid when !isStandalone() && !addToCurrentSolution()

private:
    void syncStandaloneFolder();
    void syncSolutionFolder();
    void updatePanels();
    void updateOk();

    QString      m_defaultLocation;
    Solution*    m_currentSolution    = nullptr;

    QLineEdit*   m_projectNameEdit    = nullptr;

    QRadioButton* m_standaloneBtn     = nullptr;
    QLineEdit*   m_standaloneFolderEdit = nullptr;
    QPushButton* m_standaloneBrowseBtn  = nullptr;
    bool         m_standaloneFolderEdited = false;

    QRadioButton* m_partOfSolutionBtn = nullptr;

    QRadioButton* m_addToCurrentBtn   = nullptr;
    QLabel*       m_currentSolLabel   = nullptr;

    QRadioButton* m_newSolutionBtn    = nullptr;
    QLineEdit*   m_solNameEdit        = nullptr;
    QLineEdit*   m_solFolderEdit      = nullptr;
    QPushButton* m_solBrowseBtn       = nullptr;
    bool         m_solNameEdited      = false;
    bool         m_solFolderEdited    = false;

    QGroupBox*   m_standaloneGroup    = nullptr;
    QGroupBox*   m_solutionGroup      = nullptr;

    QPushButton* m_okBtn              = nullptr;
};
