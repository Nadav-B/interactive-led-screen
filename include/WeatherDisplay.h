#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Shows current weather for Berlin, fetched from the free Open-Meteo API
// (no API key required). Polls periodically; renders whatever it last
// successfully fetched.
class WeatherDisplay {
public:
  enum class Condition { Clear, PartlyCloudy, Fog, Rain, Snow, Thunder, Unknown };

  explicit WeatherDisplay(MatrixPanel_I2S_DMA *matrix);

  // Fetches once if Wi-Fi is already connected. Safe to fail silently
  // (retried lazily by update()).
  void begin();

  // Redraws with the last-known weather.
  void show();

  // Call every loop() iteration; refetches on its own schedule
  // (kRefreshIntervalMs) and redraws when new data arrives.
  void update();

  // Maps an Open-Meteo/WMO weather code to a display condition. Pure and
  // hardware-independent, so it's covered directly by test/test_weather_display.
  static Condition conditionFromCode(int code);

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
  static constexpr unsigned long kRefreshIntervalMs = 600000;  // 10 minutes

  void render() const;
  void drawIcon(int cx, int cy, Condition condition) const;

  MatrixPanel_I2S_DMA *matrix_;
  bool hasData_ = false;
  float temperatureC_ = 0.0f;
  int humidityPercent_ = 0;
  Condition condition_ = Condition::Unknown;
  unsigned long lastFetchMs_ = 0;
};
