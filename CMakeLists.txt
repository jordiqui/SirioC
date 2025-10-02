cmake_minimum_required(VERSION 3.16)
project(SirioC VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

option(SIRIOC_BUILD_TESTS "Build the SirioC test suite" ON)

set(SIRIOC_NNUE_PRIMARY_URL "https://tests.stockfishchess.org/api/nn/nn-1c0000000000.nnue"
    CACHE STRING "URL to fetch the default (primary) NNUE network when auto-download is enabled")
set(SIRIOC_NNUE_SMALL_URL "https://tests.stockfishchess.org/api/nn/nn-37f18f62d772.nnue"
    CACHE STRING "URL to fetch the small NNUE network when auto-download is enabled")
option(SIRIOC_AUTO_DOWNLOAD_NNUE "Attempt to download default NNUE networks during configure if missing" OFF)

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
    src/engine/core/bits.c
    src/engine/eval/eval.c
    src/engine/nn/accumulator.c
    src/engine/nn/evaluate.c
    src/files/fen.cpp
    src/files/pgn_loader.cpp
    src/engine/nn/evaluator.cpp
    src/engine/nn/nnue_paths.c
    src/engine/nn/nnue_loader.cpp
    src/pyrrhic/board.cpp
    src/pyrrhic/engine.cpp
    src/pyrrhic/bench.cpp
    src/pyrrhic/path.cpp
    src/pyrrhic/tbchess.c
    src/pyrrhic/tbprobe.c
    src/pyrrhic/types.cpp
    src/engine/nn/nnue.cpp
    src/uci/Options.cpp
    src/uci/Uci.cpp)

target_include_directories(sirio_core PUBLIC
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/eval
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/nn
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/search
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/tb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/uci
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/util
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

if(SIRIOC_AUTO_DOWNLOAD_NNUE)
    set(_sirio_resources_dir "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    set(_sirio_primary_nnue "${_sirio_resources_dir}/nn-1c0000000000.nnue")
    set(_sirio_small_nnue "${_sirio_resources_dir}/nn-37f18f62d772.nnue")

    file(MAKE_DIRECTORY "${_sirio_resources_dir}")

    function(_sirio_try_download_nnue url destination label)
        if(EXISTS "${destination}")
            return()
        endif()

        message(STATUS "Downloading ${label} NNUE network from ${url}")
        file(DOWNLOAD "${url}" "${destination}"
             STATUS download_status
             SHOW_PROGRESS
             TLS_VERIFY ON)
        list(GET download_status 0 download_code)
        if(NOT download_code EQUAL 0)
            list(GET download_status 1 download_error)
            file(REMOVE "${destination}")
            message(WARNING "Failed to download ${label} NNUE network (${download_error})."
                            " Use scripts/download_nnue.sh or disable SIRIOC_AUTO_DOWNLOAD_NNUE to skip this step.")
        else()
            message(STATUS "Saved ${label} NNUE network to ${destination}")
        endif()
    endfunction()

    _sirio_try_download_nnue("${SIRIOC_NNUE_PRIMARY_URL}" "${_sirio_primary_nnue}" "primary")
    _sirio_try_download_nnue("${SIRIOC_NNUE_SMALL_URL}" "${_sirio_small_nnue}" "small")

    unset(_sirio_resources_dir)
    unset(_sirio_primary_nnue)
    unset(_sirio_small_nnue)
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
    src/engine/SirioC.c
    src/engine/core/attacks.c
    src/engine/search/bench.c
    src/engine/core/bits.c
    src/engine/core/board.c
    src/engine/eval/eval.c
    src/engine/core/history.c
    src/engine/core/move.c
    src/engine/core/movegen.c
    src/engine/core/movepick.c
    src/engine/search/perft.c
    src/engine/core/random.c
    src/engine/search/search.c
    src/engine/search/see.c
    src/engine/tb/tb.c
    src/engine/search/thread.c
    src/engine/search/transposition.c
    src/engine/uci/uci.c
    src/engine/util/util.c
    src/engine/util/path.cpp
    src/engine/nn/accumulator.c
    src/engine/nn/evaluate.c
    src/engine/nn/nnue_paths.c
    src/engine/core/zobrist.cpp
    vendor/fathom/tbprobe.c
)

add_executable(sirioc ${SIRIOC_C_ENGINE_SOURCES})
target_include_directories(sirioc PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/eval
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/nn
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/search
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/tb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/uci
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/util
    vendor/fathom)
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
