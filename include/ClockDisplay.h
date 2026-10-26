#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>

#include "LocationService.h"

// Syncs time via NTP, using LocationService's configured timezone, and
// shows it in the same Kurzgesagt-ish style as SunMoonDisplay (see
// SceneStyle.h): a 24h HH:MM in rounded seven-segment digits over the
// starry navy sky, with a gently pulsing gold colon, a thin bar beneath
// that fills over each minute (the seconds), and the date - "Mon 26 Oct" -
// in a navy pill along the bottom. A pixel-art Pikachu stands beside the
// time: he hops in when the scene appears, bobs, blinks, and his cheeks
// spark now and then. The time rises in through a clip.
// Assumes Wi-Fi is already connected (see WifiConnector) by the time
// begin() is called.
class ClockDisplay {
public:
  ClockDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService, int width = 64,
               int height = 32);
  ~ClockDisplay();

  // Starts NTP sync; time itself syncs in the background.
  void begin();

  // Reads LocationService's timezone and applies it via configTzTime().
  // Called once by begin(); the TZ is a static DST-aware POSIX rule
  // string (see include/config/location_config.h), so the C library
  // handles DST transitions with no re-apply needed.
  void applyTimezone();

  // Advances and redraws, throttled to a fixed frame rate internally.
  // Safe to call every loop() iteration.
  void update();

  // Restarts the intro animation. Use this when switching back to the
  // clock from another scene.
  void show();

private:
  static constexpr unsigned long kFrameIntervalMs = 33;  // ~30 fps

  void render(float tSec);

  MatrixPanel_I2S_DMA *matrix_;
  LocationService *locationService_;
  int width_;
  int height_;
  uint8_t *frame_;  // width_ * height_ * RGB888
  unsigned long startMs_ = 0;
  unsigned long lastFrameMs_ = 0;
  // millis() when the wall-clock second last ticked over, for the smooth
  // sub-second motion of the colon and the seconds bar.
  int lastSecond_ = -1;
  unsigned long secondStartMs_ = 0;
};
