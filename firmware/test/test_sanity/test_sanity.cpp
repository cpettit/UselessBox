#include <unity.h>

void test_toolchain_runs(void) {
    TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_toolchain_runs);
    return UNITY_END();
}
