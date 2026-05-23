#include "BaseEditor.h"

#include <QLabel>
#include <QVBoxLayout>

BaseEditor::BaseEditor(const QString& filePath, QWidget* parent)
    : QWidget(parent)
    , m_filePath(filePath)
{}

void BaseEditor::setDirty(bool dirty)
{
    if (m_dirty != dirty) {
        m_dirty = dirty;
        emit modificationChanged(dirty);
    }
}

// ---------------------------------------------------------------------------
// BaseEditorPlaceholder
// ---------------------------------------------------------------------------

BaseEditorPlaceholder::BaseEditorPlaceholder(const QString& filePath, QWidget* parent)
    : BaseEditor(filePath, parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* label  = new QLabel(filePath, this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    setLayout(layout);
    load(filePath);
}

void BaseEditorPlaceholder::load(const QString& path)
{
    setFilePath(path);
    setDirty(false);
}

bool BaseEditorPlaceholder::save()
{
    setDirty(false);
    return true;
}
