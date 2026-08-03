#pragma once

#include <QCoreApplication>
#include <QString>

// AlignThree ships with two helper executables that must sit beside it:
// SpecTableConverter (the code generator) and AlignThreeAskPass (the git
// credential helper). Both are found through applicationDirPath(), so the
// packaging scripts put all three in the same folder — on macOS that is
// AlignThree.app/Contents/MacOS.
//
// Only Windows gives them an .exe suffix. Spelling that suffix into the lookup
// meant the Linux and macOS builds silently found neither tool: Build reported
// "No converter found" and git fell back to its own prompt.
namespace toolpath {

// The file name a helper executable has on this platform.
inline QString exeName(const QString& baseName)
{
#ifdef Q_OS_WIN
    return baseName + ".exe";
#else
    return baseName;
#endif
}

// Full path to a helper executable deployed next to the running application.
inline QString siblingTool(const QString& baseName)
{
    return QCoreApplication::applicationDirPath() + "/" + exeName(baseName);
}

} // namespace toolpath
