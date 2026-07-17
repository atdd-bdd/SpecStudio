#pragma once

#include <QDialog>
#include <QList>

class Project;
class QTreeWidget;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QDialogButtonBox;

// Friendly "Share Changes" (commit + push) dialog. Shows what changed across
// every project in the solution (each project has its own git repo), lets the
// user describe the change, and offers an Advanced section with the target
// branch and a "push immediately" toggle.
class ShareChangesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareChangesDialog(const QList<Project*>& projects, QWidget* parent = nullptr);

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
