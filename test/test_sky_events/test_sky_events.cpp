// Pure-logic suite for lib/SkyEvents: sun/moon rise and set times and the
// moon's phase for the SunMoon scene. Reference values are the US Naval
// Observatory's (aa.usno.navy.mil/api/rstt) for 52.5118 N, 13.4127 E, in
// UTC, so nothing here depends on the device's timezone.
//
// Runs in BOTH test environments (see platformio.ini):
//   pio test -e native     - on the host, no board required
//   pio test -e esp32dev   - on the device, alongside the firmware suites
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <ctime>

#include <unity.h>

#include "SkyEvents.h"

void setUp(void) {}
void tearDown(void) {}

namespace {
constexpr double kLat = 52.5118;
constexpr double kLon = 13.4127;
constexpr long kToleranceSeconds = 180;  // low-precision formulas: a couple of minutes

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

void assertNear(time_t expected, time_t actual) {
  TEST_ASSERT_INT32_WITHIN(kToleranceSeconds, 0, static_cast<long>(actual - expected));
}
}  // namespace

void test_sun_autumn(void) {
  const sky::RiseSet s = sky::sunRiseSet(utc(2026, 10, 26), kLat, kLon);
  TEST_ASSERT_TRUE(s.hasRise);
  TEST_ASSERT_TRUE(s.hasSet);
  assertNear(utc(2026, 10, 26, 5, 51), s.rise);
  assertNear(utc(2026, 10, 26, 15, 48), s.set);
}

void test_sun_midsummer(void) {
  const sky::RiseSet s = sky::sunRiseSet(utc(2026, 6, 21), kLat, kLon);
  assertNear(utc(2026, 6, 21, 2, 43), s.rise);
  assertNear(utc(2026, 6, 21, 19, 33), s.set);
}

void test_sun_winter(void) {
  const sky::RiseSet s = sky::sunRiseSet(utc(2026, 1, 15), kLat, kLon);
  assertNear(utc(2026, 1, 15, 7, 10), s.rise);
  assertNear(utc(2026, 1, 15, 15, 22), s.set);
}

void test_moon_autumn_sets_before_rising(void) {
  const sky::RiseSet m = sky::moonRiseSet(utc(2026, 10, 26), kLat, kLon);
  TEST_ASSERT_TRUE(m.hasRise);
  TEST_ASSERT_TRUE(m.hasSet);
  assertNear(utc(2026, 10, 26, 6, 23), m.set);
  assertNear(utc(2026, 10, 26, 15, 21), m.rise);
}

void test_moon_midsummer(void) {
  const sky::RiseSet m = sky::moonRiseSet(utc(2026, 6, 21), kLat, kLon);
  assertNear(utc(2026, 6, 21, 10, 43), m.rise);
  assertNear(utc(2026, 6, 21, 22, 51), m.set);
}

void test_moon_winter(void) {
  const sky::RiseSet m = sky::moonRiseSet(utc(2026, 1, 15), kLat, kLon);
  assertNear(utc(2026, 1, 15, 5, 12), m.rise);
  assertNear(utc(2026, 1, 15, 11, 36), m.set);
}

void test_phase_full(void) {
  const sky::MoonPhase p = sky::moonPhase(utc(2026, 10, 26, 12));
  TEST_ASSERT_FLOAT_WITHIN(0.03f, 1.0f, static_cast<float>(p.illumination));
  TEST_ASSERT_FLOAT_WITHIN(0.04f, 0.5f, static_cast<float>(p.age));
}

void test_phase_waxing_crescent(void) {
  // USNO: 46% at 2026-06-21, waxing.
  const sky::MoonPhase p = sky::moonPhase(utc(2026, 6, 21, 12));
  TEST_ASSERT_FLOAT_WITHIN(0.06f, 0.46f, static_cast<float>(p.illumination));
  TEST_ASSERT_TRUE(p.age < 0.5);
}

void test_phase_waning_crescent(void) {
  // USNO: 10% at 2026-01-15, waning.
  const sky::MoonPhase p = sky::moonPhase(utc(2026, 1, 15, 12));
  TEST_ASSERT_FLOAT_WITHIN(0.06f, 0.10f, static_cast<float>(p.illumination));
  TEST_ASSERT_TRUE(p.age > 0.5);
}

void test_sun_altitude(void) {
  // Midsummer noon (USNO upper transit 11:08 UTC): 90 - 52.5 + 23.4.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.9f,
                           static_cast<float>(sky::sunAltitudeDeg(utc(2026, 6, 21, 11, 8), kLat, kLon)));
  // Midwinter midnight: well below the horizon.
  TEST_ASSERT_TRUE(sky::sunAltitudeDeg(utc(2026, 1, 15, 23, 0), kLat, kLon) < -40);
  // At USNO's sunrise the geometric altitude is about -0.8 (refraction + limb).
  TEST_ASSERT_FLOAT_WITHIN(0.3f, -0.83f,
                           static_cast<float>(sky::sunAltitudeDeg(utc(2026, 10, 26, 5, 51), kLat, kLon)));
}

static int run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sun_autumn);
  RUN_TEST(test_sun_midsummer);
  RUN_TEST(test_sun_winter);
  RUN_TEST(test_moon_autumn_sets_before_rising);
  RUN_TEST(test_moon_midsummer);
  RUN_TEST(test_moon_winter);
  RUN_TEST(test_phase_full);
  RUN_TEST(test_phase_waxing_crescent);
  RUN_TEST(test_phase_waning_crescent);
  RUN_TEST(test_sun_altitude);
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
