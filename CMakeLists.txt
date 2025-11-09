cmake_minimum_required(VERSION 3.16)
project(SirioC LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIRIO_ENABLE_AVX2 "Enable AVX2 optimizations" ON)
option(SIRIO_ENABLE_AVX512 "Enable AVX-512 optimizations" OFF)

add_library(sirio_core
    src/board.cpp
    src/bitboard_tables.cpp
    src/draws.cpp
    src/endgame.cpp
    src/evaluation.cpp
    src/nnue/backend.cpp
    src/nnue/api.cpp
    src/move.cpp
    src/movegen.cpp
    src/opening_book.cpp
    src/engine/work_queue_watchdog.cpp
    src/search.cpp
    src/time_manager.cpp
    src/tt.cpp
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

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(sirio_core PUBLIC atomic)
endif()

if (SIRIO_ENABLE_AVX512)
    target_compile_definitions(sirio_core PUBLIC SIRIO_USE_AVX512)
    if (MSVC)
        target_compile_options(sirio_core PUBLIC /arch:AVX512)
    else()
        target_compile_options(sirio_core PUBLIC -mavx512f -mavx512dq -mavx512vl)
    endif()
elseif (SIRIO_ENABLE_AVX2)
    target_compile_definitions(sirio_core PUBLIC SIRIO_USE_AVX2)
    if (MSVC)
        target_compile_options(sirio_core PUBLIC /arch:AVX2)
    else()
        target_compile_options(sirio_core PUBLIC -mavx2 -mfma)
    endif()
endif()

add_executable(sirio
    src/main.cpp
    src/uci.cpp
)

target_link_libraries(sirio PRIVATE sirio_core)

add_executable(sirio_tests
    tests/board_tests.cpp
    tests/evaluation_tests.cpp
    tests/perft_quiescence.cpp
    tests/perft_tests.cpp
    tests/search_tests.cpp
    tests/nnue_backend_tests.cpp
    tests/tt_tests.cpp
    tests/time_manager_tests.cpp
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

