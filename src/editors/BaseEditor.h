#pragma once

#include <QList>
#include <QPair>
#include <QWidget>

class BaseEditor : public QWidget
{
    Q_OBJECT

public:
    explicit BaseEditor(const QString& filePath, QWidget* parent = nullptr);
    ~BaseEditor() override = default;

    virtual void load(const QString& path) = 0;
    virtual bool save() = 0;

    QString filePath() const { return m_filePath; }
    bool    isDirty()  const { return m_dirty; }

    virtual void cut()           {}
    virtual void copy()          {}
    virtual void paste()         {}
    virtual void undo()          {}
    virtual void redo()          {}
    virtual void selectAll()     {}
    virtual void goToLine(int)   {}
    virtual void formatTable()   {}
    virtual void editTable()     {}
    virtual void editString()    {}
    virtual void setErrorMarks(const QList<QPair<int,int>>&) {}
    virtual void setTagCompletionWords(const QStringList&)  {}

    // Replace the whole document in one undoable step, leaving it unsaved.
    // Used by Revert, which puts an earlier version in front of the user rather
    // than writing it to disk: nothing is committed to until they save, and one
    // Ctrl+Z puts it back. Returns false for editors that hold no text.
    virtual bool replaceAllText(const QString&) { return false; }

    // Caret and viewport, so a command that works over every open editor can
    // put the user back where they were. -1 means "this editor has none".
    virtual int  cursorPosition() const     { return -1; }
    virtual void setCursorPosition(int)     {}
    virtual int  verticalScroll() const     { return -1; }
    virtual void setVerticalScroll(int)     {}

signals:
    void modificationChanged(bool dirty);
    void fileOpenRequested(const QString& absolutePath);

protected:
    void setFilePath(const QString& path) { m_filePath = path; }
    void setDirty(bool dirty);

private:
    QString m_filePath;
    bool    m_dirty = false;
};

// Temporary placeholder used in Phase 2/3 before real editors exist.
// Replaced by EditorFactory in Phase 6.
class BaseEditorPlaceholder : public BaseEditor
{
    Q_OBJECT
public:
    explicit BaseEditorPlaceholder(const QString& filePath, QWidget* parent = nullptr);
    void load(const QString& path) override;
    bool save() override;
};
