#pragma once

#include <Arduino.h>

// Single source of truth for "where the device is", read straight from
// compile-time config (include/config/location_config.h) - no network, no
// geolocation service.
//
//   - Coordinates (LOCATION_LATITUDE / LOCATION_LONGITUDE) are used by
//     WeatherDisplay's API query and by the sun/moon calculations of
//     SunMoonDisplay, BerlinDisplay and WeatherDisplay.
//   - The timezone (LOCATION_POSIX_TZ) is a POSIX TZ rule string - DST
//     transitions and all - that begin() hands to configTzTime() along with
//     the NTP servers, for every scene that shows local time. No refresh
//     needed: the C library applies the DST rule itself.
//
// Defaults target Berlin.
class LocationService {
public:
  LocationService();

  // Logs the configured location, applies its timezone and starts NTP
  // sync (which then runs in the background). Call once Wi-Fi is up.
  void begin();

  // Decimal-degree strings, ready to drop into an API query.
  String latitude() const;
  String longitude() const;

  // The same, as numbers, for the astronomy.
  double latitudeDeg() const;
  double longitudeDeg() const;

  // Human-readable place name, for logs.
  String cityName() const;
};
