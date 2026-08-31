#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "LocationService.h"
#include "WeatherCodes.h"

// Shows current weather for the location LocationService auto-detects,
// fetched from the free Open-Meteo API (no API key required). Polls
// periodically; renders whatever it last successfully fetched.
class WeatherDisplay {
public:
  // The WMO-code mapping lives in lib/WeatherCodes (host-testable); these
  // aliases keep WeatherDisplay::Condition / ::conditionFromCode working
  // for existing callers.
  using Condition = weather::Condition;
  static Condition conditionFromCode(int code) { return weather::conditionFromCode(code); }

  WeatherDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService);

  // Fetches once if Wi-Fi is already connected. Safe to fail silently
  // (retried lazily by update()).
  void begin();

  // Redraws with the last-known weather.
  void show();

  // Call every loop() iteration, regardless of which scene is active;
  // refetches on its own schedule (kRefreshIntervalMs once data has been
  // fetched at least once, the much shorter kRetryIntervalMs until then,
  // so a transient first-fetch failure clears up in seconds rather than
  // sitting on "no data" for a full refresh cycle). Doesn't render -
  // callers should call show() afterwards if this scene is the active one.
  // Returns true only on a call that actually re-fetched.
  bool update();

  // Performs one real HTTP request against the live Open-Meteo API and
  // updates the fields below. Public (and the fields readable) specifically
  // so test/test_weather_display_live can exercise the real network path
  // without needing a matrix. Doesn't touch the display.
  bool fetchWeather();
  bool hasData() const { return hasData_; }
  float temperatureCelsius() const { return temperatureC_; }
  int humidityPercent() const { return humidityPercent_; }
  Condition condition() const { return condition_; }

private:
  static constexpr unsigned long kRefreshIntervalMs = 600000;  // 10 minutes, once we have data
  static constexpr unsigned long kRetryIntervalMs = 30000;     // 30s, until the first success

  void render() const;
  void drawIcon(int cx, int cy, Condition condition) const;

  MatrixPanel_I2S_DMA *matrix_;
  LocationService *locationService_;
  bool hasData_ = false;
  float temperatureC_ = 0.0f;
  int humidityPercent_ = 0;
  Condition condition_ = Condition::Unknown;
  unsigned long lastFetchMs_ = 0;
};
