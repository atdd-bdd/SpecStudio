#pragma once

#include <QDialog>

class AppSettings;
class Solution;
class QLineEdit;

// Git > Repository Settings... — configure the solution's single shared git
// remote (URL/branch/credentials). Every project in a solution shares one
// repository, rooted at the solution folder.
class RepositorySettingsDialog : public QDialog
{
    Q_OBJECT

public:
    RepositorySettingsDialog(AppSettings* settings, Solution* solution, QWidget* parent = nullptr);

private:
    void buildUi();
    void loadValues();
    void saveValues();

    AppSettings* m_settings = nullptr;
    Solution*    m_solution = nullptr;

    QLineEdit* m_url    = nullptr;
    QLineEdit* m_branch = nullptr;
    QLineEdit* m_user   = nullptr;
    QLineEdit* m_pass   = nullptr;
};
