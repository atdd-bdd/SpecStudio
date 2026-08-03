add_executable(AlignThreeAskPass main.cpp)

target_compile_features(AlignThreeAskPass PRIVATE cxx_std_17)

# Must land next to AlignThree.exe so GitClient can find it via
# QCoreApplication::applicationDirPath().
set_target_properties(AlignThreeAskPass PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
)
