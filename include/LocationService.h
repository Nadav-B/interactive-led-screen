#pragma once

#include <Arduino.h>

// Auto-detects location and timezone from the device's public IP via the
// free ip-api.com geolocation API (no key needed) - no manual
// configuration required for WeatherDisplay or ClockDisplay. Falls back
// to config/weather_config.h's defaults if the lookup ever fails.
// Refreshes periodically so the timezone offset self-corrects across DST
// transitions without a reboot.
class LocationService {
public:
  LocationService();

  // Looks up location once if Wi-Fi is already connected. Safe to fail
  // silently (falls back to defaults; retried by update()).
  void begin();

  // Call every loop() iteration, regardless of which scene is active;
  // refreshes on its own schedule. Returns true only on a call that
  // actually re-fetched (so callers can react, e.g. re-apply the
  // timezone), false otherwise.
  bool update();

  // True once a real geolocation lookup has succeeded (vs. still running on
  // the config/weather_config.h fallback).
  bool hasData() const { return hasData_; }

  String latitude() const;
  String longitude() const;
  String cityName() const;

  // Fixed-offset POSIX TZ string reflecting the current UTC offset (the
  // API's "offset" field already accounts for whether DST is active).
  String posixTimezone() const;

private:
  bool fetchLocation();

  bool hasData_ = false;
  float latitudeValue_ = 0.0f;
  float longitudeValue_ = 0.0f;
  String cityName_;
  long utcOffsetSeconds_ = 0;
  unsigned long lastFetchMs_ = 0;
};
