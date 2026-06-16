#pragma once

class QApplication;

class ThemeManager
{
public:
    static void applyDark(QApplication* app);
    static void applyLight(QApplication* app);
    static void apply(QApplication* app, bool dark);
};
