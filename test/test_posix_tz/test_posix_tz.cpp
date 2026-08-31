// Pure-logic suite for lib/PosixTz: turning a UTC offset in seconds into
// a fixed-offset POSIX TZ string (the inverted-sign convention trips
// people up, and the half-hour zones are easy to get wrong).
//
// Runs in BOTH test environments (see platformio.ini):
//   pio test -e native     - on the host, no board required
//   pio test -e esp32dev   - on the device, alongside the firmware suites
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <unity.h>

#include "PosixTz.h"

using tz::buildFixedOffsetTz;

void setUp(void) {}
void tearDown(void) {}

void test_utc(void) {
  TEST_ASSERT_EQUAL_STRING("UTC+0", buildFixedOffsetTz(0).c_str());
}

void test_whole_hours_east_of_utc(void) {
  // Berlin summer, UTC+2 -> POSIX "UTC-2" (local = UTC - offset).
  TEST_ASSERT_EQUAL_STRING("UTC-2", buildFixedOffsetTz(2 * 3600).c_str());
  TEST_ASSERT_EQUAL_STRING("UTC-1", buildFixedOffsetTz(3600).c_str());
  // Kiribati, UTC+14.
  TEST_ASSERT_EQUAL_STRING("UTC-14", buildFixedOffsetTz(14 * 3600).c_str());
}

void test_whole_hours_west_of_utc(void) {
  // New York winter, UTC-5 -> POSIX "UTC+5".
  TEST_ASSERT_EQUAL_STRING("UTC+5", buildFixedOffsetTz(-5 * 3600).c_str());
  TEST_ASSERT_EQUAL_STRING("UTC+11", buildFixedOffsetTz(-11 * 3600).c_str());
}

void test_half_hour_zones(void) {
  // India, UTC+5:30 -> POSIX "UTC-5:30".
  TEST_ASSERT_EQUAL_STRING("UTC-5:30", buildFixedOffsetTz(5 * 3600 + 1800).c_str());
  // Newfoundland, UTC-3:30 -> POSIX "UTC+3:30".
  TEST_ASSERT_EQUAL_STRING("UTC+3:30", buildFixedOffsetTz(-(3 * 3600 + 1800)).c_str());
  // Chatham Islands, UTC+12:45.
  TEST_ASSERT_EQUAL_STRING("UTC-12:45", buildFixedOffsetTz(12 * 3600 + 45 * 60).c_str());
}

static int run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_utc);
  RUN_TEST(test_whole_hours_east_of_utc);
  RUN_TEST(test_whole_hours_west_of_utc);
  RUN_TEST(test_half_hour_zones);
  return UNITY_END();
}

#ifdef ARDUINO
void setup() {
  delay(2000);  // let the board settle before the test runner talks over serial
  run_all_tests();
}
void loop() {}
#else
int main(void) { return run_all_tests(); }
#endif
