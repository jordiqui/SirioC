cmake_minimum_required(VERSION 3.16)
project(SirioC VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

option(SIRIOC_BUILD_TESTS "Build the SirioC test suite" ON)

find_package(Threads REQUIRED)

set(_sirio_embed_default OFF)
if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _sirio_build_type)
    if(_sirio_build_type STREQUAL "RELEASE")
        set(_sirio_embed_default ON)
    endif()
endif()
option(SIRIOC_EMBED_NNUE "Embed the default NNUE network in Release builds" ${_sirio_embed_default})
unset(_sirio_build_type)
unset(_sirio_embed_default)

add_library(sirio_core
    src/bits.c
    src/eval.c
    src/nn/accumulator.c
    src/nn/evaluate.c
    src/files/fen.cpp
    src/files/pgn_loader.cpp
    src/nn/evaluator.cpp
    src/nn/nnue_loader.cpp
    src/pyrrhic/board.cpp
    src/pyrrhic/engine.cpp
    src/pyrrhic/bench.cpp
    src/pyrrhic/path.cpp
    src/pyrrhic/tbchess.c
    src/pyrrhic/tbprobe.c
    src/pyrrhic/types.cpp
    src/nn/nnue.cpp
    src/uci/Options.cpp
    src/uci/Uci.cpp)

target_include_directories(sirio_core PUBLIC
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nn
)

target_compile_features(sirio_core PUBLIC cxx_std_20)

target_compile_options(sirio_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->)

if (MSVC)
    add_compile_options($<$<CONFIG:Release>:/O2 /Oi /Ot /GL /arch:AVX2 /fp:fast>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
else()
    add_compile_options(
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CONFIG:Release>:-march=native>
        $<$<CONFIG:Release>:-mtune=native>
        $<$<CONFIG:Release>:-flto>
        $<$<CONFIG:Release>:-fno-exceptions>
        $<$<CONFIG:Release>:-fno-rtti>
        $<$<CONFIG:Release>:-fomit-frame-pointer>
    )
endif()

if (CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(sirio_core PUBLIC -msse4.1 -mpopcnt)
elseif (MSVC)
    target_compile_options(sirio_core PUBLIC /arch:AVX)
endif()

if(SIRIOC_EMBED_NNUE)
    set(_sirio_nnue_binary "${CMAKE_CURRENT_SOURCE_DIR}/resources/network.dat")
    if(EXISTS "${_sirio_nnue_binary}")
        set(_sirio_nnue_generated "${CMAKE_CURRENT_BINARY_DIR}/nnue_default.c")
        add_custom_command(
            OUTPUT "${_sirio_nnue_generated}"
            COMMAND ${CMAKE_COMMAND} -DINPUT="${_sirio_nnue_binary}" -DOUTPUT="${_sirio_nnue_generated}" -DSYMBOL=g_sirio_nnue_default -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedNNUE.cmake"
            DEPENDS "${_sirio_nnue_binary}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedNNUE.cmake"
            COMMENT "Generating embedded NNUE network")
        set_source_files_properties("${_sirio_nnue_generated}" PROPERTIES GENERATED TRUE)
        add_library(nnue_default_obj OBJECT "${_sirio_nnue_generated}")
        target_compile_features(nnue_default_obj PRIVATE c_std_99)
        target_sources(sirio_core PRIVATE $<TARGET_OBJECTS:nnue_default_obj>)
        add_dependencies(sirio_core nnue_default_obj)
        target_compile_definitions(sirio_core PRIVATE SIRIOC_EMBED_NNUE)
    else()
        message(WARNING "SIRIOC_EMBED_NNUE is enabled but resources/network.dat was not found")
    endif()
    unset(_sirio_nnue_binary)
    unset(_sirio_nnue_generated)
endif()

add_executable(sirioc_cpp src/pyrrhic/main.cpp)
target_link_libraries(sirioc_cpp PRIVATE sirio_core)

set(SIRIOC_C_ENGINE_SOURCES
    src/SirioC.c
    src/attacks.c
    src/bench.c
    src/bits.c
    src/board.c
    src/eval.c
    src/history.c
    src/move.c
    src/movegen.c
    src/movepick.c
    src/perft.c
    src/random.c
    src/search.c
    src/see.c
    src/tb.c
    src/thread.c
    src/transposition.c
    src/uci.c
    src/util.c
    src/nn/accumulator.c
    src/nn/evaluate.c
    src/zobrist.cpp
    vendor/fathom/tbprobe.c
)

add_executable(sirioc ${SIRIOC_C_ENGINE_SOURCES})
target_include_directories(sirioc PRIVATE src vendor/fathom)
target_link_libraries(sirioc PRIVATE Threads::Threads)
if (UNIX)
    target_link_libraries(sirioc PRIVATE m)
endif()

if(SIRIOC_BUILD_TESTS)
    enable_testing()
    add_executable(sirio_tests test/engine_tests.cpp)
    target_link_libraries(sirio_tests PRIVATE sirio_core)
    add_test(NAME sirio_unit_tests COMMAND sirio_tests)
endif()
