#pragma once

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

    virtual void cut()       {}
    virtual void copy()      {}
    virtual void paste()     {}
    virtual void undo()      {}
    virtual void redo()      {}
    virtual void selectAll() {}

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
