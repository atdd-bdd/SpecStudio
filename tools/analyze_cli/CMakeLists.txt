# A console front end for the IDE's Analyze, so its output can be seen and
# diffed from a terminal rather than only through the menu item.
#
# It compiles the analyzer sources directly rather than linking the application:
# AlignThree is a GUI target, and the analyzer depends on nothing in it.

add_executable(analyze_cli
    main.cpp
    ../../src/analyzer/SpecTableAnalyzer.cpp
    ../../src/analyzer/SpecTableIndex.cpp
    ../../src/analyzer/SpecTableModelChecks.cpp
    ../../converter/SpectableParser.cpp
)

target_include_directories(analyze_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../converter)

target_link_libraries(analyze_cli PRIVATE Qt6::Core)
