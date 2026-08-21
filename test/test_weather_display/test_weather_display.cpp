#include <Arduino.h>
#include <unity.h>

#include "WeatherDisplay.h"

using Condition = WeatherDisplay::Condition;

void setUp(void) {}
void tearDown(void) {}

void test_condition_clear(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Clear),
                     static_cast<int>(WeatherDisplay::conditionFromCode(0)));
}

void test_condition_partly_cloudy(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::PartlyCloudy),
                     static_cast<int>(WeatherDisplay::conditionFromCode(1)));
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::PartlyCloudy),
                     static_cast<int>(WeatherDisplay::conditionFromCode(3)));
}

void test_condition_fog(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Fog),
                     static_cast<int>(WeatherDisplay::conditionFromCode(45)));
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Fog),
                     static_cast<int>(WeatherDisplay::conditionFromCode(48)));
}

void test_condition_rain(void) {
  const int codes[] = {51, 55, 61, 65, 67, 80, 82};
  for (int code : codes) {
    TEST_ASSERT_EQUAL(static_cast<int>(Condition::Rain),
                       static_cast<int>(WeatherDisplay::conditionFromCode(code)));
  }
}

void test_condition_snow(void) {
  const int codes[] = {71, 73, 77, 85, 86};
  for (int code : codes) {
    TEST_ASSERT_EQUAL(static_cast<int>(Condition::Snow),
                       static_cast<int>(WeatherDisplay::conditionFromCode(code)));
  }
}

void test_condition_thunder(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Thunder),
                     static_cast<int>(WeatherDisplay::conditionFromCode(95)));
  TEST_ASSERT_EQUAL(static_cast<int>(Condition::Thunder),
                     static_cast<int>(WeatherDisplay::conditionFromCode(99)));
}

void test_condition_unknown(void) {
  const int codes[] = {4, 49, 68, 78, 87, 100, -1};
  for (int code : codes) {
    TEST_ASSERT_EQUAL(static_cast<int>(Condition::Unknown),
                       static_cast<int>(WeatherDisplay::conditionFromCode(code)));
  }
}

void setup() {
  delay(2000);  // let the board settle before the test runner talks over serial
  UNITY_BEGIN();
  RUN_TEST(test_condition_clear);
  RUN_TEST(test_condition_partly_cloudy);
  RUN_TEST(test_condition_fog);
  RUN_TEST(test_condition_rain);
  RUN_TEST(test_condition_snow);
  RUN_TEST(test_condition_thunder);
  RUN_TEST(test_condition_unknown);
  UNITY_END();
}

void loop() {}
