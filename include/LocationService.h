#pragma once

#include <Arduino.h>

// Single source of truth for "where the device is", read straight from
// compile-time config (include/config/location_config.h) - no network, no
// geolocation service.
//
//   - Coordinates (LOCATION_LATITUDE / LOCATION_LONGITUDE) are used by
//     WeatherDisplay and by DepartureDisplay's nearest-stop lookup.
//   - The timezone (LOCATION_POSIX_TZ) is a POSIX TZ rule string - DST
//     transitions and all - handed to configTzTime() by ClockDisplay.
//     No refresh needed: the C library applies the DST rule itself.
//
// Defaults target Berlin, matching this project's BVG departures scene.
class LocationService {
public:
  LocationService();

  // Logs the resolved location once. Kept so main.cpp's call site is
  // unchanged; does no I/O.
  void begin();

  // Decimal-degree strings, ready to drop into an API query.
  String latitude() const;
  String longitude() const;

  // Label for the weather scene heading.
  String cityName() const;

  // POSIX TZ string for configTzTime(). Static for the life of the
  // process; a DST-aware rule string keeps it correct year-round.
  String posixTimezone() const;
};
