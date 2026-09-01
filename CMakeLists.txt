cmake_minimum_required(VERSION 3.15)
project(elementary VERSION 0.11.5)

# Default to a Release build (optimizations on, NDEBUG defined) when the
# caller doesn't specify one. Without this, a plain `cmake -S . -B build`
# silently produces an unoptimized build with debug logging enabled, which
# gives meaningless numbers for elembench. Only applies to single-config
# generators (Makefiles, Ninja); multi-config generators (Xcode, MSVC)
# select the configuration at build time instead.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

option(ONLY_BUILD_WASM "Only build the wasm subdirectory" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(${ONLY_BUILD_WASM})
  add_subdirectory(runtime)
  add_subdirectory(wasm)
else()
  enable_testing()
  add_subdirectory(runtime)
  add_subdirectory(cli)
  add_subdirectory(cli-native)
  add_subdirectory(tests)
endif()
