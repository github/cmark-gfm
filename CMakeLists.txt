cmake_minimum_required(VERSION 3.0)

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()
if(POLICY CMP0092)
  cmake_policy(SET CMP0092 NEW)
endif()

project(cmark-gfm)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 29)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION_GFM 13)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.gfm.${PROJECT_VERSION_GFM})

include("FindAsan.cmake")
include("CheckFileOffsetBits.cmake")
include(CTest)
include(GNUInstallDirs)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "Do not build in-source.\nPlease remove CMakeCache.txt and the CMakeFiles/ directory.\nThen: mkdir build ; cd build ; cmake .. ; make")
endif()

option(CMARK_STATIC "Build static libcmark-gfm library" ON)
option(CMARK_SHARED "Build shared libcmark-gfm library" ON)
option(CMARK_LIB_FUZZER "Build libFuzzer fuzzing harness" OFF)
option(CMARK_FUZZ_QUADRATIC "Build quadratic fuzzing harness" OFF)

if(NOT MSVC)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_STANDARD_REQUIRED YES)
  set(CMAKE_C_EXTENSIONS NO)
endif()

# -fvisibility=hidden
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

if(NOT MSVC OR CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
  include(InstallRequiredSystemLibraries)
endif()


# Compiler flags
if(MSVC)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/W4>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/wd4706>)
  # Compile as C++ under MSVC older than 12.0
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/TP>)
  add_compile_definitions($<$<COMPILE_LANGUAGE:C>:_CRT_SECURE_NO_WARNINGS>)
  # BEGIN cmark-gfm
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/wd4204>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/wd4211>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/wd4100>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/WX>)
  # END cmark-gfm
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES Clang)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wall>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wextra>)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-pedantic>)
  # BEGIN cmark-gfm
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>)
  # END cmark-gfm
endif()

# Check integrity of node structure when compiled as debug
add_compile_definitions($<$<CONFIG:Debug>:CMARK_DEBUG_NODES>)

add_compile_options($<$<AND:$<CONFIG:PROFILE>,$<COMPILE_LANGUAGE:C>>:-pg>)

if(CMAKE_BUILD_TYPE STREQUAL Ubsan)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-fsanitize=undefined>)
endif()
if(CMARK_LIB_FUZZER)
  add_compile_options($<$<COMPILE_LANGUAGE:C>:-fsanitize-coverage=trace-pc-guard>)
endif()

if(CMARK_FUZZ_QUADRATIC)
  set(FUZZER_FLAGS "-fsanitize=fuzzer-no-link,address -g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUZZER_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FUZZER_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FUZZER_FLAGS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${FUZZER_FLAGS}")
endif()

add_subdirectory(src)
add_subdirectory(extensions)
# TODO(compnerd) should this be enabled for MinGW, which sets CMAKE_SYSTEM_NAME
# to Windows, but defines `MINGW`.
if(NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
  add_subdirectory(man)
endif()
if(BUILD_TESTING)
  add_subdirectory(test testdir)
  if(CMARK_STATIC OR CMARK_SHARED)
    add_subdirectory(api_test)
  endif()
endif()
if(CMARK_FUZZ_QUADRATIC)
  add_subdirectory(fuzz)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
  "Choose the type of build, options are: Debug Profile Release Asan Ubsan." FORCE)
endif(NOT CMAKE_BUILD_TYPE)
