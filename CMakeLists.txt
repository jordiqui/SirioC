cmake_minimum_required(VERSION 3.16)
project(SirioC VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIRIOC_BUILD_TESTS "Build the SirioC test suite" ON)

add_library(sirio_core
    src/nn/accumulator.c
    src/nn/evaluate.c
    src/files/fen.cpp
    src/files/pgn_loader.cpp
    src/nn/evaluator.cpp
    src/pyrrhic/board.cpp
    src/pyrrhic/engine.cpp
    src/pyrrhic/tbchess.c
    src/pyrrhic/tbprobe.c
    src/pyrrhic/types.cpp)

target_include_directories(sirio_core PUBLIC src)

target_compile_features(sirio_core PUBLIC cxx_std_20)

target_compile_options(sirio_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->)

add_executable(sirioc src/pyrrhic/main.cpp)
target_link_libraries(sirioc PRIVATE sirio_core)

if(SIRIOC_BUILD_TESTS)
    enable_testing()
    add_executable(sirio_tests test/engine_tests.cpp)
    target_link_libraries(sirio_tests PRIVATE sirio_core)
    add_test(NAME sirio_unit_tests COMMAND sirio_tests)
endif()
