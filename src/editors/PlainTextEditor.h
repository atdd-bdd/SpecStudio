#pragma once

#include "BaseEditor.h"
#include "LineNumberEdit.h"

class QFileSystemWatcher;
class QSyntaxHighlighter;

class PlainTextEditor : public BaseEditor
{
    Q_OBJECT

public:
    explicit PlainTextEditor(const QString& filePath, QWidget* parent = nullptr);

    void load(const QString& path) override;
    bool save() override;

    void cut()           override;
    void copy()          override;
    void paste()         override;
    void undo()          override;
    void redo()          override;
    void selectAll()     override;
    void goToLine(int n)  override;
    void formatTable()    override;
    void setErrorMarks(const QList<QPair<int,int>>& marks) override;
    void setTagCompletionWords(const QStringList& tags)    override;

    void editTable()  override;
    void editString() override;

    void suppressNextExternalChange() { m_ignoreNextChange = true; }
    void setAutoReload(bool autoReload) { m_autoReload = autoReload; }

    bool findNext(const QString& text, bool caseSensitive, bool wrapAround, bool useRegex = false);
    bool findPrev(const QString& text, bool caseSensitive, bool wrapAround, bool useRegex = false);
    bool replaceCurrent(const QString& replacement);
    int  replaceAll(const QString& findText, const QString& replacement, bool caseSensitive, bool useRegex = false);

    QPlainTextEdit* textEdit() const { return m_edit; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

    // Subclasses override to append extra actions before the menu is shown.
    virtual void populateContextMenu(QMenu*) {}

    void setHighlighter(QSyntaxHighlighter* highlighter);
    void setCompletionWords(const QStringList& words);
    LineNumberEdit* lineNumberEdit() const { return m_edit; }

private slots:
    void onFileChangedOnDisk(const QString& path);

private:
    bool isInTableContext()  const;
    bool isInStringContext() const;
    void showEditorContextMenu(const QPoint& globalPos);

    LineNumberEdit*     m_edit             = nullptr;
    QSyntaxHighlighter* m_highlighter     = nullptr;
    QFileSystemWatcher* m_watcher         = nullptr;
    bool                m_ignoreNextChange = false;
    bool                m_autoReload       = false;
    bool                m_savingNow        = false;
};
