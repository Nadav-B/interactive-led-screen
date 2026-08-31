#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "LocationService.h"

// Shows the next public-transport departures for the stop nearest the
// device, using the free BVG transport.rest API (Berlin; no API key
// required). The location comes from LocationService, which auto-detects
// latitude/longitude from the device's public IP.
//
// Two-step lookup:
//   1. /locations/nearby?latitude=..&longitude=..  -> nearest stop id + name
//   2. /stops/<id>/arrivals?duration=..            -> upcoming arrivals
//
// The stop id is resolved once (and only re-resolved if the detected
// location moves); arrivals are refetched on a short interval so the
// countdown stays current.
class DepartureDisplay {
public:
  static constexpr int kMaxRows = 3;

  struct Departure {
    String line;       // e.g. "U8", "M10", "265"
    String direction;  // headsign / provenance, e.g. "S+U Hermannstr."
    int minutes;       // whole minutes from now until arrival (>= 0)
    char whenText[6];  // "HH:MM" fallback when local time isn't synced yet
  };

  DepartureDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService);

  // Resolves the nearest stop and fetches arrivals once if Wi-Fi is
  // already connected. Safe to fail silently (retried lazily by update()).
  void begin();

  // Redraws with the last-known departures.
  void show();

  // Call every loop() iteration, regardless of which scene is active.
  // Refetches arrivals on its own schedule (kRefreshIntervalMs once data
  // has been fetched at least once, the shorter kRetryIntervalMs until
  // then). Returns true only on a call that actually re-fetched, so the
  // caller can call show() if this scene is the active one.
  bool update();

  // Performs the real HTTP requests against the live API and updates the
  // fields below. Public (and the fields readable) so a live network test
  // can exercise this path without a matrix. Doesn't touch the display.
  bool resolveStop();
  bool fetchArrivals();
  bool hasData() const { return hasData_; }
  const String &stopId() const { return stopId_; }
  const String &stopName() const { return stopName_; }
  int departureCount() const { return departureCount_; }
  const Departure &departure(int i) const { return departures_[i]; }

private:
  static constexpr unsigned long kRefreshIntervalMs = 60000;  // 1 minute, once we have data
  static constexpr unsigned long kRetryIntervalMs = 20000;    // 20s, until the first success
  static constexpr int kArrivalsDurationMin = 20;

  void render() const;
  int minutesUntil(const char *isoWhen, char *outHHMM, size_t outSize) const;

  MatrixPanel_I2S_DMA *matrix_;
  LocationService *locationService_;

  bool hasData_ = false;
  String stopId_;
  String stopName_;
  String resolvedForLat_;
  String resolvedForLon_;
  Departure departures_[kMaxRows];
  int departureCount_ = 0;
  unsigned long lastFetchMs_ = 0;
};
