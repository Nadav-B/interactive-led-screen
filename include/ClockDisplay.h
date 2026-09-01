#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>

#include "LocationService.h"

// Syncs time via NTP, using LocationService's configured timezone, and
// renders a 24h HH:MM:SS clock full-screen on the matrix, redrawing only
// when the displayed second changes. Assumes Wi-Fi is already connected
// (see WifiConnector) by the time begin() is called.
class ClockDisplay {
public:
  ClockDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService);

  // Starts NTP sync; time itself syncs in the background.
  void begin();

  // Reads LocationService's timezone and applies it via configTzTime().
  // Called once by begin(); the TZ is a static DST-aware POSIX rule
  // string (see include/config/location_config.h), so the C library
  // handles DST transitions with no re-apply needed.
  void applyTimezone();

  // Call every loop() iteration; redraws only when the displayed second changes.
  void update();

  // Forces an immediate redraw with the current time, ignoring the last-drawn
  // second. Use this when switching back to the clock from another scene.
  void show();

private:
  void render(const struct tm &timeinfo);

  MatrixPanel_I2S_DMA *matrix_;
  LocationService *locationService_;
  int lastSecond_ = -1;
};
