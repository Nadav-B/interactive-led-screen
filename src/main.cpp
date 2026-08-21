#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "ClockDisplay.h"
#include "ImageSlider.h"
#include "RatFieldAnimation.h"
#include "WeatherDisplay.h"

// Excluded from `pio test` builds (PlatformIO defines UNIT_TEST there and
// supplies its own setup()/loop() from test/); this keeps the firmware's
// entry point out of the way of test/test_weather_display.cpp.
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
ClockDisplay *clockDisplay = nullptr;
ImageSlider *imageSlider = nullptr;
RatFieldAnimation *ratField = nullptr;
WeatherDisplay *weatherDisplay = nullptr;

// Rotates full-screen between the clock, the image slider, the rat field
// animation, and the weather scene, every SCENE_INTERVAL_MS.
enum class Scene { Clock, Image, RatField, Weather };
Scene currentScene = Scene::Clock;
unsigned long lastSceneChangeMs = 0;

void setup() {
  Serial.begin(115200);

  matrixConfig.clkphase = false;
  matrix = new MatrixPanel_I2S_DMA(matrixConfig);
  matrix->begin();
  matrix->setBrightness8(64);

  clockDisplay = new ClockDisplay(matrix);
  clockDisplay->begin();
  imageSlider = new ImageSlider(matrix, PANEL_RES_X, PANEL_RES_Y);
  ratField = new RatFieldAnimation(matrix, PANEL_RES_X, PANEL_RES_Y);
  weatherDisplay = new WeatherDisplay(matrix);
  weatherDisplay->begin();

  lastSceneChangeMs = millis();
}

void loop() {
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
      // Fetches over the network on its own schedule; the first fetch after
      // entering this scene may briefly block for the HTTP round-trip.
      weatherDisplay->update();
      break;
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
        currentScene = Scene::Clock;
        clockDisplay->show();
        break;
    }
  }

  delay(LOOP_DELAY_MS);
}

#endif  // UNIT_TEST
