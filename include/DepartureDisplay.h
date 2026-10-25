#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "LocationService.h"

// Shows the next U-Bahn departures from the U-Bahn stations near the
// configured location, using the free BVG transport.rest API (Berlin; no
// API key required). The location comes from LocationService.
//
// Two-step lookup:
//   1. /locations/nearby?latitude=..&longitude=..  -> every U-Bahn station
//      within kMaxStopDistanceM (up to kMaxStops), nearest first
//   2. /stops/<id>/arrivals?duration=..&subway=true..  -> upcoming U-Bahn
//      arrivals, fetched per station and merged
//
// Several stations matter because lines that cross nearby often stop at
// different stations (e.g. U2 at Markisches Museum and U8 at
// Heinrich-Heine-Str., a few metres apart in distance); taking only the
// single nearest one would hide the other line entirely. The merged rows
// are chosen by bvg::pickDepartures so every line gets a row before any
// line gets two. The arrivals request is filtered to subway only (see
// kSubwayOnlyFilter in the .cpp) so frequent tram/bus service doesn't
// crowd the U-Bahn out. Stations are resolved once (and only re-resolved
// if the configured location changes at runtime); arrivals are refetched
// on a short interval so the countdown stays current.
class DepartureDisplay {
public:
  static constexpr int kMaxRows = 3;
  static constexpr int kMaxStops = 3;
  static constexpr int kMaxStopDistanceM = 400;

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
  int stopCount() const { return stopCount_; }
  const String &stopId(int i) const { return stopIds_[i]; }
  const String &stopName(int i) const { return stopNames_[i]; }
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
  String stopIds_[kMaxStops];
  String stopNames_[kMaxStops];
  int stopCount_ = 0;
  String resolvedForLat_;
  String resolvedForLon_;
  Departure departures_[kMaxRows];
  int departureCount_ = 0;
  unsigned long lastFetchMs_ = 0;
};
