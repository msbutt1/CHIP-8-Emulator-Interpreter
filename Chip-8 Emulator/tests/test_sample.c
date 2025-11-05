#include "unity.h"
#include "../core/chip8.h"

void setUp(void) {}
void tearDown(void) {}

static void test_version_non_null(void) {
  TEST_ASSERT_NOT_NULL(chip8_core_version());
}

static void test_version_matches_macro(void) {
  TEST_ASSERT_EQUAL_STRING(CHIP8_VERSION, chip8_core_version());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_version_non_null);
  RUN_TEST(test_version_matches_macro);
  return UNITY_END();
}


