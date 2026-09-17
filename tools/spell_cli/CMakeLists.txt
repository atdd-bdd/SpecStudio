# A console front end for the spell checker, so that what it accepts and what
# it suggests can be seen from a terminal -- the IDE cannot be driven from one.
#
#   spell_cli TotalScore TotlScore "don't" recieve
#
# prints, for each argument, the words it splits into and whether each is
# spelled correctly, with suggestions for those that are not. It uses the same
# SpellChecker and the same compiled-in dictionary as the editor.

add_executable(spell_cli
    main.cpp
    ../../src/spell/SpellChecker.cpp
    ../../resources/dictionaries.qrc
)

target_include_directories(spell_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(spell_cli PRIVATE Qt6::Core hunspell)
set_target_properties(spell_cli PROPERTIES AUTOMOC ON AUTORCC ON)
