#pragma once

#include "PlainTextEditor.h"

class FeatureEditor : public PlainTextEditor
{
    Q_OBJECT

public:
    explicit FeatureEditor(const QString& filePath, QWidget* parent = nullptr);
};
