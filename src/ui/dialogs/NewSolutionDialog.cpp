#include "NewSolutionDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

NewSolutionDialog::NewSolutionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Solution"));
    setMinimumWidth(400);

    m_nameEdit   = new QLineEdit(this);
    m_folderEdit = new QLineEdit(this);
    m_browseBtn  = new QPushButton(tr("Browse..."), this);

    auto* folderRow = new QHBoxLayout();
    folderRow->addWidget(m_folderEdit);
    folderRow->addWidget(m_browseBtn);

    auto* form = new QFormLayout();
    form->addRow(tr("Solution name:"), m_nameEdit);
    form->addRow(tr("Root folder:"),   folderRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_browseBtn, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Root Folder"));
        if (!dir.isEmpty())
            m_folderEdit->setText(dir);
    });

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Enable OK only when both fields are filled
    auto updateOk = [this, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(
            !m_nameEdit->text().trimmed().isEmpty() &&
            !m_folderEdit->text().trimmed().isEmpty());
    };
    connect(m_nameEdit,   &QLineEdit::textChanged, this, updateOk);
    connect(m_folderEdit, &QLineEdit::textChanged, this, updateOk);
    updateOk();
}

QString NewSolutionDialog::solutionName() const { return m_nameEdit->text().trimmed(); }
QString NewSolutionDialog::rootFolder()   const { return m_folderEdit->text().trimmed(); }
