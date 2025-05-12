cmake_minimum_required(VERSION 3.15)
project(CCPU)

set(CMAKE_CXX_STANDARD 23)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_SPECIFIC_COVERAGE_FLAGS "-fcoverage-mapping -fprofile-instr-generate -O0 -g")
else()
    set(COMPILER_SPECIFIC_COVERAGE_FLAGS "")
endif()

set(COMPILER_WARNING_FLAGS "-Werror -Wall -Wpedantic -Wno-unused-command-line-argument")
set(COMPILER_SANITIZER_FLAGS "-fsanitize=address -fsanitize=undefined")
set(COMPILER_CONSTEXPR_LIMIT_FLAGS "-fconstexpr-steps=4194304")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_SPECIFIC_COVERAGE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_SANITIZER_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_CONSTEXPR_LIMIT_FLAGS}")

add_executable(main src/target/main.cpp)
target_include_directories(main PUBLIC src)

add_subdirectory(test/unittest)

enable_testing()