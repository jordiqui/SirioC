cmake_minimum_required(VERSION 3.16)
project(SirioC LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sirio_core
    src/board.cpp
    src/draws.cpp
    src/endgame.cpp
    src/evaluation.cpp
    src/move.cpp
    src/movegen.cpp
    src/search.cpp
    src/syzygy.cpp
    third_party/fathom/tbprobe.c
)

target_include_directories(sirio_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fathom
)

target_compile_features(sirio_core PUBLIC cxx_std_20)

add_executable(sirio
    src/main.cpp
)

target_link_libraries(sirio PRIVATE sirio_core)

add_executable(sirio_tests
    tests/board_tests.cpp
)

target_link_libraries(sirio_tests PRIVATE sirio_core)

add_executable(sirio_bench
    bench/bench.cpp
)

target_link_libraries(sirio_bench PRIVATE sirio_core)

if (WIN32)
    if (MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        foreach(target_name IN ITEMS sirio sirio_tests)
            target_link_options(${target_name} PRIVATE -static -static-libstdc++ -static-libgcc)
        endforeach()
    endif()
endif()

enable_testing()
add_test(NAME sirio_board_tests COMMAND sirio_tests)

