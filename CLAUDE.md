# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Qt 6.10.0 is installed at `C:\Qt\6.10.0\msvc2022_64`. CMake is at `C:\Qt\Tools\CMake_64\bin\cmake.exe`. Visual Studio 2022 Community is the compiler.

```powershell
# Configure (run once, or after CMakeLists.txt changes)
$cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$qt6Dir = "C:\Qt\6.10.0\msvc2022_64\lib\cmake\Qt6"
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DQt6_DIR=$qt6Dir"

# Build
& $cmake --build build --config Debug
# Executable: build\src\Debug\AlignThree.exe

# Run (Qt DLLs must be on PATH or deployed via windeployqt)
$env:PATH += ";C:\Qt\6.10.0\msvc2022_64\bin"
.\build\src\Debug\AlignThree.exe
```

## Project Overview

AlignThree™ (formerly named SpecStudio; the repository keeps the old name) is a cross-platform IDE (Qt 6 / C++17 / CMake) for teams collaborating on specifications. The UI is modeled after Visual Studio. The original specification lives in `archive/To Do.txt`.

## Technology Stack

- **Framework:** Qt 6.10 / C++17
- **Build system:** CMake (AUTOMOC/AUTORCC enabled)
- **Compiler:** MSVC 2022 (Windows); Clang/GCC on other platforms
- **Git integration:** `QProcess` wrapping the `git` CLI — no libgit2

## Core Domain Concepts

**Solution** — top-level container for related projects. Persisted as `<name>.sspec` (JSON) in the solution root folder.

**Project** — specifications for one project. Lives in a subfolder of the solution root. Has its own git repository (`git init` is called when a project is created).

**File types:**
- `.feature` — Gherkin feature files (native editor with syntax highlighting)
- `.featurex` — extended feature files (native editor + `Data`/`import` syntax highlighting)
- `.txt` — plain text (native editor)
- `.csv`, `.xls`, `.xlsx` — spreadsheet (plain text fallback; configurable external editor)
- `.md` — Markdown (plain text fallback; configurable external editor)
- Other — plain text fallback

## Source Layout

```
src/
  app/           MainWindow, AppController (mediator), AppSettings (QSettings wrapper)
  model/         Solution, Project, ProjectFile, FileType, SolutionSerializer (.sspec JSON)
  ui/            SolutionExplorer, EditorTabWidget, OutputPanel, StatusBarManager
  ui/dialogs/    NewSolutionDialog, NewProjectDialog, GitPushDialog, SettingsDialog
  editors/       BaseEditor, PlainTextEditor, FeatureEditor, FeatureXEditor,
                 ExternalEditor, EditorFactory
  editors/syntax/ GherkinHighlighter, FeatureXHighlighter (QSyntaxHighlighter)
  git/           GitClient (QProcess wrapper), GitStatusCache (30s polling timer)
  analyzer/      ProjectIndex, FeatureXAnalyzer, AnalysisResult
  build/         BuildController (QProcess), BuildOutputParser
```

## Architecture

**AppController** (`src/app/AppController.h`) is the central mediator. It owns:
- `Solution*` — the currently open solution
- `AppSettings*` — typed wrapper over QSettings (INI at AppDataLocation)
- `SolutionTreeModel*` — drives the Solution Explorer QTreeView
- `ProjectIndex*` / `FeatureXAnalyzer*` — featurex analysis
- `BuildController*` — invokes the external translator

**AppSettings** keys (per-project settings are keyed by MD5 hash of project root path):
```
[Window]          geometry, state
[Editors]         extension/.<ext> = "" (built-in) or program path
[Projects/<hash>] gitRemoteUrl, gitBranch, gitUser, gitPassword
[Projects/<hash>/Featurex] implicitFolderImport, uniqueScenarioNames,
                            uniqueStepNames, stepSuggestionScope
[RecentSolutions] array of path values
```

**Solution persistence** — `SolutionSerializer` reads/writes JSON `.sspec` files:
```json
{ "version": 1, "name": "MySolution", "projects": [{ "name": "FrontEnd", "relativePath": "FrontEnd" }] }
```
The file list is never persisted — `Project::scanFiles()` re-scans with `QDirIterator` on load.

**Editor dispatch** — `EditorFactory::create()` (`src/editors/EditorFactory.cpp`) maps `FileType` to the correct editor. After Phase 8, user-configured external editors take precedence.

**File → Save** — calls `BaseEditor::save()`, then `Project::git()->commitAll("Auto-save")` for the project that owns the file.

**Git Commit and Push** — `GitPushDialog` prompts for a change reason, then calls `GitClient::commitAndPush(reason, "origin", branch)` where `branch` comes from `AppSettings::gitBranch(projectRoot)`.

**Analyze** — rebuilds `ProjectIndex` for all projects, runs `FeatureXAnalyzer`, and populates the Analysis tab in `OutputPanel`. Double-clicking a diagnostic navigates to that file/line.

## .featurex Semantics

- `Data` statements are project-wide; names must be unique across the project.
- Implicit folder import (whether Data from other folders is auto-included) is configurable per-project.
- Scenario/Step name uniqueness and step autocomplete scope are all configurable in Settings → FeatureX.
- `import "xxx"` adds steps from the named file to autocomplete suggestions.
