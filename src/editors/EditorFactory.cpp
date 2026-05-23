#include "EditorFactory.h"
#include "FeatureEditor.h"
#include "FeatureXEditor.h"
#include "PlainTextEditor.h"
#include "ExternalEditor.h"
#include "../model/FileType.h"

BaseEditor* EditorFactory::create(const QString& absolutePath,
                                   AppSettings*   settings,
                                   QWidget*       parent)
{
    // Check for user-configured external editor (Phase 8 wires settings properly;
    // for now settings may be nullptr and we fall through to built-in editors).
    if (settings) {
        QString ext = absolutePath.section('.', -1).toLower();
        // (AppSettings::editorForExtension implemented in Phase 8)
    }

    FileType type = fileTypeFromPath(absolutePath);

    switch (type) {
    case FileType::Feature:
        return new FeatureEditor(absolutePath, parent);
    case FileType::FeatureX:
        return new FeatureXEditor(absolutePath, parent);
    case FileType::Text:
        return new PlainTextEditor(absolutePath, parent);
    default:
        // For .csv, .md, .xlsx and Other — open as plain text for now.
        // A configured external editor would take precedence if settings are wired.
        return new PlainTextEditor(absolutePath, parent);
    }
}
