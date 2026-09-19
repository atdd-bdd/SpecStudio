# Exercises DiffView::compare from a terminal, so the line-up of two versions
# can be seen without opening the IDE:
#
#   diff_cli old.spectable new.spectable
#
# prints one row per aligned line, marked =, -, + or ~ (changed). Runs with
# the offscreen platform, since DiffView is a widget.

add_executable(diff_cli
    main.cpp
    ../../src/ui/DiffView.cpp
)
target_include_directories(diff_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(diff_cli PRIVATE Qt6::Core Qt6::Widgets)
set_target_properties(diff_cli PROPERTIES AUTOMOC ON)
