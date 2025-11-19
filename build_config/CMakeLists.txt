# Module settings
set(TPF_ARENA_VERSION_MAJOR 1)
set(TPF_ARENA_VERSION_MINOR 0)
set(TPF_ARENA_VERSION_PATCH 0)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/TPF/build_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/TPF/build_config.h"
)
add_library(tpf_build_config INTERFACE)
target_include_directories(tpf_build_config INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
