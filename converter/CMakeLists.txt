find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(SpecTableConverter
    main.cpp
    SpectableParser.cpp
    TagFilter.cpp
    CSharpGenerator.cpp
    JavaGenerator.cpp
    RustGenerator.cpp
    PythonGenerator.cpp
    CppGenerator.cpp
    JavaScriptGenerator.cpp
    TypeScriptGenerator.cpp
    GoGenerator.cpp
    SwiftGenerator.cpp
)

target_compile_features(SpecTableConverter PRIVATE cxx_std_17)
target_link_libraries(SpecTableConverter PRIVATE Qt6::Core)
target_include_directories(SpecTableConverter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
