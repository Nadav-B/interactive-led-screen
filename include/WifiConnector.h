#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Connects to Wi-Fi once at boot via WiFiManager: tries previously saved
// credentials first (drawing a "Connecting" indicator), and if that fails,
// opens a captive-portal setup access point instead of failing silently -
// join it from a phone or computer to submit new credentials without
// needing any prior network access. Leaves a "No WiFi" message on the
// matrix if the whole process times out. Other classes (ClockDisplay,
// WeatherDisplay, ...) assume Wi-Fi is already up by the time their own
// begin()/update() runs.
class WifiConnector {
public:
  explicit WifiConnector(MatrixPanel_I2S_DMA *matrix);

  // Blocks while Wi-Fi connects, or while the setup portal is open waiting
  // to be configured. Returns true if connected.
  bool begin();

  // Draws the "join this AP to configure WiFi" screen. Public only so the
  // free-function callback WiFiManager invokes can reach it - not part of
  // the intended external API.
  void drawSetupPortal() const;

private:
  void drawConnecting() const;
  void drawArc(int cx, int cy, int radius, int thickness, uint16_t color) const;
  void drawFailed() const;

  MatrixPanel_I2S_DMA *matrix_;
};
