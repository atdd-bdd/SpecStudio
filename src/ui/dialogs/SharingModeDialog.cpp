#include "SharingModeDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

SharingModeDialog::SharingModeDialog(const QString& solutionName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("How Should This Solution Be Shared?"));
    setMinimumWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("How should '%1' be shared?").arg(solutionName), this));

    m_sharedFilesRadio = new QRadioButton(tr("Shared file system"), this);
    m_gitHubRadio      = new QRadioButton(tr("Source control (GitHub)"), this);
    m_sharedFilesRadio->setChecked(true);
    auto* group = new QButtonGroup(this);
    group->addButton(m_sharedFilesRadio);
    group->addButton(m_gitHubRadio);
    layout->addWidget(m_sharedFilesRadio);
    layout->addWidget(m_gitHubRadio);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

bool SharingModeDialog::useGitHub() const { return m_gitHubRadio->isChecked(); }
