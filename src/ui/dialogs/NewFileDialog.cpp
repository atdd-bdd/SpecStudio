#include "NewFileDialog.h"
#include "../../app/AppSettings.h"

#include <QComboBox>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

static QString labelForExt(const QString& ext)
{
    if (ext == ".spectable") return QObject::tr("SpecTable file (.spectable)");
    if (ext == ".feature")  return QObject::tr("Feature file (.feature)");
    if (ext == ".featurex") return QObject::tr("FeatureX file (.featurex)");
    if (ext == ".txt")      return QObject::tr("Text file (.txt)");
    if (ext == ".md")       return QObject::tr("Markdown file (.md)");
    if (ext == ".csv")      return QObject::tr("CSV file (.csv)");
    if (ext == ".xls")      return QObject::tr("Excel file (.xls)");
    if (ext == ".xlsx")     return QObject::tr("Excel file (.xlsx)");
    return ext;
}

NewFileDialog::NewFileDialog(AppSettings* settings, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New File"));
    setMinimumWidth(360);

    m_nameEdit  = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("e.g. login"));

    m_typeCombo = new QComboBox(this);
    for (const QString& ext : AppSettings::knownExtensions())
        m_typeCombo->addItem(labelForExt(ext), ext);

    // If an external editor is configured for an extension, note it in the label
    if (settings) {
        for (int i = 0; i < m_typeCombo->count(); ++i) {
            QString ext  = m_typeCombo->itemData(i).toString().mid(1); // strip leading '.'
            QString prog = settings->editorForExtension(ext);
            if (!prog.isEmpty()) {
                QFileInfo fi(prog);
                m_typeCombo->setItemText(i,
                    m_typeCombo->itemText(i) +
                    QStringLiteral(" [%1]").arg(fi.baseName()));
            }
        }
    }

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
    if (!name.endsWith(ext, Qt::CaseInsensitive))
        name += ext;
    return name;
}
