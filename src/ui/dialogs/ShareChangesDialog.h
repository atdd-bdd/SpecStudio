#pragma once

#include <QDialog>
#include <QList>
#include <QMap>

class Project;
class GitClient;
class QTreeWidget;
class QPlainTextEdit;
class QWidget;
class QLabel;
class QDialogButtonBox;

// Friendly "Share Changes" (commit + push) dialog. Shows what changed across
// every project in the solution and lets the user describe the change. Every
// project shares one repo, so `gitClients` maps each project to that same
// GitClient instance — this dialog dedupes so the repo's status/branch is
// shown once, not once per project.
class ShareChangesDialog : public QDialog
{
    Q_OBJECT

public:
    enum class ShareResult { SharePushed, DontShareNow, Cancelled };

    ShareChangesDialog(const QList<Project*>& projects,
                       const QMap<Project*, GitClient*>& gitClients,
                       QWidget* parent = nullptr);

    // True if any project actually has uncommitted changes — caller can skip
    // showing the dialog entirely when this is false.
    bool hasAnyChanges() const { return m_hasAnyChanges; }

    QString      description() const;
    ShareResult  shareResult() const;

private:
    QTreeWidget*      m_fileTree        = nullptr;
    QLabel*           m_summaryLabel    = nullptr;
    QPlainTextEdit*   m_descriptionEdit = nullptr;
    QWidget*          m_advancedPanel   = nullptr;
    QDialogButtonBox* m_buttons         = nullptr;
    bool              m_hasAnyChanges   = false;
};
