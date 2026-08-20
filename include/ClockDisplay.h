#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>

// Connects to Wi-Fi, syncs time via NTP, and renders a 24h HH:MM:SS clock
// full-screen on the matrix, redrawing only when the displayed second changes.
class ClockDisplay {
public:
  explicit ClockDisplay(MatrixPanel_I2S_DMA *matrix);

  // Connects to Wi-Fi and starts NTP sync. Blocks up to a few seconds
  // while Wi-Fi associates; time itself syncs in the background.
  void begin();

  // Call every loop() iteration; redraws only when the displayed second changes.
  void update();

  // Forces an immediate redraw with the current time, ignoring the last-drawn
  // second. Use this when switching back to the clock from another scene.
  void show();

private:
  void render(const struct tm &timeinfo);

  MatrixPanel_I2S_DMA *matrix_;
  int lastSecond_ = -1;
};
