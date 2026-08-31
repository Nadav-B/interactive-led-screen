// Integration test: unlike test_departure_display (pure logic, no network),
// this suite connects to Wi-Fi and makes real HTTPS requests to the live
// BVG transport.rest API - the two-step nearby-stop lookup plus one
// arrivals fetch. It reuses whatever network the device last connected to
// via WiFiManager (see WifiConnector) - run the main firmware once and
// complete the "LED-Setup" portal first if this board has never been
// connected. Fails if the network/API path is broken - that's the point:
// run this to actually verify DepartureDisplay's fetch works, not just
// that its code compiles.
//
// Note: the BVG API only has data for the Berlin/Brandenburg area. If this
// board's detected location is elsewhere, resolveStop() will legitimately
// find no nearby stop and this suite will fail.
#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>

#include "DepartureDisplay.h"
#include "LocationService.h"

namespace {
LocationService *location = nullptr;
DepartureDisplay *departures = nullptr;
}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_wifi_connected(void) {
  TEST_ASSERT_EQUAL_MESSAGE(
      WL_CONNECTED, WiFi.status(),
      "Wi-Fi didn't connect - run the main firmware and complete the LED-Setup portal first");
}

void test_resolve_nearest_stop(void) {
  const bool ok = departures->resolveStop();
  TEST_ASSERT_TRUE_MESSAGE(
      ok, "resolveStop() failed - see the [DepartureDisplay] lines logged above for why "
          "(is the detected location within the BVG service area?)");
  TEST_ASSERT_FALSE_MESSAGE(departures->stopId().isEmpty(), "resolved an empty stop id");

  // BVG stop ids are all-numeric (e.g. "900100008").
  for (size_t i = 0; i < departures->stopId().length(); i++) {
    TEST_ASSERT_TRUE_MESSAGE(isdigit(departures->stopId()[i]), "stop id wasn't all digits");
  }
  Serial.printf("[test] nearest stop: %s (%s)\n", departures->stopName().c_str(),
                departures->stopId().c_str());
}

void test_fetch_real_arrivals(void) {
  const bool ok = departures->fetchArrivals();
  TEST_ASSERT_TRUE_MESSAGE(
      ok, "fetchArrivals() failed - see the [DepartureDisplay] lines logged above for why");
  TEST_ASSERT_TRUE(departures->hasData());

  const int n = departures->departureCount();
  TEST_ASSERT_TRUE_MESSAGE(n >= 0 && n <= DepartureDisplay::kMaxRows,
                            "departureCount() out of range");
  Serial.printf("[test] %d departures parsed\n", n);

  for (int i = 0; i < n; i++) {
    const DepartureDisplay::Departure &d = departures->departure(i);
    TEST_ASSERT_FALSE_MESSAGE(d.line.isEmpty(), "a departure had an empty line name");
    // whenText is always populated ("HH:MM"); minutes may be -1 if NTP
    // hasn't synced yet in this bare test harness.
    TEST_ASSERT_EQUAL_MESSAGE(5, strlen(d.whenText), "whenText wasn't HH:MM");
    TEST_ASSERT_EQUAL_MESSAGE(':', d.whenText[2], "whenText wasn't HH:MM");
    TEST_ASSERT_TRUE_MESSAGE(d.minutes >= -1, "minutes below the -1 sentinel");
    Serial.printf("[test]   %s  %s  %s (+%d')\n", d.line.c_str(), d.direction.c_str(), d.whenText,
                  d.minutes);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // let the board settle before the test runner talks over serial

  WiFi.mode(WIFI_STA);
  WiFi.begin();  // reconnects using the credentials WiFiManager already saved to flash
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }

  location = new LocationService();
  location->begin();
  departures = new DepartureDisplay(nullptr, location);  // fetch path never touches the matrix

  UNITY_BEGIN();
  RUN_TEST(test_wifi_connected);
  RUN_TEST(test_resolve_nearest_stop);
  RUN_TEST(test_fetch_real_arrivals);
  UNITY_END();
}

void loop() {}
