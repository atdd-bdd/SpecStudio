#include "StringDialog.h"

#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

StringDialog::StringDialog(const QString& content, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit String"));
    resize(540, 320);

    m_edit = new QPlainTextEdit(this);

    QFont font("Courier New", 11);
    font.setFixedPitch(true);
    m_edit->setFont(font);
    m_edit->setPlainText(content);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("String content (between triple-quotes):"), this));
    layout->addWidget(m_edit);
    layout->addWidget(buttons);
    setLayout(layout);

    m_edit->setFocus();
}

QString StringDialog::content() const
{
    return m_edit->toPlainText();
}
