add_library(tpf_arena STATIC)
target_sources(tpf_arena PRIVATE arena.c)
target_link_libraries(tpf_arena PRIVATE SDL3::SDL3)
target_include_directories(tpf_arena INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# Unit testing
setup_test(NAME arena LIBS SDL3::SDL3 tpf_arena)
add_memcheck_target(arena_test)
