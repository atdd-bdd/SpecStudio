#include "PlainTextEditor.h"
#include "LineNumberEdit.h"

#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QRegularExpression>
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

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &PlainTextEditor::onFileChangedOnDisk);

    load(filePath);
}

void PlainTextEditor::load(const QString& path)
{
    // Update watcher to track the new path
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (QFile::exists(path))
        m_watcher->addPath(path);

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

void PlainTextEditor::onFileChangedOnDisk(const QString& path)
{
    // Re-add the path — some editors replace the file on save (Qt removes it from watcher)
    if (QFile::exists(path) && !m_watcher->files().contains(path))
        m_watcher->addPath(path);

    auto btn = QMessageBox::question(this, tr("File Changed"),
        tr("'%1' was modified outside the editor. Reload?")
            .arg(QFileInfo(path).fileName()),
        QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes)
        load(path);
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

void PlainTextEditor::cut()       { m_edit->cut(); }
void PlainTextEditor::copy()      { m_edit->copy(); }
void PlainTextEditor::paste()     { m_edit->paste(); }
void PlainTextEditor::undo()      { m_edit->undo(); }
void PlainTextEditor::redo()      { m_edit->redo(); }
void PlainTextEditor::selectAll() { m_edit->selectAll(); }

void PlainTextEditor::goToLine(int n)
{
    const QTextBlock block = m_edit->document()->findBlockByLineNumber(qMax(0, n - 1));
    if (!block.isValid()) return;
    QTextCursor c(block);
    m_edit->setTextCursor(c);
    m_edit->ensureCursorVisible();
}

bool PlainTextEditor::findNext(const QString& text, bool caseSensitive, bool wrapAround, bool useRegex)
{
    auto doFind = [&](QTextDocument::FindFlags flags) -> bool {
        if (useRegex) {
            QRegularExpression re(text, caseSensitive
                ? QRegularExpression::NoPatternOption
                : QRegularExpression::CaseInsensitiveOption);
            return re.isValid() && m_edit->find(re, flags);
        }
        if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
        return m_edit->find(text, flags);
    };

    bool found = doFind({});
    if (!found && wrapAround) {
        QTextCursor c = m_edit->textCursor();
        c.movePosition(QTextCursor::Start);
        m_edit->setTextCursor(c);
        found = doFind({});
    }
    return found;
}

bool PlainTextEditor::findPrev(const QString& text, bool caseSensitive, bool wrapAround, bool useRegex)
{
    auto doFind = [&](QTextDocument::FindFlags flags) -> bool {
        flags |= QTextDocument::FindBackward;
        if (useRegex) {
            QRegularExpression re(text, caseSensitive
                ? QRegularExpression::NoPatternOption
                : QRegularExpression::CaseInsensitiveOption);
            return re.isValid() && m_edit->find(re, flags);
        }
        if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
        return m_edit->find(text, flags);
    };

    bool found = doFind({});
    if (!found && wrapAround) {
        QTextCursor c = m_edit->textCursor();
        c.movePosition(QTextCursor::End);
        m_edit->setTextCursor(c);
        found = doFind({});
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

int PlainTextEditor::replaceAll(const QString& findText, const QString& replacement, bool caseSensitive, bool useRegex)
{
    QList<QTextCursor> hits;
    QTextCursor c(m_edit->document());

    if (useRegex) {
        QRegularExpression re(findText, caseSensitive
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid()) return 0;
        while (true) {
            c = m_edit->document()->find(re, c);
            if (c.isNull()) break;
            hits.append(c);
        }
    } else {
        QTextDocument::FindFlags flags;
        if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
        while (true) {
            c = m_edit->document()->find(findText, c, flags);
            if (c.isNull()) break;
            hits.append(c);
        }
    }

    if (hits.isEmpty()) return 0;

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

void PlainTextEditor::setCompletionWords(const QStringList& words)
{
    m_edit->setBaseCompletionWords(words);
}
