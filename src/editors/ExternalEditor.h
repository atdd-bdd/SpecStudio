#pragma once

#include "BaseEditor.h"

// Opens a file in an external program (configured via Settings).
// Acts as a placeholder tab that shows the file path and launches the app.
class ExternalEditor : public BaseEditor
{
    Q_OBJECT

public:
    ExternalEditor(const QString& filePath,
                   const QString& externalProgram,
                   QWidget* parent = nullptr);

    void load(const QString& path) override;
    bool save() override;

private:
    QString m_program;
};
