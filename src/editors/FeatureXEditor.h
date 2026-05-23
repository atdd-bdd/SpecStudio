#pragma once

#include "PlainTextEditor.h"

class FeatureXEditor : public PlainTextEditor
{
    Q_OBJECT

public:
    explicit FeatureXEditor(const QString& filePath, QWidget* parent = nullptr);
};
