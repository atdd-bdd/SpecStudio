add_executable(SpecStudioAskPass main.cpp)

target_compile_features(SpecStudioAskPass PRIVATE cxx_std_17)

# Must land next to SpecStudio.exe so GitClient can find it via
# QCoreApplication::applicationDirPath().
set_target_properties(SpecStudioAskPass PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
)
