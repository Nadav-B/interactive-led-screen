#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "BerlinDisplay.h"
#include "BonarBridgeDisplay.h"
#include "ClockDisplay.h"
#include "ImageSlider.h"
#include "LocationService.h"
#include "ShabbatDisplay.h"
#include "SunMoonDisplay.h"
#include "WeatherDisplay.h"
#include "WifiConnector.h"

// Excluded from `pio test` builds (PlatformIO defines UNIT_TEST there and
// supplies its own setup()/loop() from test/); this keeps the firmware's
// entry point out of the way of each suite's own runner.
#ifndef UNIT_TEST

constexpr int PANEL_RES_X = 64;
constexpr int PANEL_RES_Y = 32;
constexpr int PANEL_CHAIN = 1;
constexpr unsigned long SCENE_INTERVAL_MS = 10000;
constexpr unsigned long IDLE_DELAY_MS = 30;

// Direct ESP32-to-HUB75 connections.
// See the wiring table: ../README.md#hub75-to-esp32-wiring
constexpr int R1_PIN = 25;
constexpr int G1_PIN = 26;
constexpr int B1_PIN = 27;
constexpr int R2_PIN = 14;
constexpr int G2_PIN = 13;
constexpr int B2_PIN = 32;
constexpr int A_PIN = 33;
constexpr int B_PIN = 4;
constexpr int C_PIN = 16;
constexpr int D_PIN = 17;
constexpr int CLK_PIN = 18;
constexpr int LAT_PIN = 23;
constexpr int OE_PIN = 19;

HUB75_I2S_CFG::i2s_pins panelPins = {
  R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
  A_PIN, B_PIN, C_PIN, D_PIN, -1,
  LAT_PIN, OE_PIN, CLK_PIN
};

HUB75_I2S_CFG matrixConfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, panelPins);
MatrixPanel_I2S_DMA *matrix = nullptr;
LocationService *locationService = nullptr;
WeatherDisplay *weatherDisplay = nullptr;

// Rotates full-screen through these, in order, every SCENE_INTERVAL_MS:
// the clock, Shabbat Shalom (only on Shabbat - scenes whose isActive() is
// false are skipped), the photo slider, the weather, today's sunrise/set
// and daylight, the Berlin skyline and Bonar Bridge (filled in by
// setup()). Only starts once
// Wi-Fi connects during setup(); if it never connects, the matrix is left
// showing WifiConnector's "No WiFi" message instead. Location and timezone
// come from include/config/location_config.h (see LocationService).
style::Scene *scenes[7] = {};
constexpr size_t SCENE_COUNT = sizeof(scenes) / sizeof(scenes[0]);
size_t currentScene = 0;
unsigned long lastSceneChangeMs = 0;
bool wifiConnected = false;

constexpr uint32_t NETWORK_TASK_STACK = 16384;  // TLS handshakes need the headroom
constexpr TickType_t NETWORK_POLL_TICKS = pdMS_TO_TICKS(250);

// Weather refreshes on its own schedule (fast retries until the first
// success, much longer intervals after). Its HTTPS request blocks for
// seconds at a time - up to 10s when the API is unreachable - so it runs
// here, on core 0, and never stalls the animations rendering in loop() on
// core 1, which pick up the new values on their next frame.
void networkTask(void *) {
  for (;;) {
    weatherDisplay->poll();
    vTaskDelay(NETWORK_POLL_TICKS);
  }
}

void setup() {
  Serial.begin(115200);

  matrixConfig.clkphase = false;
  matrix = new MatrixPanel_I2S_DMA(matrixConfig);
  matrix->begin();
  matrix->setBrightness8(64);

  WifiConnector wifiConnector(matrix);
  wifiConnected = wifiConnector.begin();
  if (!wifiConnected) {
    return;  // WifiConnector already left "No WiFi" on the matrix.
  }

  locationService = new LocationService();
  locationService->begin();  // timezone + NTP, for every scene showing local time

  weatherDisplay = new WeatherDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  scenes[0] = new ClockDisplay(matrix, PANEL_RES_X, PANEL_RES_Y);
  scenes[1] = new ShabbatDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  scenes[2] = new ImageSlider(matrix, PANEL_RES_X, PANEL_RES_Y);
  scenes[3] = weatherDisplay;
  scenes[4] = new SunMoonDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  scenes[5] = new BerlinDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  scenes[6] = new BonarBridgeDisplay(matrix, PANEL_RES_X, PANEL_RES_Y);
  // The weather's first poll() fetches straight away, without holding up
  // setup().
  xTaskCreatePinnedToCore(networkTask, "network", NETWORK_TASK_STACK, nullptr, 1, nullptr, 0);

  scenes[currentScene]->begin();
  lastSceneChangeMs = millis();
}

void loop() {
  if (!wifiConnected) {
    delay(IDLE_DELAY_MS);
    return;
  }

  if (millis() - lastSceneChangeMs >= SCENE_INTERVAL_MS) {
    lastSceneChangeMs = millis();
    // Next scene that wants to show now; the clock always does, so this
    // always finds one.
    do {
      currentScene = (currentScene + 1) % SCENE_COUNT;
    } while (!scenes[currentScene]->isActive());
    scenes[currentScene]->begin();
  }
  scenes[currentScene]->update();

  // Just a yield: each scene paces its own frames, so sleeping longer
  // here would only make them miss their frame times.
  delay(1);
}

#endif  // UNIT_TEST
