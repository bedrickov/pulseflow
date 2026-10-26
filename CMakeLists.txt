cmake_minimum_required(VERSION 3.20)
project(pulseflow VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

find_package(Threads REQUIRED)

# Core static library
add_library(pulseflow_core STATIC
    src/mmap_reader.cpp
    src/parser.cpp
    src/pipeline.cpp
    src/sink.cpp
)

target_include_directories(pulseflow_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pulseflow_core PUBLIC
    Threads::Threads
)

# CLI executable
add_executable(pulseflow src/cli/main.cpp)
target_link_libraries(pulseflow PRIVATE pulseflow_core)

# Unit tests executable
enable_testing()
add_executable(pulseflow_tests tests/test_main.cpp)
target_link_libraries(pulseflow_tests PRIVATE pulseflow_core)

add_test(NAME UnitTests COMMAND pulseflow_tests)
