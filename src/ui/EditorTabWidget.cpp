#include "EditorTabWidget.h"
#include "../editors/BaseEditor.h"
#include "../editors/EditorFactory.h"

#include <QLabel>
#include <QMessageBox>
#include <QFileInfo>

EditorTabWidget::EditorTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(true);
    setDocumentMode(true);

    connect(this, &QTabWidget::tabCloseRequested, this, &EditorTabWidget::onTabCloseRequested);
    connect(this, &QTabWidget::currentChanged,    this, &EditorTabWidget::onCurrentChanged);
}

void EditorTabWidget::openFile(const QString& absolutePath, Project* /*project*/)
{
    if (m_openFiles.contains(absolutePath)) {
        setCurrentWidget(m_openFiles[absolutePath]);
        return;
    }

    auto* editor = EditorFactory::create(absolutePath, nullptr, this);
    m_openFiles[absolutePath] = editor;

    QString title = QFileInfo(absolutePath).fileName();
    int idx = addTab(editor, title);
    setCurrentIndex(idx);

    connect(editor, &BaseEditor::modificationChanged, this, [this, editor](bool dirty) {
        int i = indexOf(editor);
        if (i < 0) return;
        QString t = tabText(i);
        if (dirty && !t.endsWith('*'))
            setTabText(i, t + '*');
        else if (!dirty && t.endsWith('*'))
            setTabText(i, t.chopped(1));
    });
}

BaseEditor* EditorTabWidget::currentEditor() const
{
    return qobject_cast<BaseEditor*>(currentWidget());
}

BaseEditor* EditorTabWidget::editorForPath(const QString& absolutePath) const
{
    return m_openFiles.value(absolutePath, nullptr);
}

bool EditorTabWidget::saveCurrentFile()
{
    if (auto* ed = currentEditor())
        return ed->save();
    return false;
}

bool EditorTabWidget::saveAllFiles()
{
    bool ok = true;
    for (auto* ed : m_openFiles)
        if (!ed->save()) ok = false;
    return ok;
}

bool EditorTabWidget::closeTab(int index)
{
    auto* ed = qobject_cast<BaseEditor*>(widget(index));
    if (!ed) return true;

    if (ed->isDirty()) {
        auto btn = QMessageBox::question(this, tr("Unsaved Changes"),
            tr("'%1' has unsaved changes. Save before closing?")
                .arg(QFileInfo(ed->filePath()).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel) return false;
        if (btn == QMessageBox::Save)   ed->save();
    }

    m_openFiles.remove(ed->filePath());
    removeTab(index);
    ed->deleteLater();
    return true;
}

void EditorTabWidget::onTabCloseRequested(int index)
{
    closeTab(index);
}

void EditorTabWidget::onCurrentChanged(int index)
{
    auto* ed = qobject_cast<BaseEditor*>(widget(index));
    emit currentFileChanged(ed ? ed->filePath() : QString());
}
