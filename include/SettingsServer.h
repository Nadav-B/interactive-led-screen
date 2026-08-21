#pragma once

#include <WebServer.h>

#include "WeatherDisplay.h"

// Hosts a small always-on settings page for changing WeatherDisplay's
// location at runtime, without reflashing. Reachable at the device's IP
// (port 80) any time it's connected to Wi-Fi - not just during the
// WifiConnector setup portal. Values are persisted via WeatherSettings
// (NVS) and take effect immediately (triggers an on-the-spot refetch).
class SettingsServer {
public:
  explicit SettingsServer(WeatherDisplay *weatherDisplay);

  // Starts the HTTP server. Call once, after Wi-Fi is connected.
  void begin();

  // Call every loop() iteration to process pending HTTP requests.
  void handleClient();

private:
  void handleRoot();
  void handleSave();
  String renderPage(const String &message) const;

  WebServer server_;
  WeatherDisplay *weatherDisplay_;
};
