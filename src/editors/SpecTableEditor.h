#pragma once

#include "PlainTextEditor.h"

class SpecTableEditor : public PlainTextEditor
{
    Q_OBJECT

public:
    explicit SpecTableEditor(const QString& filePath, QWidget* parent = nullptr);

    void editTable()  override;
    void editString() override;
};
