#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "ClockDisplay.h"
#include "DepartureDisplay.h"
#include "ImageSlider.h"
#include "LocationService.h"
#include "RatFieldAnimation.h"
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
RatFieldAnimation *ratField = nullptr;
WeatherDisplay *weatherDisplay = nullptr;
DepartureDisplay *departureDisplay = nullptr;

// Rotates full-screen between the clock, the image slider, the rat field
// animation, the weather scene, and the nearby-stop departures board,
// every SCENE_INTERVAL_MS. Only starts
// once Wi-Fi connects during setup(); if it never connects, the matrix is
// left showing WifiConnector's "No WiFi" message instead. Location and
// timezone are auto-detected by LocationService - no manual config.
enum class Scene { Clock, Image, RatField, Weather, Departures };
Scene currentScene = Scene::Clock;
unsigned long lastSceneChangeMs = 0;
bool wifiConnected = false;

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

  clockDisplay = new ClockDisplay(matrix, locationService);
  clockDisplay->begin();
  imageSlider = new ImageSlider(matrix, PANEL_RES_X, PANEL_RES_Y);
  ratField = new RatFieldAnimation(matrix, PANEL_RES_X, PANEL_RES_Y);
  weatherDisplay = new WeatherDisplay(matrix, locationService);
  weatherDisplay->begin();
  departureDisplay = new DepartureDisplay(matrix, locationService);
  departureDisplay->begin();

  lastSceneChangeMs = millis();
}

void loop() {
  if (!wifiConnected) {
    delay(LOOP_DELAY_MS);
    return;
  }

  // Both refresh on their own schedule (fast retries until each first
  // succeeds, much longer intervals after) regardless of which scene is
  // active, so a transient failure clears up on its own instead of sitting
  // on "no data" until the Weather scene happens to come back around.
  if (locationService->update()) {
    clockDisplay->applyTimezone();
  }
  if (weatherDisplay->update() && currentScene == Scene::Weather) {
    weatherDisplay->show();
  }
  if (departureDisplay->update() && currentScene == Scene::Departures) {
    departureDisplay->show();
  }

  switch (currentScene) {
    case Scene::Clock:
      clockDisplay->update();
      break;
    case Scene::Image:
      break;  // Static until the next scene change.
    case Scene::RatField:
      ratField->update();
      break;
    case Scene::Weather:
      break;  // Rendered above only when a fetch actually completes, or on scene entry.
    case Scene::Departures:
      break;  // Rendered above only when a fetch actually completes, or on scene entry.
  }

  if (millis() - lastSceneChangeMs >= SCENE_INTERVAL_MS) {
    lastSceneChangeMs = millis();
    switch (currentScene) {
      case Scene::Clock:
        currentScene = Scene::Image;
        imageSlider->showNext();
        break;
      case Scene::Image:
        currentScene = Scene::RatField;
        ratField->begin();
        break;
      case Scene::RatField:
        currentScene = Scene::Weather;
        weatherDisplay->show();
        break;
      case Scene::Weather:
        currentScene = Scene::Departures;
        departureDisplay->show();
        break;
      case Scene::Departures:
        currentScene = Scene::Clock;
        clockDisplay->show();
        break;
    }
  }

  delay(LOOP_DELAY_MS);
}

#endif  // UNIT_TEST
