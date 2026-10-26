#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "BerlinDisplay.h"
#include "ClockDisplay.h"
#include "ImageSlider.h"
#include "LocationService.h"
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
constexpr unsigned long LOOP_DELAY_MS = 30;

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
ClockDisplay *clockDisplay = nullptr;
ImageSlider *imageSlider = nullptr;
WeatherDisplay *weatherDisplay = nullptr;
SunMoonDisplay *sunMoonDisplay = nullptr;
BerlinDisplay *berlinDisplay = nullptr;

// Rotates full-screen between the clock, the photo slider, the weather
// scene, today's sun and moon rise/set times, and the Berlin skyline,
// every SCENE_INTERVAL_MS. Only starts once
// Wi-Fi connects during setup(); if it never connects, the matrix is left
// showing WifiConnector's "No WiFi" message instead. Location and timezone
// come from include/config/location_config.h (see LocationService).
enum class Scene { Clock, Image, Weather, SunMoon, Berlin };
Scene currentScene = Scene::Clock;
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
    weatherDisplay->update();
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
  locationService->begin();

  clockDisplay = new ClockDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  clockDisplay->begin();
  imageSlider = new ImageSlider(matrix, PANEL_RES_X, PANEL_RES_Y);
  // No begin() for weather: its first update() on the network task
  // fetches straight away, without holding up setup().
  weatherDisplay = new WeatherDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  sunMoonDisplay = new SunMoonDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  berlinDisplay = new BerlinDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  xTaskCreatePinnedToCore(networkTask, "network", NETWORK_TASK_STACK, nullptr, 1, nullptr, 0);

  clockDisplay->show();
  lastSceneChangeMs = millis();
}

void loop() {
  if (!wifiConnected) {
    delay(LOOP_DELAY_MS);
    return;
  }

  switch (currentScene) {
    case Scene::Clock:
      clockDisplay->update();
      break;
    case Scene::Image:
      imageSlider->update();
      break;
    case Scene::Weather:
      weatherDisplay->animate();
      break;
    case Scene::SunMoon:
      sunMoonDisplay->update();
      break;
    case Scene::Berlin:
      berlinDisplay->update();
      break;
  }

  if (millis() - lastSceneChangeMs >= SCENE_INTERVAL_MS) {
    lastSceneChangeMs = millis();
    switch (currentScene) {
      case Scene::Clock:
        currentScene = Scene::Image;
        imageSlider->begin();
        break;
      case Scene::Image:
        currentScene = Scene::Weather;
        weatherDisplay->show();
        break;
      case Scene::Weather:
        currentScene = Scene::SunMoon;
        sunMoonDisplay->begin();
        break;
      case Scene::SunMoon:
        currentScene = Scene::Berlin;
        berlinDisplay->begin();
        break;
      case Scene::Berlin:
        currentScene = Scene::Clock;
        clockDisplay->show();
        break;
    }
  }

  delay(LOOP_DELAY_MS);
}

#endif  // UNIT_TEST
