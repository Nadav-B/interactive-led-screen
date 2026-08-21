// Integration test: unlike test_weather_display (pure logic, no network),
// this suite connects to Wi-Fi and makes one real HTTPS request to the
// live Open-Meteo API. It reuses whatever network the device last
// connected to via WiFiManager (see WifiConnector) - run the main firmware
// once and complete the "LED-Setup" portal first if this board has never
// been connected. Fails if the network/API path is broken - that's the
// point: run this to actually verify WeatherDisplay's fetch works, not
// just that its code compiles.
#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>

#include "WeatherDisplay.h"

namespace {
WeatherDisplay *weather = nullptr;
}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_wifi_connected(void) {
  TEST_ASSERT_EQUAL_MESSAGE(
      WL_CONNECTED, WiFi.status(),
      "Wi-Fi didn't connect - run the main firmware and complete the LED-Setup portal first");
}

void test_fetch_real_weather(void) {
  const bool ok = weather->fetchWeather();
  TEST_ASSERT_TRUE_MESSAGE(
      ok, "fetchWeather() failed - see the [WeatherDisplay] lines logged above for why");
  TEST_ASSERT_TRUE(weather->hasData());

  // Sanity ranges wide enough to only catch garbage/parsing bugs, not to
  // second-guess the actual weather.
  TEST_ASSERT_FLOAT_WITHIN(60.0f, 10.0f, weather->temperatureCelsius());
  TEST_ASSERT_TRUE(weather->humidityPercent() >= 0 && weather->humidityPercent() <= 100);
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // let the board settle before the test runner talks over serial

  weather = new WeatherDisplay(nullptr);  // fetchWeather() never touches the matrix

  WiFi.mode(WIFI_STA);
  WiFi.begin();  // reconnects using the credentials WiFiManager already saved to flash
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }

  UNITY_BEGIN();
  RUN_TEST(test_wifi_connected);
  RUN_TEST(test_fetch_real_weather);
  UNITY_END();
}

void loop() {}
