# SpecStudio

A Visual Studio-style IDE for writing and managing specification files, with built-in C# unit test generation.

## Overview

SpecStudio supports `.spectable` files — a structured DSL for defining entities, business rules, calculations, and test scenarios. It can generate C# MSTest/NUnit/xUnit scaffolding from those specifications automatically, producing typed data classes and a test class per scenario.

Other supported file types: `.feature` (Gherkin), `.featurex` (extended Gherkin with `Data`/`import`), `.csv`, `.md`, `.txt`, and configurable external editors for anything else.

## Features

- Syntax highlighting, code folding, bracket matching, and autocomplete for `.spectable` and `.feature*` files
- Go to Definition (F12), Find All References (Shift+F12), and project-wide Rename (F2)
- Symbol tree and attribute inspector panels
- Grid editor for pipe tables, string dialog for triple-quoted text
- Analysis diagnostics with inline squiggles and Output panel jump-to-line
- `.specconfig` project configuration for the unit test converter
- Per-project git integration (commit, push, pull, diff, conflict resolution)
- Split editor, dark theme, configurable fonts
- Recent solutions, print support

## Building

**Requirements:** Qt 6.10, Visual Studio 2022, CMake 3.16+

```powershell
$cmake  = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$qt6Dir = "C:\Qt\6.10.0\msvc2022_64\lib\cmake\Qt6"

# Configure
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DQt6_DIR=$qt6Dir"

# Build SpecStudio
& $cmake --build build --config Debug --target SpecStudio

# Build the converter (optional — needed for unit test generation)
& $cmake --build build --config Debug --target SpecTableConverter
```

Executables:
- `build\src\Debug\SpecStudio.exe`
- `build\converter\Debug\SpecTableConverter.exe`

To run without installing Qt:

```powershell
$env:PATH += ";C:\Qt\6.10.0\msvc2022_64\bin"
.\build\src\Debug\SpecStudio.exe
```

## Unit Test Generation

1. Write a `.spectable` file describing your entities, rules, and scenarios.
2. Create a `.specconfig` file in the project folder (open it in SpecStudio for a form editor):

```json
{
    "version": 1,
    "outputDirectory": "generated",
    "language": "CSharp",
    "framework": "MSTest",
    "namespace": "gherkinexecutor",
    "overwriteGlue": false,
    "converterPath": ""
}
```

3. **Build > Build Current File** — generates `*String.cs`, `*Typed.cs`, `*_Tests.cs`, and `*_glue.cs` into the output directory.
4. Fill in the glue stubs (`*_glue.cs`) to call your production code. Glue files are never overwritten.

`converterPath` can be left blank — SpecStudio auto-detects `SpecTableConverter.exe` next to its own executable, or in the Visual Studio dev-build location.

## Project Structure

```
src/
  app/           MainWindow, AppController, AppSettings, ThemeManager
  model/         Solution, Project, ProjectFile, FileType, SpecConfig, SolutionSerializer
  ui/            SolutionExplorer, EditorTabWidget, OutputPanel, StatusBarManager,
                 AttributeInspectorPanel, EntityTreePanel, dialogs/
  editors/       BaseEditor, PlainTextEditor, FeatureEditor, FeatureXEditor,
                 SpecTableEditor, SpecConfigEditor, ExternalEditor, EditorFactory,
                 LineNumberEdit, syntax/
  git/           GitClient, GitStatusCache
  analyzer/      ProjectIndex, FeatureXAnalyzer, SpecTableIndex, SpecTableAnalyzer
  build/         BuildController, BuildOutputParser
converter/
  SpectableParser.h/.cpp
  CSharpGenerator.h/.cpp
  SpectableModel.h
  main.cpp
```

## Technology

- **Framework:** Qt 6.10 / C++17
- **Build:** CMake (AUTOMOC/AUTORCC)
- **Compiler:** MSVC 2022 (Windows); Clang/GCC on other platforms
- **Git integration:** `QProcess` wrapping the `git` CLI

## Documentation

See [SpecStudio User Guide.md](SpecStudio%20User%20Guide.md) for full syntax reference, keyboard shortcuts, and glue code patterns.
