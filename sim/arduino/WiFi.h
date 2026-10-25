#pragma once

// Host stand-in for the ESP32 WiFi object: the computer is always online.

#include <Arduino.h>

enum wl_status_t { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };

class WiFiClass {
public:
  wl_status_t status() const { return WL_CONNECTED; }
};
extern WiFiClass WiFi;
