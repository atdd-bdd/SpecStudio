#include "NewSolutionDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>

NewSolutionDialog::NewSolutionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Solution"));
    setMinimumWidth(460);

    m_nameEdit   = new QLineEdit(this);
    m_folderEdit = new QLineEdit(this);
    m_browseBtn  = new QPushButton(tr("Browse..."), this);

    const QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    // Auto-fill folder from name unless the user has manually edited it
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this, docsPath](const QString& name) {
        if (!m_folderEdited) {
            QString safe = name.trimmed();
            safe.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
            m_folderEdit->setText(safe.isEmpty() ? QString()
                                                 : docsPath + QDir::separator() + safe);
        }
    });

    // Once the user types directly in the folder box, stop auto-filling
    connect(m_folderEdit, &QLineEdit::textEdited, this, [this] {
        m_folderEdited = true;
    });

    auto* folderRow = new QHBoxLayout();
    folderRow->addWidget(m_folderEdit);
    folderRow->addWidget(m_browseBtn);

    auto* form = new QFormLayout();
    form->addRow(tr("Solution name:"), m_nameEdit);
    form->addRow(tr("Root folder:"),   folderRow);

    m_sharedFilesRadio = new QRadioButton(tr("Shared file system"), this);
    m_gitHubRadio      = new QRadioButton(tr("Source control (GitHub)"), this);
    m_sharedFilesRadio->setChecked(true);
    auto* sharingGroup = new QButtonGroup(this);
    sharingGroup->addButton(m_sharedFilesRadio);
    sharingGroup->addButton(m_gitHubRadio);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("How should this solution be shared?"), this));
    layout->addWidget(m_sharedFilesRadio);
    layout->addWidget(m_gitHubRadio);
    layout->addWidget(buttons);

    connect(m_browseBtn, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Parent Folder"));
        if (!dir.isEmpty()) {
            QString name = m_nameEdit->text().trimmed();
            m_folderEdit->setText(name.isEmpty() ? dir : dir + QDir::separator() + name);
            m_folderEdited = true;
        }
    });

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

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
bool    NewSolutionDialog::useGitHub()    const { return m_gitHubRadio->isChecked(); }
