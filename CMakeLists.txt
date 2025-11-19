cmake_minimum_required(VERSION 3.22)
project(TPF_Arena
  VERSION "1.0.0"
  LANGUAGES C
  DESCRIPTION "TPF Arena implementation")

# CMake configuration
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CMake internals
include(cmake/testing.cmake)
enable_testing()

# System packages
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

# Vendor dependencies
add_subdirectory(vendor)

# Modules
add_subdirectory(build_config)
add_subdirectory(arena)
