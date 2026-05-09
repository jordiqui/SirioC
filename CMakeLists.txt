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
    src/evaluation_route.cpp
    src/history.cpp
    src/nnue/backend.cpp
    src/nnue/api.cpp
    src/nnue/features.cpp
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

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
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
    tests/evaluation_route_harness_tests.cpp
    tests/perft_quiescence.cpp
    tests/perft_tests.cpp
    tests/search_tests.cpp
    tests/nnue_backend_tests.cpp
    tests/nnue_features_tests.cpp
    tests/nnue_roundtrip_tests.cpp
    tests/nnue_inference_v2_tests.cpp
    tests/nnue_eval_probe_v2_tests.cpp
    tests/nnue_feature_diff_v2_tests.cpp
    tests/nnue_accumulator_delta_v2_tests.cpp
    tests/nnue_accumulator_transition_v2_tests.cpp
    tests/nnue_board_shadow_v2_tests.cpp
    tests/nnue_experimental_runtime_v2_tests.cpp
    tests/nnue_evaluation_shadow_hook_v2_tests.cpp
    tests/evaluation_shadow_integration_v2_tests.cpp
    tests/internal_eval_backend_selector_v2_tests.cpp
    tests/evaluation_internal_selector_v2_tests.cpp
    tests/evaluation_cpp_shadow_wrapper_v2_tests.cpp
    tests/evaluation_initialization_shadow_v2_tests.cpp
    tests/nnue_api_build_info_v2_tests.cpp
    tests/nnue_format_detection_v2_tests.cpp
    tests/tt_tests.cpp
    tests/history_tests.cpp
    tests/time_manager_tests.cpp
)

target_link_libraries(sirio_tests PRIVATE sirio_core)

add_executable(sirio_feature_dump
    tests/nnue_feature_dump.cpp
)

target_link_libraries(sirio_feature_dump PRIVATE sirio_core)

add_executable(sirio_bench
    bench/bench.cpp
)

target_link_libraries(sirio_bench PRIVATE sirio_core)

add_executable(sirio_nnue_runtime_smoke_contract
    tests/nnue_runtime_smoke_contract.cpp
)

target_link_libraries(sirio_nnue_runtime_smoke_contract PRIVATE sirio_core)


if (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "MSYS")
    if (MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(_sirio_static_link_opts
            -static
            -static-libstdc++
            -static-libgcc
            -Wl,-Bstatic
            -lwinpthread
            -Wl,-Bdynamic
        )
        foreach(target_name IN ITEMS sirio sirio_tests sirio_bench sirio_nnue_runtime_smoke_contract)
            target_link_options(${target_name} PRIVATE ${_sirio_static_link_opts})
        endforeach()
    endif()
endif()

enable_testing()
add_test(NAME sirio_board_tests COMMAND sirio_tests)
