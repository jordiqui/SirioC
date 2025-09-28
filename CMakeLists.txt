cmake_minimum_required(VERSION 3.20)
project(SirioC LANGUAGES C CXX)

option(ENABLE_LTO "Enable link-time optimization" ON)
option(ENABLE_AVX2 "Enable AVX2/BMI2 where available" OFF)
option(ENABLE_SSSE3 "Enable SSSE3 where available" OFF)
option(ENABLE_SSE41_POPCNT "Enable SSE4.1 and POPCNT where available" ON)
option(ENABLE_NATIVE "Enable -march=native when available" OFF)
option(ENABLE_VNNI "Enable AVX-512 VNNI where available" OFF)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if (MSVC)
  add_compile_options(/O2 /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-O3 -DNDEBUG)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
                      $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
  include(CheckCXXCompilerFlag)

  set(SIRIOC_FEATURE_FLAGS "")

  if (ENABLE_NATIVE)
    check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
    if (COMPILER_SUPPORTS_MARCH_NATIVE)
      list(APPEND SIRIOC_FEATURE_FLAGS -march=native)
    else()
      message(WARNING "-march=native requested but not supported by the compiler; building without it.")
    endif()
  endif()

  if (ENABLE_SSSE3)
    check_cxx_compiler_flag("-mssse3" COMPILER_SUPPORTS_SSSE3)
    if (COMPILER_SUPPORTS_SSSE3)
      list(APPEND SIRIOC_FEATURE_FLAGS -mssse3)
      add_compile_definitions(SIRIOC_USE_SSSE3=1)
    else()
      message(WARNING "SSSE3 flag requested but not supported by the compiler; building without it.")
    endif()
  endif()

  if (ENABLE_SSE41_POPCNT)
    check_cxx_compiler_flag("-msse4.1" COMPILER_SUPPORTS_SSE41)
    check_cxx_compiler_flag("-mpopcnt" COMPILER_SUPPORTS_POPCNT)
    if (COMPILER_SUPPORTS_SSE41)
      list(APPEND SIRIOC_FEATURE_FLAGS -msse4.1)
      add_compile_definitions(SIRIOC_USE_SSE41=1)
    else()
      message(WARNING "SSE4.1 flag requested but not supported by the compiler; building without it.")
    endif()
    if (COMPILER_SUPPORTS_POPCNT)
      list(APPEND SIRIOC_FEATURE_FLAGS -mpopcnt)
      add_compile_definitions(SIRIOC_USE_POPCNT=1)
    else()
      message(WARNING "POPCNT flag requested but not supported by the compiler; building without it.")
    endif()
  endif()

  if (ENABLE_AVX2)
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    check_cxx_compiler_flag("-mbmi2" COMPILER_SUPPORTS_BMI2)
    check_cxx_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
    if (COMPILER_SUPPORTS_AVX2)
      list(APPEND SIRIOC_FEATURE_FLAGS -mavx2)
      add_compile_definitions(SIRIOC_USE_AVX2=1)
    else()
      message(WARNING "AVX2 flag requested but not supported by the compiler; building without it.")
    endif()
    if (COMPILER_SUPPORTS_BMI2)
      list(APPEND SIRIOC_FEATURE_FLAGS -mbmi2)
      add_compile_definitions(SIRIOC_USE_BMI2=1)
    else()
      message(WARNING "BMI2 flag requested but not supported by the compiler; building without it.")
    endif()
    if (COMPILER_SUPPORTS_FMA)
      list(APPEND SIRIOC_FEATURE_FLAGS -mfma)
      add_compile_definitions(SIRIOC_USE_FMA=1)
    endif()
  endif()

  if (ENABLE_VNNI)
    check_cxx_compiler_flag("-mavx512vnni" COMPILER_SUPPORTS_VNNI)
    if (COMPILER_SUPPORTS_VNNI)
      list(APPEND SIRIOC_FEATURE_FLAGS -mavx512vnni)
      add_compile_definitions(SIRIOC_USE_VNNI=1)
    else()
      message(WARNING "AVX-512 VNNI flag requested but not supported by the compiler; building without it.")
    endif()
  endif()

  if (SIRIOC_FEATURE_FLAGS)
    add_compile_options(${SIRIOC_FEATURE_FLAGS})
  endif()

endif()

if (ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
  if (ipo_supported)
    message(STATUS "IPO/LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(STATUS "IPO/LTO not supported: ${ipo_msg}")
  endif()
endif()

set(SIRIOC_SOURCES
  ${CMAKE_SOURCE_DIR}/src/bench.cpp
  ${CMAKE_SOURCE_DIR}/src/core/board.cpp
  ${CMAKE_SOURCE_DIR}/src/core/fen.cpp
  ${CMAKE_SOURCE_DIR}/src/core/movegen.cpp
  ${CMAKE_SOURCE_DIR}/src/core/perft.cpp
  ${CMAKE_SOURCE_DIR}/src/bench.cpp
  ${CMAKE_SOURCE_DIR}/src/eval/eval.cpp
  ${CMAKE_SOURCE_DIR}/src/eval/nnue/accumulator.cpp
  ${CMAKE_SOURCE_DIR}/src/eval/nnue/evaluator.cpp
  ${CMAKE_SOURCE_DIR}/src/tt.cpp
  ${CMAKE_SOURCE_DIR}/src/search/search.cpp
  ${CMAKE_SOURCE_DIR}/src/syzygy/syzygy.cpp
  ${CMAKE_SOURCE_DIR}/src/uci/uci.cpp
  ${CMAKE_SOURCE_DIR}/src/util/log.cpp
  ${CMAKE_SOURCE_DIR}/src/util/time.cpp
  ${CMAKE_SOURCE_DIR}/3rdparty/fathom/tbprobe.c
)

set(MAIN_SOURCE ${CMAKE_SOURCE_DIR}/src/main.cpp)

add_library(sirio_core ${SIRIOC_SOURCES})
target_include_directories(sirio_core PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include/engine/syzygy
  ${CMAKE_SOURCE_DIR}/3rdparty/fathom
)
set_source_files_properties(${CMAKE_SOURCE_DIR}/3rdparty/fathom/tbprobe.c
  PROPERTIES
    LANGUAGE C
)
target_compile_definitions(sirio_core PUBLIC
  ENGINE_NAME="SirioC"
  ENGINE_VERSION="0.1.0"
)

add_executable(SirioC ${MAIN_SOURCE})
target_link_libraries(SirioC PRIVATE sirio_core)

add_executable(legal_moves_cli tests/legal_moves_cli.cpp)
target_link_libraries(legal_moves_cli PRIVATE sirio_core)

add_executable(movegen_tests tests/movegen_tests.cpp)
target_link_libraries(movegen_tests PRIVATE sirio_core)

add_executable(perft_tests tests/perft_tests.cpp)
target_link_libraries(perft_tests PRIVATE sirio_core)

 codex/refactor-static_exchange_eval-for-see
add_executable(see_regression_tests tests/see_regression_tests.cpp)
target_link_libraries(see_regression_tests PRIVATE sirio_core)
=======
 codex/enhance-time-allocation-strategy-in-compute_allocation
add_executable(time_tests tests/time_tests.cpp)
target_link_libraries(time_tests PRIVATE sirio_core)
=======
add_executable(eval_tests tests/eval_tests.cpp)
target_link_libraries(eval_tests PRIVATE sirio_core)
 main
 main

if (MINGW)
  foreach(target SirioC legal_moves_cli movegen_tests perft_tests)
    target_link_libraries(${target} PRIVATE stdc++)
  endforeach()

  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
      CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    foreach(target SirioC legal_moves_cli movegen_tests perft_tests)
      target_link_libraries(${target} PRIVATE stdc++fs)
    endforeach()
  endif()
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(SirioCTests tests/board_fen_tests.cpp)
target_link_libraries(SirioCTests PRIVATE sirio_core)

add_test(NAME movegen_unit_tests COMMAND movegen_tests)
add_test(NAME perft_regression_tests COMMAND perft_tests)
 codex/refactor-static_exchange_eval-for-see
add_test(NAME see_regression_tests COMMAND see_regression_tests)

 codex/enhance-time-allocation-strategy-in-compute_allocation
add_test(NAME time_management_tests COMMAND time_tests)
=======
add_test(NAME evaluation_heuristics_tests COMMAND eval_tests)
 main
 main
add_test(NAME go_command_responds_with_legal_move
         COMMAND Python3::Interpreter
                 ${CMAKE_SOURCE_DIR}/tests/test_go.py
                 $<TARGET_FILE:SirioC>
                 $<TARGET_FILE:legal_moves_cli>)
add_test(NAME board_fen_tests COMMAND SirioCTests)
add_test(NAME bench_smoke_test
         COMMAND Python3::Interpreter
                 ${CMAKE_SOURCE_DIR}/tests/test_bench.py
                 $<TARGET_FILE:SirioC>)
