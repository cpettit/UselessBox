#include <unity.h>
#include <ChannelController.h>
#include <Channel.h>
#include "../fakes.h"

static ChannelConfig cfg() {
    ChannelConfig c;
    c.homeAngle = 10; c.outAngle = 100;
    c.reactHoldMs = 300; c.returnMs = 200;
    return c;
}

// helpers shared across tests
static FakeClock clk;
static FakeSwitchInput sw;
static RecordingServoOutput servo;

void setUp(void) {
    clk.now = 0;
    for (int i = 0; i < 7; i++) { sw.states[i] = false; servo.last[i] = 255; }
    servo.writes = 0;
}

void test_begin_writes_home_to_all(void) {
    Channel chs[2] = { Channel(cfg()), Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 2, 1, 50);
    c.begin();
    TEST_ASSERT_EQUAL_UINT8(10, servo.last[0]);
    TEST_ASSERT_EQUAL_UINT8(10, servo.last[1]);
}

void test_switch_on_drives_servo_out(void) {
    Channel chs[1] = { Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 1, 1, 50);
    c.begin();
    c.tick();                 // t=0: debouncer initializes OFF, no reaction
    sw.set(0, true);
    clk.advance(60);          // hold ON past 50ms debounce
    c.tick();                 // debounced ON -> trigger
    TEST_ASSERT_EQUAL_UINT8(100, servo.last[0]); // arm out
}

void test_stagger_limits_concurrency(void) {
    Channel chs[2] = { Channel(cfg()), Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 2, 1, 50);  // maxConcurrent = 1
    c.begin();
    c.tick();
    sw.set(0, true); sw.set(1, true);
    clk.advance(60);
    c.tick();                 // only ONE channel may actuate
    TEST_ASSERT_EQUAL_UINT8(1, c.busyCount());
    bool oneOut = (servo.last[0] == 100) != (servo.last[1] == 100);
    TEST_ASSERT_TRUE(oneOut); // exactly one arm out
}

void test_second_channel_runs_after_first_returns(void) {
    Channel chs[2] = { Channel(cfg()), Channel(cfg()) };
    ChannelController c(sw, servo, clk, chs, 2, 1, 50);
    c.begin();
    c.tick();
    sw.set(0, true); sw.set(1, true);
    clk.advance(60);
    c.tick();                 // ch A actuates
    clk.advance(600);         // let A fully finish (300 hold + 200 return + margin)
    c.tick();                 // A returns to idle; B may now actuate
    c.tick();
    TEST_ASSERT_EQUAL_UINT8(1, c.busyCount()); // now B is the busy one
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_writes_home_to_all);
    RUN_TEST(test_switch_on_drives_servo_out);
    RUN_TEST(test_stagger_limits_concurrency);
    RUN_TEST(test_second_channel_runs_after_first_returns);
    return UNITY_END();
}
