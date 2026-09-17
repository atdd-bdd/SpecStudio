# Hunspell 1.7.2, the spell-checking library behind LibreOffice, Firefox and
# macOS, vendored here as source so that every platform builds the same
# checker with no package to install. Tri-licensed MPL 1.1 / GPL 2 / LGPL 2.1;
# the licence files sit beside the sources. Only the library is taken -- the
# command-line tools, tests and build system of the upstream tree are not.

add_library(hunspell STATIC
    affentry.cxx
    affixmgr.cxx
    csutil.cxx
    filemgr.cxx
    hashmgr.cxx
    hunspell.cxx
    hunzip.cxx
    phonet.cxx
    replist.cxx
    suggestmgr.cxx
)

target_include_directories(hunspell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(hunspell PUBLIC HUNSPELL_STATIC)
target_compile_features(hunspell PUBLIC cxx_std_11)

if(MSVC)
    # Upstream uses the C string functions freely; the CRT's deprecation
    # warnings about them are noise here, not findings.
    target_compile_definitions(hunspell PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_compile_options(hunspell PRIVATE /w)
else()
    target_compile_options(hunspell PRIVATE -w)
endif()
