#pragma once

#include <QDialog>
#include <QStringList>

class GitClient;
class QLabel;
class QListWidget;
class QPushButton;

class ConflictResolutionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConflictResolutionDialog(const QString& projectName,
                                      GitClient*     git,
                                      const QString& projectRoot,
                                      const QStringList& conflictedFiles,
                                      QWidget* parent = nullptr);

signals:
    void openFileRequested(const QString& absolutePath);

private slots:
    void onOpenInEditor();
    void onUseMine();
    void onUseTheirs();
    void onUseMineAll();
    void onUseTheirsAll();
    void onAbortMerge();
    void onFinishMerge();

private:
    void refreshList();
    QStringList selectedRelativePaths() const;

    GitClient*   m_git;
    QString      m_projectRoot;
    QStringList  m_conflicted;
    QListWidget* m_fileList;
    QLabel*      m_statusLabel;
    QPushButton* m_finishBtn;
};
