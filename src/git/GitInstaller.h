#pragma once

#include <QString>

class QWidget;

// Detects whether git is installed and, if not, offers to download and run
// the official Git for Windows installer.
namespace GitInstaller
{
    bool isGitInstalled();

    // Ensures git is installed, prompting to download+install it if missing.
    // Shows the real installer UI and blocks (without freezing SpecStudio's own
    // UI) until it exits. Returns true if git is available by the time this
    // returns. `contextLabel` is used in dialog text, e.g. "clone a solution".
    bool ensureGitInstalled(QWidget* parent, const QString& contextLabel);
}
