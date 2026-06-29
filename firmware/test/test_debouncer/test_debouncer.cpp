#include <unity.h>
#include <Debouncer.h>

void test_initializes_to_first_reading(void) {
    Debouncer d(50);
    TEST_ASSERT_TRUE(d.update(true, 0));
    TEST_ASSERT_TRUE(d.stable());
}

void test_transient_bounce_is_ignored(void) {
    Debouncer d(50);
    d.update(false, 0);            // stable = false
    d.update(true, 10);            // raw goes high...
    bool s = d.update(false, 30);  // ...but drops before 50ms window
    TEST_ASSERT_FALSE(s);          // never became stable-high
}

void test_sustained_change_flips_after_window(void) {
    Debouncer d(50);
    d.update(false, 0);
    d.update(true, 10);            // raw high at t=10
    TEST_ASSERT_FALSE(d.update(true, 59));  // 49ms held: not yet
    TEST_ASSERT_TRUE(d.update(true, 60));   // 50ms held: flips
}

void test_boundary_is_inclusive(void) {
    Debouncer d(50);
    d.update(false, 0);
    d.update(true, 100);
    TEST_ASSERT_TRUE(d.update(true, 150)); // exactly 50ms -> flips
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_initializes_to_first_reading);
    RUN_TEST(test_transient_bounce_is_ignored);
    RUN_TEST(test_sustained_change_flips_after_window);
    RUN_TEST(test_boundary_is_inclusive);
    return UNITY_END();
}
