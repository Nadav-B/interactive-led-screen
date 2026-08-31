// Pure-logic suite for lib/WeatherCodes: the WMO-code-to-condition
// mapping WeatherDisplay uses to pick an icon.
//
// Runs in BOTH test environments (see platformio.ini):
//   pio test -e native     - on the host, no board required
//   pio test -e esp32dev   - on the device, alongside the firmware suites
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <unity.h>

#include "WeatherCodes.h"

using weather::Condition;
using weather::conditionFromCode;

void setUp(void) {}
void tearDown(void) {}

void test_condition_clear(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Clear), static_cast<int>(conditionFromCode(0)));
}

void test_condition_partly_cloudy(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::PartlyCloudy),
                    static_cast<int>(conditionFromCode(1)));
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::PartlyCloudy),
                    static_cast<int>(conditionFromCode(3)));
}

void test_condition_fog(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Fog), static_cast<int>(conditionFromCode(45)));
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Fog), static_cast<int>(conditionFromCode(48)));
}

void test_condition_rain(void) {
  const int codes[] = {51, 55, 61, 65, 67, 80, 82};
  for (int code : codes) {
    TEST_ASSERT_EQUAL(static_cast<int>(Condition::Rain), static_cast<int>(conditionFromCode(code)));
  }
}

void test_condition_snow(void) {
  const int codes[] = {71, 73, 77, 85, 86};
  for (int code : codes) {
    TEST_ASSERT_EQUAL(static_cast<int>(Condition::Snow), static_cast<int>(conditionFromCode(code)));
  }
}

void test_condition_thunder(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Thunder), static_cast<int>(conditionFromCode(95)));
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Thunder), static_cast<int>(conditionFromCode(99)));
}

void test_condition_unknown(void) {
  const int codes[] = {4, 49, 68, 78, 87, 100, -1};
  for (int code : codes) {
    TEST_ASSERT_EQUAL(static_cast<int>(Condition::Unknown),
                      static_cast<int>(conditionFromCode(code)));
  }
}

static int run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_condition_clear);
  RUN_TEST(test_condition_partly_cloudy);
  RUN_TEST(test_condition_fog);
  RUN_TEST(test_condition_rain);
  RUN_TEST(test_condition_snow);
  RUN_TEST(test_condition_thunder);
  RUN_TEST(test_condition_unknown);
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
