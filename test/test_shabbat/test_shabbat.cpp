// Pure-logic suite for lib/Shabbat: whether a moment falls between Friday
// candle lighting and Saturday nightfall, for 52.5118 N, 13.4127 E
// (Berlin). Sunsets are the US Naval Observatory's, as in
// test_sky_events; times here are UTC with Berlin's offset applied by
// hand, so nothing depends on the device's timezone.
//
// Runs in BOTH test environments (see platformio.ini):
//   pio test -e native     - on the host, no board required
//   pio test -e esp32dev   - on the device, alongside the firmware suites
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <ctime>

#include <unity.h>

#include "Shabbat.h"

void setUp(void) {}
void tearDown(void) {}

namespace {
constexpr double kLat = 52.5118;
constexpr double kLon = 13.4127;
constexpr int kFriday = 5, kSaturday = 6, kSunday = 0, kThursday = 4;

// Unix timestamp for y-m-d hh:mm UTC (Howard Hinnant's days_from_civil).
time_t utc(int y, int m, int d, int hh = 0, int mm = 0) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long days = era * 146097L + doe - 719468L;
  return static_cast<time_t>(days * 86400L + hh * 3600L + mm * 60L);
}

// Local (CET, UTC+1) midnight starting y-m-d.
time_t cetMidnight(int y, int m, int d) { return utc(y, m, d) - 3600; }
}  // namespace

// Fri 30 Oct 2026: sunset ~15:41 UTC, so candles at ~15:23 UTC.
void test_friday_before_candle_lighting(void) {
  TEST_ASSERT_FALSE(shabbat::isShabbat(utc(2026, 10, 30, 15, 10), kFriday,
                                       cetMidnight(2026, 10, 30), kLat, kLon));
}

void test_friday_after_candle_lighting(void) {
  TEST_ASSERT_TRUE(shabbat::isShabbat(utc(2026, 10, 30, 15, 35), kFriday,
                                      cetMidnight(2026, 10, 30), kLat, kLon));
  TEST_ASSERT_TRUE(shabbat::isShabbat(utc(2026, 10, 30, 22, 50), kFriday,
                                      cetMidnight(2026, 10, 30), kLat, kLon));
}

// Sat 31 Oct 2026: sunset ~15:39 UTC, so nightfall at ~16:21 UTC.
void test_saturday_until_nightfall(void) {
  TEST_ASSERT_TRUE(shabbat::isShabbat(utc(2026, 10, 31, 9, 0), kSaturday,
                                      cetMidnight(2026, 10, 31), kLat, kLon));
  TEST_ASSERT_TRUE(shabbat::isShabbat(utc(2026, 10, 31, 16, 10), kSaturday,
                                      cetMidnight(2026, 10, 31), kLat, kLon));
  TEST_ASSERT_FALSE(shabbat::isShabbat(utc(2026, 10, 31, 16, 35), kSaturday,
                                       cetMidnight(2026, 10, 31), kLat, kLon));
}

void test_other_days_never(void) {
  TEST_ASSERT_FALSE(shabbat::isShabbat(utc(2026, 10, 29, 20, 0), kThursday,
                                       cetMidnight(2026, 10, 29), kLat, kLon));
  TEST_ASSERT_FALSE(shabbat::isShabbat(utc(2026, 11, 1, 10, 0), kSunday,
                                       cetMidnight(2026, 11, 1), kLat, kLon));
}

// Midsummer: Fri 19 Jun 2026, sunset ~19:33 UTC (21:33 CEST), so candles
// only from ~19:15 UTC - a summer Friday afternoon isn't Shabbat yet.
void test_midsummer_friday_late_start(void) {
  const time_t midnight = utc(2026, 6, 19) - 7200;  // CEST, UTC+2
  TEST_ASSERT_FALSE(shabbat::isShabbat(utc(2026, 6, 19, 17, 0), kFriday, midnight, kLat, kLon));
  TEST_ASSERT_TRUE(shabbat::isShabbat(utc(2026, 6, 19, 19, 25), kFriday, midnight, kLat, kLon));
}

static int run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_friday_before_candle_lighting);
  RUN_TEST(test_friday_after_candle_lighting);
  RUN_TEST(test_saturday_until_nightfall);
  RUN_TEST(test_other_days_never);
  RUN_TEST(test_midsummer_friday_late_start);
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
