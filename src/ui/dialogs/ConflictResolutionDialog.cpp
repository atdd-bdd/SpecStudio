#include "ConflictResolutionDialog.h"
#include "../../git/GitClient.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ConflictResolutionDialog::ConflictResolutionDialog(const QString& projectName,
                                                   GitClient*     git,
                                                   const QString& projectRoot,
                                                   const QStringList& conflictedFiles,
                                                   QWidget* parent)
    : QDialog(parent)
    , m_git(git)
    , m_projectRoot(projectRoot)
    , m_conflicted(conflictedFiles)
{
    setWindowTitle(tr("Merge Conflicts — %1").arg(projectName));
    setMinimumSize(520, 360);

    auto* header = new QLabel(
        tr("%1 conflicted file(s). Resolve each file then click Finish Merge.")
            .arg(conflictedFiles.size()), this);
    header->setWordWrap(true);

    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (const QString& f : conflictedFiles)
        m_fileList->addItem(f);

    auto* openBtn       = new QPushButton(tr("Open in Editor"),      this);
    auto* mineBtn       = new QPushButton(tr("Use Mine"),             this);
    auto* theirsBtn     = new QPushButton(tr("Use Theirs"),           this);
    auto* mineAllBtn    = new QPushButton(tr("Use Mine for All"),     this);
    auto* theirsAllBtn  = new QPushButton(tr("Use Theirs for All"),   this);
    auto* abortBtn      = new QPushButton(tr("Abort Merge"),          this);
    m_finishBtn         = new QPushButton(tr("Finish Merge"),         this);
    m_finishBtn->setDefault(true);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray; font-style: italic;");

    auto* selRow = new QHBoxLayout;
    selRow->addWidget(openBtn);
    selRow->addWidget(mineBtn);
    selRow->addWidget(theirsBtn);
    selRow->addStretch();

    auto* allRow = new QHBoxLayout;
    allRow->addWidget(mineAllBtn);
    allRow->addWidget(theirsAllBtn);
    allRow->addStretch();

    auto* bottomRow = new QHBoxLayout;
    bottomRow->addWidget(abortBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(m_finishBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(header);
    layout->addWidget(m_fileList, 1);
    layout->addLayout(selRow);
    layout->addLayout(allRow);
    layout->addWidget(m_statusLabel);
    layout->addLayout(bottomRow);

    connect(openBtn,      &QPushButton::clicked, this, &ConflictResolutionDialog::onOpenInEditor);
    connect(mineBtn,      &QPushButton::clicked, this, &ConflictResolutionDialog::onUseMine);
    connect(theirsBtn,    &QPushButton::clicked, this, &ConflictResolutionDialog::onUseTheirs);
    connect(mineAllBtn,   &QPushButton::clicked, this, &ConflictResolutionDialog::onUseMineAll);
    connect(theirsAllBtn, &QPushButton::clicked, this, &ConflictResolutionDialog::onUseTheirsAll);
    connect(abortBtn,     &QPushButton::clicked, this, &ConflictResolutionDialog::onAbortMerge);
    connect(m_finishBtn,  &QPushButton::clicked, this, &ConflictResolutionDialog::onFinishMerge);
}

void ConflictResolutionDialog::refreshList()
{
    m_conflicted = m_git->conflictedFiles();
    m_fileList->clear();
    for (const QString& f : m_conflicted)
        m_fileList->addItem(f);
    if (m_conflicted.isEmpty()) {
        m_statusLabel->setText(tr("All conflicts resolved — click Finish Merge."));
        m_statusLabel->setStyleSheet("color: green;");
    }
}

QStringList ConflictResolutionDialog::selectedRelativePaths() const
{
    QStringList result;
    for (auto* item : m_fileList->selectedItems())
        result << item->text();
    return result;
}

void ConflictResolutionDialog::onOpenInEditor()
{
    for (const QString& rel : selectedRelativePaths())
        emit openFileRequested(m_projectRoot + "/" + rel);
}

void ConflictResolutionDialog::onUseMine()
{
    for (const QString& rel : selectedRelativePaths())
        m_git->resolveOurs(rel);
    refreshList();
}

void ConflictResolutionDialog::onUseTheirs()
{
    for (const QString& rel : selectedRelativePaths())
        m_git->resolveTheirs(rel);
    refreshList();
}

void ConflictResolutionDialog::onUseMineAll()
{
    for (const QString& rel : m_conflicted)
        m_git->resolveOurs(rel);
    refreshList();
}

void ConflictResolutionDialog::onUseTheirsAll()
{
    for (const QString& rel : m_conflicted)
        m_git->resolveTheirs(rel);
    refreshList();
}

void ConflictResolutionDialog::onAbortMerge()
{
    auto btn = QMessageBox::question(this, tr("Abort Merge"),
        tr("Abort the merge and restore the pre-merge state?"),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;
    m_git->abortMerge();
    reject();
}

void ConflictResolutionDialog::onFinishMerge()
{
    const QStringList remaining = m_git->conflictedFiles();
    if (!remaining.isEmpty()) {
        QMessageBox::warning(this, tr("Unresolved Conflicts"),
            tr("%1 file(s) still have conflicts. Resolve them first.")
                .arg(remaining.size()));
        return;
    }
    m_git->finishMerge({});
    accept();
}
