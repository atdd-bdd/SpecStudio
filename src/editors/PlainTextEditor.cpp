#include "PlainTextEditor.h"
#include "LineNumberEdit.h"

#include <QFile>
#include <QMessageBox>
#include <QSyntaxHighlighter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QVBoxLayout>

PlainTextEditor::PlainTextEditor(const QString& filePath, QWidget* parent)
    : BaseEditor(filePath, parent)
{
    m_edit = new LineNumberEdit(this);
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

bool PlainTextEditor::findNext(const QString& text, bool caseSensitive, bool wrapAround)
{
    QTextDocument::FindFlags flags;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    bool found = m_edit->find(text, flags);
    if (!found && wrapAround) {
        QTextCursor c = m_edit->textCursor();
        c.movePosition(QTextCursor::Start);
        m_edit->setTextCursor(c);
        found = m_edit->find(text, flags);
    }
    return found;
}

bool PlainTextEditor::findPrev(const QString& text, bool caseSensitive, bool wrapAround)
{
    QTextDocument::FindFlags flags = QTextDocument::FindBackward;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    bool found = m_edit->find(text, flags);
    if (!found && wrapAround) {
        QTextCursor c = m_edit->textCursor();
        c.movePosition(QTextCursor::End);
        m_edit->setTextCursor(c);
        found = m_edit->find(text, flags);
    }
    return found;
}

bool PlainTextEditor::replaceCurrent(const QString& replacement)
{
    QTextCursor c = m_edit->textCursor();
    if (!c.hasSelection()) return false;
    c.insertText(replacement);
    return true;
}

int PlainTextEditor::replaceAll(const QString& findText, const QString& replacement, bool caseSensitive)
{
    QTextDocument::FindFlags flags;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;

    // Collect all hits first so positions stay valid during replacement.
    QList<QTextCursor> hits;
    QTextCursor c(m_edit->document());
    while (true) {
        c = m_edit->document()->find(findText, c, flags);
        if (c.isNull()) break;
        hits.append(c);
    }
    if (hits.isEmpty()) return 0;

    // Replace in reverse order inside one undo block.
    QTextCursor undo = hits.first();
    undo.beginEditBlock();
    for (int i = hits.size() - 1; i >= 0; --i)
        hits[i].insertText(replacement);
    undo.endEditBlock();
    return hits.size();
}

void PlainTextEditor::setHighlighter(QSyntaxHighlighter* highlighter)
{
    delete m_highlighter;
    m_highlighter = highlighter;
    if (m_highlighter)
        m_highlighter->setDocument(m_edit->document());
}
