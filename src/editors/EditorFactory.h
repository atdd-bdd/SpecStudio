#pragma once

#include <QString>

class BaseEditor;
class QWidget;
class AppSettings;

class EditorFactory
{
public:
    static BaseEditor* create(const QString& absolutePath,
                              AppSettings*   settings,
                              QWidget*       parent = nullptr);
};
