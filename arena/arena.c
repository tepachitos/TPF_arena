#include "TPF/arena.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_error.h>

static size_t align_is_pow2(const size_t a) {
  return a && ((a & (a - 1)) == 0);
}

TPF_Arena *TPF_CreateArena(const size_t size) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(size > 0 && "TPF_CreateHeapArena: size cannot be zero");
  // clang-format on

  TPF_Arena *arena = SDL_malloc(sizeof(TPF_Arena));
  if (arena == NULL) {
    return NULL;
  }

  arena->data_base = SDL_malloc(size);
  if (arena->data_base == NULL) {
    SDL_free(arena);
    return NULL;
  }

  arena->data_offset = 0;
  arena->data_size = size;
  return arena;
}

void TPF_DestroyArena(TPF_Arena *arena) {
  if (arena == NULL) {
    return;
  }

  if (arena->data_base != NULL) {
    SDL_free(arena->data_base);
  }

  SDL_free(arena);
}

void *TPF_ArenaTryPush(TPF_Arena *arena, const size_t size) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaTryPush: arena must not be NULL");
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(size > 0 && "TPF_ArenaTryPush: size must not be zero");
  // clang-format on

  if (size > TPF_ArenaRemaining(arena)) {
    return NULL;
  }

  const size_t mark = arena->data_offset;
  arena->data_offset += size;
  return arena->data_base + mark;
}

void *TPF_ArenaPush(TPF_Arena *arena, const size_t size) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaPush: arena must not be NULL");
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(size > 0 && "TPF_ArenaPush: size must not be zero");
  // clang-format on

  void *ptr = TPF_ArenaTryPush(arena, size);
  if (ptr == NULL) {
    SDL_SetError(
        "failed to allocate %zu bytes of memory in pool (remaining %zu)", size,
        arena->data_size - arena->data_offset);
    return NULL;
  }

  return ptr;
}

void *TPF_ArenaPushZeroes(TPF_Arena *arena, const size_t size) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaPushZeroes: arena must not be NULL");
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(size > 0 && "TPF_ArenaPushZeroes: size must not be zero");
  // clang-format on

  void *ptr = TPF_ArenaTryPush(arena, size);
  if (ptr == NULL) {
    return NULL;
  }

  SDL_memset(ptr, 0, size);
  return ptr;
}

void *TPF_ArenaTryAlignedPush(TPF_Arena *arena, const size_t alignment, const size_t size) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaTryAlignedPush: arena must not be NULL");
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(size > 0 && "TPF_ArenaTryAlignedPush: size must not be zero");
  // clang-format on

  if (!align_is_pow2(alignment)) {
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    SDL_assert(false &&
               "TPF_ArenaTryAlignedPush: alignment is not a power of 2");
    return NULL;
  }

  Uint8 *head = arena->data_base + arena->data_offset;
  uintptr_t addr = (uintptr_t)head;
  size_t mask = alignment - 1;
  size_t pad = (size_t)((alignment - (addr & mask)) & mask);

  size_t remaining = arena->data_size - arena->data_offset;
  if (pad > remaining || size > remaining - pad) {
    return NULL;
  }

  arena->data_offset += pad + size;
  return head + pad;
}

void *TPF_ArenaAlignedPush(TPF_Arena *arena, const size_t alignment, const size_t size) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaAlignedPush: arena must not be NULL");
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(size > 0 && "TPF_ArenaAlignedPush: size must not be zero");
  // clang-format on

  void *ptr = TPF_ArenaTryAlignedPush(arena, alignment, size);
  if (ptr == NULL) {
    SDL_SetError("failed to allocate %zu aligned bytes of memory in pool "
                 "(remaining %zu)",
                 size, arena->data_size - arena->data_offset);
    return NULL;
  }

  return ptr;
}

void *TPF_ArenaAlignedPushZeroes(TPF_Arena *arena, const size_t alignment,
                                 const size_t size) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaAlignedPushZeroes: arena must not be NULL");
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(size > 0 && "TPF_ArenaAlignedPushZeroes: size must not be zero");
  // clang-format on

  void *ptr = TPF_ArenaTryAlignedPush(arena, alignment, size);
  if (ptr == NULL) {
    return NULL;
  }

  SDL_memset(ptr, 0, size);
  return ptr;
}

size_t TPF_ArenaMark(const TPF_Arena *arena) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaMark: arena must not be NULL");
  // clang-format on
  return arena->data_offset;
}

void TPF_ArenaResetTo(TPF_Arena *arena, const size_t mark) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaRestTo: arena must not be NULL");
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(mark <= arena->data_size && "TPF_ArenaRestTo: mark out of bounds");
  // clang-format on

#ifdef TPF_ARENA_DEBUG
  // poison so valgrind yells at us
  SDL_memset(arena->data_base, 0xDD, arena->data_offset);
#endif
  arena->data_offset = mark;
}

void TPF_ArenaClear(TPF_Arena *arena) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaClear: arena must not be NULL");
  // clang-format on

#ifdef TPF_ARENA_DEBUG
  // poison so valgrind yells at us
  SDL_memset(arena->data_base, 0xDD, arena->data_offset);
#endif
  arena->data_offset = 0;
}

size_t TPF_ArenaRemaining(const TPF_Arena *arena) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaRemaining: arena must not be NULL");
  // clang-format on
  return arena->data_size - arena->data_offset;
}

size_t TPF_ArenaUsed(const TPF_Arena *arena) {
  // clang-format off
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  SDL_assert_paranoid(arena != NULL && "TPF_ArenaUsed: arena must not be NULL");
  // clang-format on
  return arena->data_offset;
}
