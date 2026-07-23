#pragma once

#include <QDialog>

class QRadioButton;

// Minimal standalone "Shared file system vs GitHub" choice, identical in
// content to NewSolutionDialog's inline radio group. Used by NewProjectDialog
// flows that construct a brand-new Solution on the fly, so that choice isn't
// skippable just because the user went through "New Project..." instead of
// "New Solution...".
class SharingModeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SharingModeDialog(const QString& solutionName, QWidget* parent = nullptr);

    bool useGitHub() const;

private:
    QRadioButton* m_sharedFilesRadio = nullptr;
    QRadioButton* m_gitHubRadio      = nullptr;
};
