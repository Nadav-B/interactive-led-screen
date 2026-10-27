#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Connects to Wi-Fi once at boot via WiFiManager: tries previously saved
// credentials first (drawing a "Connecting" indicator), and if that fails,
// opens a captive-portal setup access point instead of failing silently -
// join it from a phone or computer to submit new credentials without
// needing any prior network access.
//
// The portal stays open for as long as it takes, with an animated setup
// screen (it drifts slowly, so no LED is stuck lit in one spot for hours).
// Meanwhile it retries the saved network every few minutes while nobody is
// on the portal, so a router that was only down for a while is picked up
// again on its own. Other classes (ClockDisplay, WeatherDisplay, ...)
// assume Wi-Fi is already up by the time their own begin()/update() runs.
class WifiConnector {
public:
  explicit WifiConnector(MatrixPanel_I2S_DMA *matrix);

  // Blocks while Wi-Fi connects, or while the setup portal is open waiting
  // to be configured. Returns true once connected.
  bool begin();

private:
  void drawConnecting() const;
  void drawArc(int cx, int cy, int radius, int thickness, uint16_t color) const;
  void drawSetupPortal(float tSec) const;

  MatrixPanel_I2S_DMA *matrix_;
};
