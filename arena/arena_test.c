// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <SDL3/SDL_error.h>  // SDL_GetError, SDL_ClearError (optional)
#include <SDL3/SDL_stdinc.h> // Uint8
#include <TPF/arena.h>

static void test_create_destroy(void **state) {
  (void)state;

  TPF_Arena *a = TPF_CreateArena(1024);
  assert_non_null(a);
  assert_non_null(a->data_base);
  assert_int_equal(TPF_ArenaUsed(a), 0);
  assert_int_equal(TPF_ArenaRemaining(a), 1024);

  TPF_DestroyArena(a);
}

static void test_try_push_and_used_remaining(void **state) {
  (void)state;

  TPF_Arena *a = TPF_CreateArena(64);
  assert_non_null(a);

  Uint8 *pa = TPF_ArenaTryPush(a, 16);
  assert_non_null(pa);
  assert_int_equal(TPF_ArenaUsed(a), 16);
  assert_int_equal(TPF_ArenaRemaining(a), 48);

  Uint8 *pb = TPF_ArenaTryPush(a, 48);
  assert_non_null(pb);
  assert_int_equal(TPF_ArenaUsed(a), 64);
  assert_int_equal(TPF_ArenaRemaining(a), 0);

  // No space left
  void *pc = TPF_ArenaTryPush(a, 1);
  assert_null(pc);
  assert_int_equal(TPF_ArenaUsed(a), 64);
  assert_int_equal(TPF_ArenaRemaining(a), 0);

  TPF_DestroyArena(a);
}

static void test_push_sets_error_on_failure(void **state) {
  (void)state;

  TPF_Arena *a = TPF_CreateArena(32);
  assert_non_null(a);

  // Fill it
  Uint8 *tmp = TPF_ArenaTryPush(a, 32);
  assert_non_null(tmp);
  assert_int_equal(TPF_ArenaRemaining(a), 0);

  SDL_ClearError();
  void *p = TPF_ArenaPush(a, 1);
  assert_null(p);

  const char *err = SDL_GetError();
  // We don't assert exact message, just that it's non-empty.
  assert_non_null(err);
  assert_true(err[0] != '\0');

  TPF_DestroyArena(a);
}

static void test_push_zeroes_writes_zero(void **state) {
  (void)state;

  TPF_Arena *a = TPF_CreateArena(128);
  assert_non_null(a);

  Uint8 *p = (Uint8 *)TPF_ArenaPushZeroes(a, 32);
  assert_non_null(p);

  for (size_t i = 0; i < 32; i++) {
    assert_int_equal(p[i], 0);
  }

  TPF_DestroyArena(a);
}

static void assert_aligned(const void *p, size_t alignment) {
  uintptr_t v = (uintptr_t)p;
  assert_true((v & (alignment - 1)) == 0);
}

static void test_aligned_push_alignment_and_no_overlap(void **state) {
  (void)state;

  TPF_Arena *a = TPF_CreateArena(256);
  assert_non_null(a);

  // Make head unaligned on purpose
  void *u = TPF_ArenaTryPush(a, 3);
  assert_non_null(u);

  // Now allocate aligned blocks
  void *p8 = TPF_ArenaTryAlignedPush(a, 8, 8);
  assert_non_null(p8);
  assert_aligned(p8, 8);

  void *p16 = TPF_ArenaTryAlignedPush(a, 16, 16);
  assert_non_null(p16);
  assert_aligned(p16, 16);

  void *p64 = TPF_ArenaTryAlignedPush(a, 64, 1);
  assert_non_null(p64);
  assert_aligned(p64, 64);

  // Basic overlap sanity: each subsequent allocation pointer should be >=
  // previous
  assert_true((uintptr_t)p16 >= (uintptr_t)p8);
  assert_true((uintptr_t)p64 >= (uintptr_t)p16);

  TPF_DestroyArena(a);
}

static void test_mark_and_reset_to(void **state) {
  (void)state;

  TPF_Arena *a = TPF_CreateArena(128);
  assert_non_null(a);

  void *tmp1 = TPF_ArenaTryPush(a, 10);
  assert_non_null(tmp1);
  size_t m1 = TPF_ArenaMark(a);
  assert_int_equal(m1, 10);

  void *tmp2 = TPF_ArenaTryPush(a, 20);
  assert_non_null(tmp2);
  size_t m2 = TPF_ArenaMark(a);
  assert_int_equal(m2, 30);

  // Reset to first mark
  TPF_ArenaResetTo(a, m1);
  assert_int_equal(TPF_ArenaUsed(a), 10);
  assert_int_equal(TPF_ArenaRemaining(a), 118);

  // Re-allocate after reset should reuse same address as the old second alloc
  // would have
  Uint8 *p = (Uint8 *)TPF_ArenaTryPush(a, 5);
  assert_non_null(p);
  assert_int_equal(TPF_ArenaUsed(a), 15);

  // Reset to zero via clear
  TPF_ArenaClear(a);
  assert_int_equal(TPF_ArenaUsed(a), 0);
  assert_int_equal(TPF_ArenaRemaining(a), 128);

  TPF_DestroyArena(a);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_create_destroy),
      cmocka_unit_test(test_try_push_and_used_remaining),
      cmocka_unit_test(test_push_sets_error_on_failure),
      cmocka_unit_test(test_push_zeroes_writes_zero),
      cmocka_unit_test(test_aligned_push_alignment_and_no_overlap),
      cmocka_unit_test(test_mark_and_reset_to),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
