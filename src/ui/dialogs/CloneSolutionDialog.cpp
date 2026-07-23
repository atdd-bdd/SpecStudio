#include "CloneSolutionDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

CloneSolutionDialog::CloneSolutionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Clone an Existing Solution"));
    setMinimumWidth(460);

    m_urlEdit   = new QLineEdit(this);
    m_dirEdit   = new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), this);
    m_browseBtn = new QPushButton(tr("Browse..."), this);
    m_userEdit  = new QLineEdit(this);
    m_tokenEdit = new QLineEdit(this);
    m_tokenEdit->setEchoMode(QLineEdit::Password);

    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(m_dirEdit);
    dirRow->addWidget(m_browseBtn);

    auto* form = new QFormLayout();
    form->addRow(tr("Remote URL:"), m_urlEdit);
    form->addRow(tr("Local folder:"), dirRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("If this is a private repository, provide credentials:"), this));
    auto* credForm = new QFormLayout();
    credForm->addRow(tr("Username:"), m_userEdit);
    credForm->addRow(tr("Personal Access Token:"), m_tokenEdit);
    layout->addLayout(credForm);
    layout->addWidget(buttons);

    connect(m_browseBtn, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Parent Folder"));
        if (!dir.isEmpty())
            m_dirEdit->setText(dir);
    });

    auto updateOk = [this, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(
            !m_urlEdit->text().trimmed().isEmpty() &&
            !m_dirEdit->text().trimmed().isEmpty());
    };
    connect(m_urlEdit, &QLineEdit::textChanged, this, updateOk);
    connect(m_dirEdit, &QLineEdit::textChanged, this, updateOk);
    updateOk();
}

QString CloneSolutionDialog::remoteUrl() const { return m_urlEdit->text().trimmed(); }
QString CloneSolutionDialog::localDirectory() const { return m_dirEdit->text().trimmed(); }
QString CloneSolutionDialog::username() const { return m_userEdit->text().trimmed(); }
QString CloneSolutionDialog::personalAccessToken() const { return m_tokenEdit->text(); }
