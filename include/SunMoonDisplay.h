#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>

#include "LocationService.h"
#include "SkyEvents.h"

// Today's sunrise/sunset and moonrise/moonset for the configured location,
// in a Kurzgesagt-ish flat-vector style: navy gradient sky, three
// parallax star layers that twinkle, a glowing sun cut off by the left
// edge (halo rings, rotating ray wedges, a ring pulsing outward) and a
// moon whose phase is shaded with a smoothstep terminator and a cyan rim.
// Times sit in dark navy pills and rise into view through a clip.
//
// The scene is a 2-page virtual canvas: the sun page first, then an
// eased camera pan to the moon page. Times are computed on-device by
// lib/SkyEvents from the config coordinates and the NTP clock - no API.
// The sky, stars and motion curves are shared with ImageSlider (see
// SceneStyle.h).
class SunMoonDisplay {
public:
  SunMoonDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService, int width,
                 int height);
  ~SunMoonDisplay();

  // Restarts the intro animation and recomputes today's events if the
  // local date changed (or the clock hadn't synced last time). Call once
  // before the scene becomes active.
  void begin();

  // Advances and redraws, throttled to a fixed frame rate internally.
  // Safe to call every loop() iteration.
  void update();

private:
  static constexpr unsigned long kFrameIntervalMs = 33;  // ~30 fps
  static constexpr unsigned long kPanStartMs = 4600;
  static constexpr unsigned long kPanDurationMs = 1300;

  void refreshEvents();
  void render(float tSec);
  void drawSun(float tSec, float camX);
  void drawMoon(float tSec, float camX);
  void drawPill(int canvasX, int y, float camX, float revealSec, float tSec, bool up,
                time_t when, bool has, const uint8_t rgb[3]);

  MatrixPanel_I2S_DMA *matrix_;
  LocationService *locationService_;
  int width_;
  int height_;
  uint8_t *frame_;  // width_ * height_ * RGB888

  bool hasEvents_ = false;
  int eventsYear_ = -1;
  int eventsYday_ = -1;
  sky::RiseSet sun_;
  sky::RiseSet moon_;
  sky::MoonPhase phase_;

  unsigned long startMs_ = 0;
  unsigned long lastFrameMs_ = 0;
};
