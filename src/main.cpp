#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "ClockDisplay.h"
#include "ImageSlider.h"

constexpr int PANEL_RES_X = 64;
constexpr int PANEL_RES_Y = 32;
constexpr int PANEL_CHAIN = 1;
constexpr unsigned long SCENE_INTERVAL_MS = 10000;

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

// Alternates full-screen between the clock and the image slider every
// SCENE_INTERVAL_MS.
enum class Scene { Clock, Image };
Scene currentScene = Scene::Clock;
unsigned long lastSceneChangeMs = 0;

void setup() {
  matrixConfig.clkphase = false;
  matrix = new MatrixPanel_I2S_DMA(matrixConfig);
  matrix->begin();
  matrix->setBrightness8(64);

  clockDisplay = new ClockDisplay(matrix);
  clockDisplay->begin();
  imageSlider = new ImageSlider(matrix, PANEL_RES_X, PANEL_RES_Y);

  lastSceneChangeMs = millis();
}

void loop() {
  if (currentScene == Scene::Clock) {
    clockDisplay->update();
  }

  if (millis() - lastSceneChangeMs >= SCENE_INTERVAL_MS) {
    lastSceneChangeMs = millis();
    if (currentScene == Scene::Clock) {
      currentScene = Scene::Image;
      imageSlider->showNext();
    } else {
      currentScene = Scene::Clock;
      clockDisplay->show();
    }
  }

  delay(200);
}
