# Asks CursorModel what is at each line of a specification, from a terminal:
#
#   cursor_cli file.spectable [line ...]
#
# so the editor's reading of a line -- step, table row, Examples:, block start,
# named comment, Import, Insert -- can be checked without opening the IDE.

add_executable(cursor_cli
    main.cpp
    ../../src/editors/CursorModel.cpp
    ../../converter/SpectableParser.cpp
)
target_include_directories(cursor_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../converter)
target_link_libraries(cursor_cli PRIVATE Qt6::Core)
