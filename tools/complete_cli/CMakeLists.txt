# Prints the .spectable editor's suggestion lists from a terminal:
#
#   complete_cli <folder> [--and <folder>] [file.spectable ...]
#
# so a change to what autocomplete offers can be checked without opening the
# IDE and typing. It compiles the list builders directly -- they were moved out
# of SpecTableEditor into CompletionLists.cpp for exactly this reason, and
# depend on nothing in the GUI.

add_executable(complete_cli
    main.cpp
    ../../src/editors/CompletionLists.cpp
    ../../src/analyzer/SpecTableIndex.cpp
    ../../converter/SpectableParser.cpp
)

target_include_directories(complete_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../converter)

target_link_libraries(complete_cli PRIVATE Qt6::Core)
