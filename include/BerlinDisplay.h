#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "LocationService.h"

// A little animated Berlin skyline in the same Kurzgesagt-ish style as the
// other scenes (see SceneStyle.h): the Fernsehturm at Alexanderplatz -
// silver sphere with its window band, red-and-white antenna and blinking
// aviation light - beside the Berliner Dom's green dome and the Rotes
// Rathaus tower, over a city of rooftops, with a red-and-ochre S-Bahn
// rolling across a brick viaduct along the bottom.
//
// The sky follows the real time of day at the configured location (from
// lib/SkyEvents' sun altitude): blue by day, coral at golden hour, the
// starry navy sky at night with the moon in its real phase - and the
// windows of the city, the tower's band and the train light up after
// dark. The camera drifts slowly so the layers move in parallax, and the
// tower rises into place when the scene appears.
class BerlinDisplay {
public:
  BerlinDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService, int width,
                int height);
  ~BerlinDisplay();

  // Restarts the intro animation. Call once before the scene becomes
  // active.
  void begin();

  // Advances and redraws, throttled to a fixed frame rate internally.
  // Safe to call every loop() iteration.
  void update();

private:
  static constexpr unsigned long kFrameIntervalMs = 33;  // ~30 fps

  void render(float tSec);
  void drawSky(float tSec);
  void drawRooftops(float camX);
  void drawLandmarks(float camX);
  void drawTower(float tSec, float camX);
  void drawTrain(float tSec);
  void drawViaduct();
  void put(int x, int y, const float rgb[3], float cover = 1);
  void putRgb(int x, int y, const uint8_t rgb[3]);
  void add(int x, int y, const float rgb[3], float k);

  MatrixPanel_I2S_DMA *matrix_;
  LocationService *locationService_;
  int width_;
  int height_;
  uint8_t *frame_;  // width_ * height_ * RGB888
  unsigned long startMs_ = 0;
  unsigned long lastFrameMs_ = 0;

  // 0 = full day, 1 = full night, from the sun's real altitude; refreshed
  // each begin().
  float night_ = 1;
  float golden_ = 0;
  float moonAge_ = 0.5f;
  float moonIllumination_ = 1;
};
