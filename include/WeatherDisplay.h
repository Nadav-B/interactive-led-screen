#pragma once

#include "LocationService.h"
#include "SceneStyle.h"
#include "WeatherCodes.h"

// Shows current weather for the configured location, fetched from the
// free Open-Meteo API (no API key required), in the same Kurzgesagt-ish
// style as SunMoonDisplay (see SceneStyle.h): an animated condition on the
// left over the starry navy sky - the sun or, after dark, the moon in its
// real phase; flat two-tone capsule clouds drifting; rain streaks, snow
// drifting, fog bands sliding, lightning with a white flash - and the
// temperature and humidity in navy pills on the right. Renders whatever
// it last successfully fetched.
class WeatherDisplay : public style::Scene {
public:
  // The WMO-code mapping lives in lib/WeatherCodes (host-testable).
  using Condition = weather::Condition;

  WeatherDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                 int width = 64, int height = 32);

  // The data side, separate from the scene's begin()/update(): call
  // regularly, regardless of which scene is active. Refetches on its own
  // schedule (kRefreshIntervalMs once data has been fetched at least
  // once, the much shorter kRetryIntervalMs until then, so a transient
  // first-fetch failure clears up in seconds rather than sitting on "no
  // data" for a full refresh cycle). Blocks for the HTTPS request, so it
  // runs on a background task; the scene picks up the new values on its
  // next frame.
  void poll();

  // Performs one real HTTP request against the live Open-Meteo API and
  // updates the fields below. Public (and the fields readable) specifically
  // so test/test_weather_display_live can exercise the real network path
  // without needing a matrix. Doesn't touch the display.
  bool fetchWeather();
  bool hasData() const { return hasData_; }
  float temperatureCelsius() const { return temperatureC_; }
  int humidityPercent() const { return humidityPercent_; }

private:
  static constexpr unsigned long kRefreshIntervalMs = 600000;  // 10 minutes, once we have data
  static constexpr unsigned long kRetryIntervalMs = 30000;     // 30s, until the first success

  // Takes the moon's phase for this visit (shown at night).
  void onEnter() override;
  void render(float tSec) override;
  void drawCondition(float tSec);
  void drawCloud(float cx, float cy, float scale, bool stormy);
  void drawPills(float tSec);

  LocationService *locationService_;
  float moonAge_ = 0.5f;
  float moonIllumination_ = 1;

  // Written by the fetch (possibly on another core), read by render();
  // all word-sized, so no torn reads.
  bool hasData_ = false;
  float temperatureC_ = 0.0f;
  int humidityPercent_ = 0;
  Condition condition_ = Condition::Unknown;
  bool isDay_ = true;
  unsigned long lastFetchMs_ = 0;
};
