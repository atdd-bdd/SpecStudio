cmake_minimum_required(VERSION 3.21)
project(AlignThree VERSION 0.9.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Visual Studio picks up vcpkg's user-wide MSBuild integration (vcpkg
# integrate install), which runs applocal.ps1 after every link to copy the
# vcpkg DLLs a target needs beside it. Nothing here comes from vcpkg -- Qt and
# the vendored Hunspell are the dependencies -- so the step copies nothing, and
# on a machine without PowerShell 7 it prints "'pwsh.exe' is not recognized"
# before falling back to Windows PowerShell and succeeding. Off per target, so
# the build log says only what happened and the global vcpkg install is left
# alone. Must be set before the targets are created.
set(CMAKE_VS_GLOBALS "VcpkgApplocalDeps=false")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui PrintSupport Network)

add_subdirectory(third_party/hunspell)
add_subdirectory(converter)
add_subdirectory(askpass)
add_subdirectory(src)
# A console front end for Analyze, so its findings can be diffed from a terminal.
add_subdirectory(tools/analyze_cli)
add_subdirectory(tools/spell_cli)
add_subdirectory(tools/diff_cli)
add_subdirectory(tools/cursor_cli)
add_subdirectory(tools/complete_cli)
