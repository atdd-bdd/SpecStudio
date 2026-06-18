#pragma once

#include "PlainTextEditor.h"

class QMenu;
class SpecTableIndex;

class SpecTableEditor : public PlainTextEditor
{
    Q_OBJECT

public:
    explicit SpecTableEditor(const QString& filePath, QWidget* parent = nullptr);

    void setIndex(SpecTableIndex* index) { m_index = index; }

protected:
    void populateContextMenu(QMenu* menu) override;

private:
    SpecTableIndex* m_index = nullptr;
};
