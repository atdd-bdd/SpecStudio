#pragma once

#include <QDialog>

class AppSettings;
class Solution;
class Project;
class QRadioButton;
class QComboBox;
class QLineEdit;
class QStackedWidget;

// Git > Repository Settings... — the one place to configure git for a
// solution: whether every project shares one repo (Combined, rooted at the
// solution folder) or each has its own (Separate, the original/default
// behavior), plus the relevant remote URL/branch/credentials either way.
class RepositorySettingsDialog : public QDialog
{
    Q_OBJECT

public:
    RepositorySettingsDialog(AppSettings* settings, Solution* solution, QWidget* parent = nullptr);

private:
    void buildUi();
    void loadValues();
    void saveValues();
    void loadProjectFields(Project* proj);
    void saveProjectFields(Project* proj);

    AppSettings* m_settings = nullptr;
    Solution*    m_solution = nullptr;
    Project*     m_pickedProject = nullptr;

    QRadioButton* m_separateRadio = nullptr;
    QRadioButton* m_combinedRadio = nullptr;
    QStackedWidget* m_stack = nullptr;

    // Combined mode: one shared remote for the whole solution
    QLineEdit* m_combinedUrl    = nullptr;
    QLineEdit* m_combinedBranch = nullptr;
    QLineEdit* m_combinedUser   = nullptr;
    QLineEdit* m_combinedPass   = nullptr;

    // Separate mode: pick a project, edit its own remote
    QComboBox* m_projectPicker  = nullptr;
    QLineEdit* m_separateUrl    = nullptr;
    QLineEdit* m_separateBranch = nullptr;
    QLineEdit* m_separateUser   = nullptr;
    QLineEdit* m_separatePass   = nullptr;
};
