#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "LocationService.h"
#include "WeatherCodes.h"

// Shows current weather for the configured location, fetched from the
// free Open-Meteo API (no API key required), in the same Kurzgesagt-ish
// style as SunMoonDisplay (see SceneStyle.h): an animated condition on the
// left over the starry navy sky - the sun or, after dark, the moon in its
// real phase; flat two-tone capsule clouds drifting; rain streaks, snow
// drifting, fog bands sliding, lightning with a white flash - and the
// temperature and humidity in navy pills on the right. Polls
// periodically; renders whatever it last successfully fetched.
class WeatherDisplay {
public:
  // The WMO-code mapping lives in lib/WeatherCodes (host-testable); these
  // aliases keep WeatherDisplay::Condition / ::conditionFromCode working
  // for existing callers.
  using Condition = weather::Condition;
  static Condition conditionFromCode(int code) { return weather::conditionFromCode(code); }

  WeatherDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                 int width = 64, int height = 32);
  ~WeatherDisplay();

  // Fetches once if Wi-Fi is already connected. Safe to fail silently
  // (retried lazily by update()).
  void begin();

  // Restarts the intro animation. Call once when the scene becomes active.
  void show();

  // Advances and redraws with the last-known weather, throttled to a fixed
  // frame rate internally. Safe to call every loop() iteration.
  void animate();

  // Call every loop() iteration, regardless of which scene is active;
  // refetches on its own schedule (kRefreshIntervalMs once data has been
  // fetched at least once, the much shorter kRetryIntervalMs until then,
  // so a transient first-fetch failure clears up in seconds rather than
  // sitting on "no data" for a full refresh cycle). Doesn't render, so it
  // can run on a background task; animate() picks up the new values.
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
  bool isDay() const { return isDay_; }

private:
  static constexpr unsigned long kRefreshIntervalMs = 600000;  // 10 minutes, once we have data
  static constexpr unsigned long kRetryIntervalMs = 30000;     // 30s, until the first success

  static constexpr unsigned long kFrameIntervalMs = 33;  // ~30 fps

  void render(float tSec);
  void drawCondition(float tSec);
  void drawCloud(float cx, float cy, float scale, bool stormy);
  void drawPills(float tSec);

  MatrixPanel_I2S_DMA *matrix_;
  LocationService *locationService_;
  int width_;
  int height_;
  uint8_t *frame_;  // width_ * height_ * RGB888
  unsigned long startMs_ = 0;
  unsigned long lastFrameMs_ = 0;

  // Written by the fetch (possibly on another core), read by render();
  // all word-sized, so no torn reads.
  bool hasData_ = false;
  float temperatureC_ = 0.0f;
  int humidityPercent_ = 0;
  Condition condition_ = Condition::Unknown;
  bool isDay_ = true;
  unsigned long lastFetchMs_ = 0;
};
