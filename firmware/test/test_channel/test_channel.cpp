#include <unity.h>
#include <Channel.h>

static ChannelConfig cfg() {
    ChannelConfig c;
    c.homeAngle = 10; c.outAngle = 100;
    c.reactHoldMs = 300; c.returnMs = 200;
    return c;
}

void test_starts_idle_at_home(void) {
    Channel ch(cfg());
    TEST_ASSERT_EQUAL(ChannelState::Idle, ch.state());
    TEST_ASSERT_EQUAL_UINT8(10, ch.desiredAngle());
    TEST_ASSERT_FALSE(ch.isBusy());
}

void test_trigger_starts_reaction_out(void) {
    Channel ch(cfg());
    ch.trigger(1000);
    TEST_ASSERT_EQUAL(ChannelState::Reacting, ch.state());
    TEST_ASSERT_EQUAL_UINT8(100, ch.desiredAngle());
    TEST_ASSERT_TRUE(ch.isBusy());
}

void test_holds_then_returns_then_idles(void) {
    Channel ch(cfg());
    ch.trigger(1000);
    ch.update(1299);                                   // 299ms: still reacting
    TEST_ASSERT_EQUAL(ChannelState::Reacting, ch.state());
    ch.update(1300);                                   // 300ms: -> returning
    TEST_ASSERT_EQUAL(ChannelState::Returning, ch.state());
    TEST_ASSERT_EQUAL_UINT8(10, ch.desiredAngle());
    ch.update(1499);                                   // 199ms returning
    TEST_ASSERT_EQUAL(ChannelState::Returning, ch.state());
    ch.update(1500);                                   // 200ms: -> idle
    TEST_ASSERT_EQUAL(ChannelState::Idle, ch.state());
    TEST_ASSERT_FALSE(ch.isBusy());
}

void test_trigger_ignored_while_busy(void) {
    Channel ch(cfg());
    ch.trigger(1000);
    ch.trigger(1100);                                  // ignored
    ch.update(1300);
    TEST_ASSERT_EQUAL(ChannelState::Returning, ch.state()); // timed off first trigger
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_starts_idle_at_home);
    RUN_TEST(test_trigger_starts_reaction_out);
    RUN_TEST(test_holds_then_returns_then_idles);
    RUN_TEST(test_trigger_ignored_while_busy);
    return UNITY_END();
}
