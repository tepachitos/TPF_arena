// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <SDL3/SDL_error.h>  // SDL_GetError, SDL_ClearError
#include <SDL3/SDL_stdinc.h> // Uint8
#include <TPF/arena.h>

static void assert_aligned(const void *p, size_t alignment) {
  uintptr_t v = (uintptr_t)p;
  assert_true((v & (alignment - 1)) == 0);
}

static TPF_ArenaConfig cfg(size_t max_arena_size, size_t max_chunk_size,
                           size_t def_chunk_size) {
  TPF_ArenaConfig c;
  c.max_arena_size = max_arena_size; // 0 = unlimited (per your impl)
  c.max_chunk_size = max_chunk_size; // 0 = unlimited (recommended)
  c.def_chunk_size = def_chunk_size;
  return c;
}

static void test_create_destroy(void **state) {
  (void)state;

  TPF_ArenaConfig c = cfg(/*max_arena_size*/ 4096, /*max_chunk_size*/ 1024,
                          /*def_chunk_size*/ 256);
  TPF_Arena *a = TPF_ArenaCreate(c);
  assert_non_null(a);

  assert_int_equal(TPF_ArenaUsed(a), 0);

  // Remaining is "budget remaining until max_arena_size"
  // At creation, total_cap should be 0, so remaining == max_arena_size.
  assert_int_equal(TPF_ArenaRemaining(a), 4096);

  // No active chunk yet.
  assert_int_equal(TPF_ArenaTailRemaining(a), 0);

  TPF_ArenaDestroy(a);
}

static void test_try_push_basic_and_tail_remaining(void **state) {
  (void)state;

  // Keep it simple: one chunk of 256 max.
  TPF_ArenaConfig config = cfg(/*max_arena_size*/ 1024, /*max_chunk_size*/ 256,
                               /*def_chunk_size*/ 256);
  TPF_Arena *arena = TPF_ArenaCreate(config);
  assert_non_null(arena);

  // First alloc should create a chunk (256) and consume 16 (alignment 8).
  void *ptr1 = TPF_ArenaTryPush(arena, 8, 16);
  assert_non_null(ptr1);
  assert_aligned(ptr1, 8);

  assert_int_equal(TPF_ArenaUsed(arena), 16);
  assert_int_equal(TPF_ArenaTailRemaining(arena), 256 - 16);

  // Another alloc: still in same chunk
  void *ptr2 = TPF_ArenaTryPush(arena, 8, 32);
  assert_non_null(ptr2);
  assert_aligned(ptr2, 8);
  assert_int_equal(TPF_ArenaUsed(arena), 16 + 32);

  // Tail remaining should decrease accordingly (padding might apply, but with 8
  // and sizes multiple of 8, pad should be 0 here).
  assert_int_equal(TPF_ArenaTailRemaining(arena), 256 - (16 + 32));

  TPF_ArenaDestroy(arena);
}

static void
test_push_sets_error_on_failure_due_to_max_arena_size(void **state) {
  (void)state;

  // max_arena_size=256, max_chunk_size=256, def=256 means:
  // First allocation creates one 256 chunk. Second allocation that doesn't fit
  // tail would require a new chunk but should fail due to arena budget.
  TPF_ArenaConfig config = cfg(/*max_arena_size*/ 256, /*max_chunk_size*/ 256,
                               /*def_chunk_size*/ 256);
  TPF_Arena *arena = TPF_ArenaCreate(config);
  assert_non_null(arena);

  // Fill most of the first chunk
  void *ptr1 = TPF_ArenaTryPush(arena, 8, 240);
  assert_non_null(ptr1);
  assert_int_equal(TPF_ArenaUsed(arena), 240);

  // Now request something that cannot fit in remaining 16 bytes
  SDL_ClearError();
  void *q = TPF_ArenaPush(arena, 8, 32);
  assert_null(q);

  const char *err = SDL_GetError();
  assert_non_null(err);
  assert_true(err[0] != '\0');

  TPF_ArenaDestroy(arena);
}

static void test_push_zeroes_writes_zero(void **state) {
  (void)state;

  TPF_ArenaConfig config = cfg(/*max_arena_size*/ 1024, /*max_chunk_size*/ 256,
                               /*def_chunk_size*/ 256);
  TPF_Arena *arena = TPF_ArenaCreate(config);
  assert_non_null(arena);

  Uint8 *ptr1 = (Uint8 *)TPF_ArenaPushZeroes(arena, 16, 64);
  assert_non_null(ptr1);
  assert_aligned(ptr1, 16);

  for (size_t i = 0; i < 64; i++) {
    assert_int_equal(ptr1[i], 0);
  }

  TPF_ArenaDestroy(arena);
}

static void test_alignment_various(void **state) {
  (void)state;

  TPF_ArenaConfig config = cfg(/*max_arena_size*/ 4096, /*max_chunk_size*/ 512,
                               /*def_chunk_size*/ 512);
  TPF_Arena *arena = TPF_ArenaCreate(config);
  assert_non_null(arena);

  void *ptr8 = TPF_ArenaTryPush(arena, 8, 1);
  assert_non_null(ptr8);
  assert_aligned(ptr8, 8);

  void *ptr16 = TPF_ArenaTryPush(arena, 16, 1);
  assert_non_null(ptr16);
  assert_aligned(ptr16, 16);

  void *ptr64 = TPF_ArenaTryPush(arena, 64, 1);
  assert_non_null(ptr64);
  assert_aligned(ptr64, 64);

  // Pointers should be in non-decreasing order for monotonic bump within a
  // chunk.
  assert_true((uintptr_t)ptr16 >= (uintptr_t)ptr8);
  assert_true((uintptr_t)ptr64 >= (uintptr_t)ptr16);

  TPF_ArenaDestroy(arena);
}

static void test_mark_and_reset_to_reuses_space(void **state) {
  (void)state;

  // Chunk size 128 so we can push enough to potentially require a second chunk.
  TPF_ArenaConfig config = cfg(/*max_arena_size*/ 512, /*max_chunk_size*/ 128,
                               /*def_chunk_size*/ 128);
  TPF_Arena *arena = TPF_ArenaCreate(config);
  assert_non_null(arena);

  void *ptr1 = TPF_ArenaTryPush(arena, 8, 32);
  assert_non_null(ptr1);

  const TPF_ArenaMark m1 = TPF_ArenaGetMark(arena);
  assert_int_equal(TPF_ArenaUsed(arena), 32);

  // Push enough to force a new chunk: remaining in first chunk is 96, so 120
  // forces a second.
  void *ptr2 = TPF_ArenaTryPush(arena, 8, 120);
  assert_non_null(ptr2);

  // Used should be >= 32 + 120 (padding might add a few bytes, but with 8 it
  // should be exact)
  assert_true(TPF_ArenaUsed(arena) >= 152);

  // Reset to mark (should drop the second chunk and restore tail used)
  TPF_ArenaResetTo(arena, m1);
  assert_true(TPF_ArenaUsed(arena) == 32);

  // After reset, allocation should reuse space in the first chunk (address
  // should match p1+32)
  Uint8 *p3 = (Uint8 *)TPF_ArenaTryPush(arena, 8, 16);
  assert_non_null(p3);
  assert_true((uintptr_t)p3 == (uintptr_t)ptr1 + 32);

  // Clear should reset arena to empty and free active chunks into free list
  // internally
  TPF_ArenaClear(arena);
  assert_int_equal(TPF_ArenaUsed(arena), 0);

  TPF_ArenaDestroy(arena);
}

static void test_try_push_zeroes_does_not_set_error(void **state) {
  (void)state;

  TPF_ArenaConfig config = cfg(/*max_arena_size*/ 128, /*max_chunk_size*/ 128,
                               /*def_chunk_size*/ 128);
  TPF_Arena *arena = TPF_ArenaCreate(config);
  assert_non_null(arena);

  // Fill most of the chunk
  void *ptr1 = TPF_ArenaTryPush(arena, 8, 120);
  assert_non_null(ptr1);
  SDL_ClearError();

  // This should fail (cannot fit and cannot grow because max_arena_size
  // prevents a new chunk)
  void *ptr2 = TPF_ArenaTryPushZeroes(arena, 8, 32);
  assert_null(ptr2);

  // "Try" variant should not set SDL error (may be empty string)
  const char *err = SDL_GetError();
  // SDL returns "" when no error; some builds return NULL; accept both.
  if (err) {
    assert_true(err[0] == '\0');
  }

  TPF_ArenaDestroy(arena);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_create_destroy),
      cmocka_unit_test(test_try_push_basic_and_tail_remaining),
      cmocka_unit_test(test_push_sets_error_on_failure_due_to_max_arena_size),
      cmocka_unit_test(test_push_zeroes_writes_zero),
      cmocka_unit_test(test_alignment_various),
      cmocka_unit_test(test_mark_and_reset_to_reuses_space),
      cmocka_unit_test(test_try_push_zeroes_does_not_set_error),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
