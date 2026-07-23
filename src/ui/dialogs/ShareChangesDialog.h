#pragma once

#include <QDialog>
#include <QList>
#include <QMap>

class Project;
class GitClient;
class QTreeWidget;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QDialogButtonBox;

// Friendly "Share Changes" (commit + push) dialog. Shows what changed across
// every project in the solution, lets the user describe the change, and
// offers an Advanced section with the target branch and a "push immediately"
// toggle. `gitClients` maps each project to the GitClient its git actions
// actually resolve to — normally one repo per project, but when the
// solution's repos are Combined, every project maps to the same shared
// instance, and this dialog shows that repo's status/branch once, not once
// per project.
class ShareChangesDialog : public QDialog
{
    Q_OBJECT

public:
    ShareChangesDialog(const QList<Project*>& projects,
                       const QMap<Project*, GitClient*>& gitClients,
                       QWidget* parent = nullptr);

    // True if any project actually has uncommitted changes — caller can skip
    // showing the dialog entirely when this is false.
    bool hasAnyChanges() const { return m_hasAnyChanges; }

    QString description() const;
    bool    pushImmediately() const;

private:
    void toggleAdvanced();

    QTreeWidget*      m_fileTree        = nullptr;
    QLabel*           m_summaryLabel    = nullptr;
    QPlainTextEdit*   m_descriptionEdit = nullptr;
    QCheckBox*        m_pushImmediately = nullptr;
    QWidget*          m_advancedPanel   = nullptr;
    QDialogButtonBox* m_buttons         = nullptr;
    bool              m_hasAnyChanges   = false;
};
