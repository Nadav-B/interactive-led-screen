#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Connects to Wi-Fi once at boot, animating a connecting indicator on the
// matrix while it associates and leaving a "No WiFi" message instead of
// failing silently if it times out. Other classes (ClockDisplay,
// WeatherDisplay, ...) assume Wi-Fi is already up by the time their own
// begin()/update() runs.
class WifiConnector {
public:
  explicit WifiConnector(MatrixPanel_I2S_DMA *matrix);

  // Blocks up to a few seconds while Wi-Fi associates. Returns true if
  // connected.
  bool begin();

private:
  void drawConnecting(int activeArcs) const;
  void drawArc(int cx, int cy, int radius, int thickness, uint16_t color) const;
  void drawFailed() const;

  MatrixPanel_I2S_DMA *matrix_;
};
