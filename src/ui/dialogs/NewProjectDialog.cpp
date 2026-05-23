#include "NewProjectDialog.h"
#include "../../model/Solution.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

NewProjectDialog::NewProjectDialog(Solution* solution, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Project"));
    setMinimumWidth(380);

    m_nameEdit = new QLineEdit(this);

    auto* form = new QFormLayout();
    form->addRow(tr("Project name:"), m_nameEdit);
    form->addRow(tr("Solution:"),     new QLabel(solution ? solution->name() : QString(), this));
    form->addRow(tr("Location:"),     new QLabel(solution ? solution->rootPath() : QString(), this));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto updateOk = [this, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(
            !m_nameEdit->text().trimmed().isEmpty());
    };
    connect(m_nameEdit, &QLineEdit::textChanged, this, updateOk);
    updateOk();
}

QString NewProjectDialog::projectName() const { return m_nameEdit->text().trimmed(); }
