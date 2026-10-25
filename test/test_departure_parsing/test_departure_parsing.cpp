// Pure-logic suite for the BVG parsing helpers in lib/DepartureParsing:
// stop-name tidy-up, ISO-8601 "HH:MM" parsing, the departure
// countdown (midnight wrap, past-departure clamp), and choosing which
// arrivals from several nearby stops fill the board.
//
// Runs in BOTH test environments (see platformio.ini):
//   pio test -e native     - on the host, no board required
//   pio test -e esp32dev   - on the device, alongside the firmware suites
// The single source works in both because the runner is split on ARDUINO
// at the bottom: setup()/loop() on-device, main() on the host.
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <cstring>
#include <vector>

#include <unity.h>

#include "DepartureParsing.h"

void setUp(void) {}
void tearDown(void) {}

void test_shorten_name_strips_berlin_suffix(void) {
  TEST_ASSERT_EQUAL_STRING("Jannowitzbrucke", bvg::shortenName("Jannowitzbrucke (Berlin)").c_str());
}

void test_shorten_name_strips_station_class_prefix(void) {
  TEST_ASSERT_EQUAL_STRING("Hermannstr.", bvg::shortenName("S+U Hermannstr. (Berlin)").c_str());
  TEST_ASSERT_EQUAL_STRING("Wittenau", bvg::shortenName("U Wittenau (Berlin)").c_str());
  TEST_ASSERT_EQUAL_STRING("Schoneweide/Sterndamm",
                            bvg::shortenName("S Schoneweide/Sterndamm (Berlin)").c_str());
}

void test_shorten_name_leaves_plain_names_alone(void) {
  TEST_ASSERT_EQUAL_STRING("Alexanderplatz", bvg::shortenName("Alexanderplatz").c_str());
}

void test_shorten_name_handles_null(void) {
  TEST_ASSERT_EQUAL_STRING("", bvg::shortenName(nullptr).c_str());
}

void test_parse_iso_hour_minute_valid(void) {
  int hh = -1, mm = -1;
  char buf[6] = {0};
  TEST_ASSERT_TRUE(bvg::parseIsoHourMinute("2026-08-31T19:38:00+02:00", hh, mm, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_INT(19, hh);
  TEST_ASSERT_EQUAL_INT(38, mm);
  TEST_ASSERT_EQUAL_STRING("19:38", buf);
}

void test_parse_iso_hour_minute_midnight(void) {
  int hh = -1, mm = -1;
  char buf[6] = {0};
  TEST_ASSERT_TRUE(bvg::parseIsoHourMinute("2026-01-01T00:05:00Z", hh, mm, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_INT(0, hh);
  TEST_ASSERT_EQUAL_INT(5, mm);
  TEST_ASSERT_EQUAL_STRING("00:05", buf);
}

void test_parse_iso_hour_minute_rejects_garbage(void) {
  int hh = 0, mm = 0;
  char buf[6];

  strcpy(buf, "x");
  TEST_ASSERT_FALSE(bvg::parseIsoHourMinute("not-a-timestamp", hh, mm, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("", buf);

  strcpy(buf, "x");
  TEST_ASSERT_FALSE(bvg::parseIsoHourMinute("2026-08-31", hh, mm, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("", buf);

  strcpy(buf, "x");
  TEST_ASSERT_FALSE(bvg::parseIsoHourMinute(nullptr, hh, mm, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("", buf);

  // Right length, but the HH:MM slice isn't digits.
  strcpy(buf, "x");
  TEST_ASSERT_FALSE(bvg::parseIsoHourMinute("2026-08-31Txx:yy:00+02:00", hh, mm, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_minutes_from_now_basic(void) {
  TEST_ASSERT_EQUAL_INT(0, bvg::minutesFromNow(13, 0, 13, 0));
  TEST_ASSERT_EQUAL_INT(12, bvg::minutesFromNow(13, 0, 13, 12));
  TEST_ASSERT_EQUAL_INT(90, bvg::minutesFromNow(13, 0, 14, 30));
}

void test_minutes_from_now_clamps_small_negatives_to_zero(void) {
  // A departure a few minutes in the past (delay, or a late poll) shows
  // as "now", not a negative countdown.
  TEST_ASSERT_EQUAL_INT(0, bvg::minutesFromNow(13, 5, 13, 0));
  TEST_ASSERT_EQUAL_INT(0, bvg::minutesFromNow(13, 0, 12, 0));  // exactly -60
}

void test_minutes_from_now_wraps_across_midnight(void) {
  TEST_ASSERT_EQUAL_INT(8, bvg::minutesFromNow(23, 55, 0, 3));
  TEST_ASSERT_EQUAL_INT(4, bvg::minutesFromNow(23, 59, 0, 3));
}

void test_pick_departures_shows_every_line_before_repeats(void) {
  // U2 runs often at the nearest stop; U8 is a little further out in time.
  const std::vector<bvg::DepartureCandidate> c = {
      {"U2", 1}, {"U2", 3}, {"U2", 5}, {"U8", 7}, {"U8", 9},
  };
  const std::vector<size_t> rows = bvg::pickDepartures(c, 3);
  TEST_ASSERT_EQUAL_UINT(3, rows.size());
  TEST_ASSERT_EQUAL_STRING("U2", c[rows[0]].line.c_str());
  TEST_ASSERT_EQUAL_INT(1, c[rows[0]].sortKey);
  TEST_ASSERT_EQUAL_STRING("U2", c[rows[1]].line.c_str());
  TEST_ASSERT_EQUAL_INT(3, c[rows[1]].sortKey);
  TEST_ASSERT_EQUAL_STRING("U8", c[rows[2]].line.c_str());
  TEST_ASSERT_EQUAL_INT(7, c[rows[2]].sortKey);
}

void test_pick_departures_sorts_unordered_input(void) {
  const std::vector<bvg::DepartureCandidate> c = {{"U8", 6}, {"U2", 4}, {"U8", 2}, {"U2", 8}};
  const std::vector<size_t> rows = bvg::pickDepartures(c, 3);
  TEST_ASSERT_EQUAL_UINT(3, rows.size());
  TEST_ASSERT_EQUAL_INT(2, c[rows[0]].sortKey);
  TEST_ASSERT_EQUAL_INT(4, c[rows[1]].sortKey);
  TEST_ASSERT_EQUAL_INT(6, c[rows[2]].sortKey);
}

void test_pick_departures_more_lines_than_rows_keeps_soonest_lines(void) {
  const std::vector<bvg::DepartureCandidate> c = {{"U5", 9}, {"U2", 1}, {"U8", 4}, {"U6", 2}};
  const std::vector<size_t> rows = bvg::pickDepartures(c, 3);
  TEST_ASSERT_EQUAL_UINT(3, rows.size());
  TEST_ASSERT_EQUAL_STRING("U2", c[rows[0]].line.c_str());
  TEST_ASSERT_EQUAL_STRING("U6", c[rows[1]].line.c_str());
  TEST_ASSERT_EQUAL_STRING("U8", c[rows[2]].line.c_str());
}

void test_pick_departures_fewer_than_rows(void) {
  const std::vector<bvg::DepartureCandidate> c = {{"U2", 5}};
  TEST_ASSERT_EQUAL_UINT(1, bvg::pickDepartures(c, 3).size());
  TEST_ASSERT_EQUAL_UINT(0, bvg::pickDepartures({}, 3).size());
}

static int run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_shorten_name_strips_berlin_suffix);
  RUN_TEST(test_shorten_name_strips_station_class_prefix);
  RUN_TEST(test_shorten_name_leaves_plain_names_alone);
  RUN_TEST(test_shorten_name_handles_null);
  RUN_TEST(test_parse_iso_hour_minute_valid);
  RUN_TEST(test_parse_iso_hour_minute_midnight);
  RUN_TEST(test_parse_iso_hour_minute_rejects_garbage);
  RUN_TEST(test_minutes_from_now_basic);
  RUN_TEST(test_minutes_from_now_clamps_small_negatives_to_zero);
  RUN_TEST(test_minutes_from_now_wraps_across_midnight);
  RUN_TEST(test_pick_departures_shows_every_line_before_repeats);
  RUN_TEST(test_pick_departures_sorts_unordered_input);
  RUN_TEST(test_pick_departures_more_lines_than_rows_keeps_soonest_lines);
  RUN_TEST(test_pick_departures_fewer_than_rows);
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
