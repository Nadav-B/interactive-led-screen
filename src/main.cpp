#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "BerlinDisplay.h"
#include "BonarBridgeDisplay.h"
#include "ClockDisplay.h"
#include "ImageSlider.h"
#include "LocationService.h"
#include "RatEyesDisplay.h"
#include "ShabbatDisplay.h"
#include "SleepDisplay.h"
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

// By day, rotates full-screen through these, in order, every SCENE_INTERVAL_MS:
// the clock, Shabbat Shalom (only on Shabbat - scenes whose isActive() is
// false are skipped), the photo slider, the wet rat whose eyes pop, the
// weather, today's sunrise/set and daylight, the Berlin skyline and Bonar
// Bridge (filled in by setup()). Only starts once Wi-Fi connects during setup() - until then
// WifiConnector keeps its setup portal open. Location and timezone come
// from include/config/location_config.h (see LocationService).
style::Scene *dayScenes[8] = {};

// Night mode, NIGHT_START_HOUR to NIGHT_END_HOUR local time: the panel
// dims, the rotation shrinks to the sleeping rat (plus Shabbat Shalom on
// Shabbat nights), which renders only a few frames a second, and the
// weather stops fetching until morning.
constexpr int NIGHT_START_HOUR = 23;
constexpr int NIGHT_END_HOUR = 7;
constexpr uint8_t DAY_BRIGHTNESS = 64;
constexpr uint8_t NIGHT_BRIGHTNESS = 16;
constexpr unsigned long MODE_CHECK_MS = 1000;
style::Scene *nightScenes[2] = {};

style::Scene **scenes = dayScenes;
size_t sceneCount = sizeof(dayScenes) / sizeof(dayScenes[0]);
size_t currentScene = 0;
unsigned long lastSceneChangeMs = 0;
unsigned long lastModeCheckMs = 0;
volatile bool nightMode = false;  // read by the network task on the other core
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
    if (!nightMode) {  // paused overnight; the first poll after 07:00 fetches
      weatherDisplay->poll();
    }
    vTaskDelay(NETWORK_POLL_TICKS);
  }
}

bool isNightNow() {
  struct tm local;
  if (!getLocalTime(&local, 0)) {
    return false;  // Clock not synced yet: stay in day mode.
  }
  return local.tm_hour >= NIGHT_START_HOUR || local.tm_hour < NIGHT_END_HOUR;
}

// Switches between the day and night rotations, starting the new one from
// its first scene at the matching brightness.
void enterMode(bool night) {
  nightMode = night;
  scenes = night ? nightScenes : dayScenes;
  sceneCount = night ? sizeof(nightScenes) / sizeof(nightScenes[0])
                     : sizeof(dayScenes) / sizeof(dayScenes[0]);
  currentScene = 0;
  matrix->setBrightness8(night ? NIGHT_BRIGHTNESS : DAY_BRIGHTNESS);
  scenes[currentScene]->begin();
  lastSceneChangeMs = millis();
  Serial.printf("[main] %s mode\n", night ? "night" : "day");
}

void setup() {
  Serial.begin(115200);

  matrixConfig.clkphase = false;
  matrix = new MatrixPanel_I2S_DMA(matrixConfig);
  matrix->begin();
  matrix->setBrightness8(DAY_BRIGHTNESS);

  WifiConnector wifiConnector(matrix);
  wifiConnected = wifiConnector.begin();
  if (!wifiConnected) {
    return;  // Not expected: begin() waits on its setup portal until connected.
  }

  locationService = new LocationService();
  locationService->begin();  // timezone + NTP, for every scene showing local time

  weatherDisplay = new WeatherDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  ShabbatDisplay *shabbat = new ShabbatDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  dayScenes[0] = new ClockDisplay(matrix, PANEL_RES_X, PANEL_RES_Y);
  dayScenes[1] = shabbat;
  dayScenes[2] = new ImageSlider(matrix, PANEL_RES_X, PANEL_RES_Y);
  dayScenes[3] = new RatEyesDisplay(matrix, PANEL_RES_X, PANEL_RES_Y);
  dayScenes[4] = weatherDisplay;
  dayScenes[5] = new SunMoonDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  dayScenes[6] = new BerlinDisplay(matrix, locationService, PANEL_RES_X, PANEL_RES_Y);
  dayScenes[7] = new BonarBridgeDisplay(matrix, PANEL_RES_X, PANEL_RES_Y);
  nightScenes[0] = new SleepDisplay(matrix, PANEL_RES_X, PANEL_RES_Y);
  nightScenes[1] = shabbat;
  // The weather's first poll() fetches straight away, without holding up
  // setup().
  xTaskCreatePinnedToCore(networkTask, "network", NETWORK_TASK_STACK, nullptr, 1, nullptr, 0);

  enterMode(isNightNow());
}

void loop() {
  if (!wifiConnected) {
    delay(IDLE_DELAY_MS);
    return;
  }

  if (millis() - lastModeCheckMs >= MODE_CHECK_MS) {
    lastModeCheckMs = millis();
    if (isNightNow() != nightMode) {
      enterMode(!nightMode);
    }
  }

  if (millis() - lastSceneChangeMs >= SCENE_INTERVAL_MS) {
    lastSceneChangeMs = millis();
    // Next scene that wants to show now; each rotation's first scene (the
    // clock, the sleeping rat) always does, so this always finds one.
    const size_t previous = currentScene;
    do {
      currentScene = (currentScene + 1) % sceneCount;
    } while (!scenes[currentScene]->isActive());
    // Coming back round to the same scene (a night with no Shabbat) just
    // carries on, rather than restarting its animation.
    if (currentScene != previous) {
      scenes[currentScene]->begin();
    }
  }
  scenes[currentScene]->update();

  // Just a yield: each scene paces its own frames, so sleeping longer
  // here would only make them miss their frame times.
  delay(1);
}

#endif  // UNIT_TEST
