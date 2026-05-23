#include "GitPushDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

GitPushDialog::GitPushDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Commit and Push"));
    setMinimumWidth(400);

    m_reasonEdit = new QLineEdit(this);
    m_reasonEdit->setPlaceholderText(tr("Describe the change..."));

    auto* form = new QFormLayout();
    form->addRow(tr("Change reason:"), m_reasonEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Enter a description for this commit:"), this));
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto updateOk = [this, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(
            !m_reasonEdit->text().trimmed().isEmpty());
    };
    connect(m_reasonEdit, &QLineEdit::textChanged, this, updateOk);
    updateOk();
}

QString GitPushDialog::changeReason() const
{
    return m_reasonEdit->text().trimmed();
}
