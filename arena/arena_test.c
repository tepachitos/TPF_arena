// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <TPF/arena.h>

static void test_dummy(void **state) {
  (void)state;

  assert_int_equal(TPF_ARENA_VERSION_MAJOR, 1);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_dummy),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
