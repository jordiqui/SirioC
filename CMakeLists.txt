cmake_minimum_required(VERSION 3.20)
project(SirioC LANGUAGES CXX)

option(ENABLE_LTO "Enable link-time optimization" ON)
option(ENABLE_AVX2 "Enable AVX2 where available" ON)

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
    add_compile_options(-mavx2 -mbmi2)
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

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE SRC_FILES
  src/*.cpp
)

add_executable(SirioC ${SRC_FILES})

if (MINGW)
  target_link_libraries(SirioC PRIVATE stdc++ stdc++fs)
endif()

target_compile_definitions(SirioC PRIVATE
  ENGINE_NAME=\"SirioC\"
  ENGINE_VERSION=\"0.1.0\"
)
