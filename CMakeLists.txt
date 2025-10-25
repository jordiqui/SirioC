cmake_minimum_required(VERSION 3.16)
project(SirioC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sirio_core
    src/board.cpp
    src/endgame.cpp
    src/evaluation.cpp
    src/move.cpp
    src/movegen.cpp
    src/search.cpp
)

target_include_directories(sirio_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
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

enable_testing()
add_test(NAME sirio_board_tests COMMAND sirio_tests)

