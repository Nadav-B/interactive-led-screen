// Pure-logic suite for lib/ClockText: the "HH:MM" times and "Mon 26 Oct"
// date the scenes draw. What time it *is* comes from NTP plus the static
// POSIX TZ in include/config/location_config.h; this is just the
// formatting.
//
// Runs in BOTH test environments (see platformio.ini):
//   pio test -e native     - on the host, no board required
//   pio test -e esp32dev   - on the device, alongside the firmware suites
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <cstring>
#include <ctime>

#include <unity.h>

#include "ClockText.h"

void setUp(void) {}
void tearDown(void) {}

namespace {
struct tm makeTm(int h, int m, int s) {
  struct tm t;
  std::memset(&t, 0, sizeof(t));
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  return t;
}
}  // namespace

void test_hm_midnight_and_end_of_day(void) {
  char buf[6];
  clockface::formatHm(makeTm(0, 0, 0), buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("00:00", buf);
  clockface::formatHm(makeTm(23, 59, 59), buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("23:59", buf);
}

void test_hm_is_zero_padded_24h(void) {
  char buf[6];
  clockface::formatHm(makeTm(7, 5, 59), buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("07:05", buf);
  clockface::formatHm(makeTm(23, 59, 0), buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("23:59", buf);
}

void test_date_short_form(void) {
  struct tm t = makeTm(12, 0, 0);
  t.tm_wday = 1;
  t.tm_mday = 26;
  t.tm_mon = 9;
  char buf[11];
  clockface::formatDate(t, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("Mon 26 Oct", buf);
}

void test_date_single_digit_day_is_not_padded(void) {
  struct tm t = makeTm(0, 0, 0);
  t.tm_wday = 0;
  t.tm_mday = 5;
  t.tm_mon = 0;
  char buf[11];
  clockface::formatDate(t, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("Sun 5 Jan", buf);
}

static int run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hm_midnight_and_end_of_day);
  RUN_TEST(test_hm_is_zero_padded_24h);
  RUN_TEST(test_date_short_form);
  RUN_TEST(test_date_single_digit_day_is_not_padded);
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
