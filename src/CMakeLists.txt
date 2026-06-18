set(SOURCES
    main.cpp

    app/AppController.h
    app/AppController.cpp
    app/MainWindow.h
    app/MainWindow.cpp

    editors/BaseEditor.h
    editors/BaseEditor.cpp

    ui/SolutionExplorer.h
    ui/SolutionExplorer.cpp
    ui/EditorTabWidget.h
    ui/EditorTabWidget.cpp
    ui/OutputPanel.h
    ui/OutputPanel.cpp
    ui/StatusBarManager.h
    ui/StatusBarManager.cpp

    model/FileType.h
    model/ProjectFile.h
    model/ProjectFile.cpp
    model/Project.h
    model/Project.cpp
    model/Solution.h
    model/Solution.cpp
    model/SolutionSerializer.h
    model/SolutionSerializer.cpp

    ui/SolutionTreeModel.h
    ui/SolutionTreeModel.cpp

    ui/dialogs/NewSolutionDialog.h
    ui/dialogs/NewSolutionDialog.cpp
    ui/dialogs/NewProjectDialog.h
    ui/dialogs/NewProjectDialog.cpp

    editors/LineNumberEdit.h
    editors/LineNumberEdit.cpp
    editors/PlainTextEditor.h
    editors/PlainTextEditor.cpp

    git/GitClient.h
    git/GitClient.cpp

    editors/EditorFactory.h
    editors/EditorFactory.cpp
    editors/FeatureEditor.h
    editors/FeatureEditor.cpp
    editors/FeatureXEditor.h
    editors/FeatureXEditor.cpp
    editors/ExternalEditor.h
    editors/ExternalEditor.cpp

    editors/syntax/GherkinHighlighter.h
    editors/syntax/GherkinHighlighter.cpp
    editors/syntax/FeatureXHighlighter.h
    editors/syntax/FeatureXHighlighter.cpp
    editors/syntax/SpecTableHighlighter.h
    editors/syntax/SpecTableHighlighter.cpp
    editors/SpecTableEditor.h
    editors/SpecTableEditor.cpp

    git/GitStatusCache.h
    git/GitStatusCache.cpp

    ui/dialogs/GitPushDialog.h
    ui/dialogs/GitPushDialog.cpp
    ui/dialogs/SettingsDialog.h
    ui/dialogs/SettingsDialog.cpp
    ui/dialogs/NewFileDialog.h
    ui/dialogs/NewFileDialog.cpp
    ui/dialogs/FindReplaceDialog.h
    ui/dialogs/FindReplaceDialog.cpp
    ui/dialogs/ConflictResolutionDialog.h
    ui/dialogs/ConflictResolutionDialog.cpp
    ui/dialogs/GridDialog.h
    ui/dialogs/GridDialog.cpp
    ui/dialogs/StringDialog.h
    ui/dialogs/StringDialog.cpp
    ui/dialogs/AttributeTableDialog.h
    ui/dialogs/AttributeTableDialog.cpp

    app/AppSettings.h
    app/AppSettings.cpp
    app/ThemeManager.h
    app/ThemeManager.cpp

    analyzer/AnalysisResult.h
    analyzer/ProjectIndex.h
    analyzer/ProjectIndex.cpp
    analyzer/FeatureXAnalyzer.h
    analyzer/FeatureXAnalyzer.cpp
    analyzer/SpecTableIndex.h
    analyzer/SpecTableIndex.cpp
    analyzer/SpecTableAnalyzer.h
    analyzer/SpecTableAnalyzer.cpp

    build/BuildController.h
    build/BuildController.cpp
    build/BuildOutputParser.h
    build/BuildOutputParser.cpp
)

qt_add_executable(SpecStudio WIN32 MACOSX_BUNDLE
    ${SOURCES}
)

target_include_directories(SpecStudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SpecStudio PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::PrintSupport
)
