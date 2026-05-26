#include "NewFileDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

NewFileDialog::NewFileDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New File"));
    setMinimumWidth(340);

    m_nameEdit  = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("e.g. login"));

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Feature file (.feature)",  ".feature");
    m_typeCombo->addItem("FeatureX file (.featurex)", ".featurex");
    m_typeCombo->addItem("Text file (.txt)",          ".txt");

    auto* form = new QFormLayout();
    form->addRow(tr("File name:"), m_nameEdit);
    form->addRow(tr("File type:"), m_typeCombo);

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

QString NewFileDialog::fileName() const
{
    QString name = m_nameEdit->text().trimmed();
    QString ext  = m_typeCombo->currentData().toString();
    // Don't double-add the extension if the user typed it already
    if (!name.endsWith(ext, Qt::CaseInsensitive))
        name += ext;
    return name;
}
