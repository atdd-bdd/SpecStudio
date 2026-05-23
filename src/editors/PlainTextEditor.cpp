#include "PlainTextEditor.h"

#include <QFile>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QTextStream>
#include <QVBoxLayout>

PlainTextEditor::PlainTextEditor(const QString& filePath, QWidget* parent)
    : BaseEditor(filePath, parent)
{
    m_edit = new QPlainTextEdit(this);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);

    QFont font("Courier New", 10);
    font.setFixedPitch(true);
    m_edit->setFont(font);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_edit);
    setLayout(layout);

    connect(m_edit, &QPlainTextEdit::modificationChanged,
            this,   [this](bool modified) { setDirty(modified); });

    load(filePath);
}

void PlainTextEditor::load(const QString& path)
{
    setFilePath(path);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_edit->setPlainText(QString());
        setDirty(false);
        return;
    }

    QTextStream in(&file);
    m_edit->setPlainText(in.readAll());
    m_edit->document()->setModified(false);
    setDirty(false);
}

bool PlainTextEditor::save()
{
    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Save Failed"),
            tr("Cannot write '%1': %2").arg(filePath(), file.errorString()));
        return false;
    }

    QTextStream out(&file);
    out << m_edit->toPlainText();

    m_edit->document()->setModified(false);
    setDirty(false);
    return true;
}

void PlainTextEditor::cut()   { m_edit->cut(); }
void PlainTextEditor::copy()  { m_edit->copy(); }
void PlainTextEditor::paste() { m_edit->paste(); }

void PlainTextEditor::setHighlighter(QSyntaxHighlighter* highlighter)
{
    delete m_highlighter;
    m_highlighter = highlighter;
    if (m_highlighter)
        m_highlighter->setDocument(m_edit->document());
}
