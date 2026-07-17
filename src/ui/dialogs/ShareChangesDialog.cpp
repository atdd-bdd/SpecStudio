#include "ShareChangesDialog.h"
#include "../../model/Project.h"
#include "../../git/GitClient.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

QString friendlyStatus(const QString& code)
{
    if (code == "??") return QStringLiteral("Added");
    if (code.contains('A')) return QStringLiteral("Added");
    if (code.contains('D')) return QStringLiteral("Removed");
    if (code.contains('R')) return QStringLiteral("Renamed");
    return QStringLiteral("Updated");
}

} // namespace

ShareChangesDialog::ShareChangesDialog(const QList<Project*>& projects, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Share Changes"));
    setMinimumSize(560, 480);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);

    m_fileTree = new QTreeWidget(this);
    m_fileTree->setHeaderLabels({ tr("File"), tr("Change") });
    m_fileTree->setRootIsDecorated(true);

    int totalFiles = 0;
    for (Project* proj : projects) {
        const QStringList lines = proj->git()->status();
        if (lines.isEmpty()) continue;

        auto* projItem = new QTreeWidgetItem(m_fileTree, { proj->name() });
        QFont f = projItem->font(0);
        f.setBold(true);
        projItem->setFont(0, f);

        for (const QString& line : lines) {
            if (line.size() < 3) continue;
            const QString code = line.left(2);
            const QString path = line.mid(3).trimmed();
            auto* fileItem = new QTreeWidgetItem(projItem, { path, friendlyStatus(code) });
            Q_UNUSED(fileItem);
            ++totalFiles;
        }
        projItem->setExpanded(true);
    }
    m_hasAnyChanges = totalFiles > 0;

    m_summaryLabel->setText(totalFiles > 0
        ? tr("You have changes ready to share.\n%1 file(s) changed across %2 project(s).")
              .arg(totalFiles).arg(projects.size())
        : tr("No changes were found to share."));

    auto* descLabel = new QLabel(tr("Describe your changes:"), this);
    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setPlaceholderText(
        tr("This helps others understand what you shared."));
    m_descriptionEdit->setMaximumHeight(80);

    // ---- Advanced (collapsed by default) ----
    auto* advancedToggle = new QPushButton(tr("Advanced Options ▸"), this);
    advancedToggle->setFlat(true);
    advancedToggle->setStyleSheet("text-align: left;");

    m_advancedPanel = new QWidget(this);
    auto* advForm = new QFormLayout(m_advancedPanel);
    advForm->setContentsMargins(16, 4, 0, 4);
    for (Project* proj : projects)
        advForm->addRow(tr("%1 branch:").arg(proj->name()), new QLabel(proj->git()->currentBranch(), m_advancedPanel));
    m_pushImmediately = new QCheckBox(tr("Push immediately after sharing"), m_advancedPanel);
    m_pushImmediately->setChecked(true);
    advForm->addRow(m_pushImmediately);
    m_advancedPanel->setVisible(false);

    connect(advancedToggle, &QPushButton::clicked, this, [this, advancedToggle] {
        const bool nowVisible = !m_advancedPanel->isVisible();
        m_advancedPanel->setVisible(nowVisible);
        advancedToggle->setText(nowVisible ? tr("Advanced Options ▾") : tr("Advanced Options ▸"));
    });

    m_buttons = new QDialogButtonBox(this);
    auto* shareBtn  = m_buttons->addButton(tr("Share Changes"), QDialogButtonBox::AcceptRole);
    m_buttons->addButton(QDialogButtonBox::Cancel);
    shareBtn->setDefault(true);
    shareBtn->setToolTip(tr("Saves your changes and makes them available to others. (Commit + Push)"));

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_fileTree, 1);
    layout->addWidget(descLabel);
    layout->addWidget(m_descriptionEdit);
    layout->addWidget(advancedToggle);
    layout->addWidget(m_advancedPanel);
    layout->addWidget(m_buttons);
}

QString ShareChangesDialog::description() const
{
    return m_descriptionEdit->toPlainText().trimmed();
}

bool ShareChangesDialog::pushImmediately() const
{
    return m_pushImmediately->isChecked();
}
