cmake_minimum_required(VERSION 3.20)
project(SirioC LANGUAGES CXX)

option(ENABLE_LTO "Enable link-time optimization" ON)
option(ENABLE_AVX2 "Enable AVX2 where available" OFF)

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
  add_compile_options(-O3 -fno-exceptions -fno-rtti -DNDEBUG)
  if (ENABLE_AVX2)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    check_cxx_compiler_flag("-mbmi2" COMPILER_SUPPORTS_BMI2)
    if (COMPILER_SUPPORTS_AVX2 AND COMPILER_SUPPORTS_BMI2)
      add_compile_options(-mavx2 -mbmi2)
      add_compile_definitions(SIRIOC_USE_AVX2=1)
    else()
      message(WARNING "AVX2/BMI2 flags requested but not supported by the compiler; building without them.")
    endif()
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

file(GLOB_RECURSE SRC_FILES
  src/*.cpp
)

set(MAIN_SOURCE ${CMAKE_SOURCE_DIR}/src/main.cpp)
list(REMOVE_ITEM SRC_FILES ${MAIN_SOURCE})

add_library(sirio_core ${SRC_FILES})
target_include_directories(sirio_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
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
add_test(NAME go_command_responds_with_legal_move
         COMMAND Python3::Interpreter
                 ${CMAKE_SOURCE_DIR}/tests/test_go.py
                 $<TARGET_FILE:SirioC>
                 $<TARGET_FILE:legal_moves_cli>)
add_test(NAME board_fen_tests COMMAND SirioCTests)
