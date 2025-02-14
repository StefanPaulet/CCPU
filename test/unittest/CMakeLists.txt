include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

add_executable(unittests unitTest.cpp
    registerTest.cpp
    aluTest.cpp)

target_link_libraries(unittests PRIVATE Catch2::Catch2WithMain)

target_include_directories(unittests PRIVATE ../../src)

set_target_properties(
    unittests
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)