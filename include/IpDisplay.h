#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Shows the device's local IP address as plain text, so the settings
// dashboard (see SettingsServer) can be typed into a browser without
// needing to check the serial log.
class IpDisplay {
public:
  explicit IpDisplay(MatrixPanel_I2S_DMA *matrix);

  // Redraws with the device's current IP. Call once each time this scene
  // becomes active.
  void show() const;

private:
  MatrixPanel_I2S_DMA *matrix_;
};
