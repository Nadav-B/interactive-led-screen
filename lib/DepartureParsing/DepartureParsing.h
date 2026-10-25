#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Pure helpers for turning the BVG transport.rest API's responses into
// something the panel can show. No Arduino, no network, no matrix - so
// this whole unit builds and runs on the host (see the `native` env in
// platformio.ini and test/test_departure_parsing), which is the fallback
// for exercising the logic when no ESP32 is plugged in.
namespace bvg {

// Strips the "(Berlin)" suffix and a leading station-class tag
// ("S+U ", "S ", "U ") from a stop or headsign name. Passing nullptr
// yields an empty string.
std::string shortenName(const char *name);

// Parses the local wall-clock "HH:MM" out of an ISO-8601 timestamp like
// "2026-08-31T19:38:00+02:00". Writes "HH:MM" into outHHMM (when
// outSize > 0) and the numbers into hourOut/minOut. Returns false - and
// clears outHHMM - if the string isn't in that shape.
bool parseIsoHourMinute(const char *iso, int &hourOut, int &minOut, char *outHHMM, size_t outSize);

// Whole minutes from the wall-clock nowHour:nowMin to the later
// wall-clock whenHour:whenMin, assuming the two are close: wraps once
// across midnight and never returns a negative value.
int minutesFromNow(int nowHour, int nowMin, int whenHour, int whenMin);

// One upcoming arrival, merged from any of the nearby stops. sortKey
// orders arrivals soonest-first (minutes from now, or minutes since
// midnight when the clock isn't synced yet - either way, smaller = sooner).
struct DepartureCandidate {
  std::string line;
  int sortKey;
};

// Chooses which arrivals fill the board's maxRows rows. Every line gets
// its soonest arrival shown before any line gets a second row, so one
// busy line (e.g. U2 at one stop) can't crowd out another nearby line
// (e.g. U8 at the next stop); leftover rows go to the soonest remaining
// arrivals. Returns indices into `candidates`, soonest first.
std::vector<size_t> pickDepartures(const std::vector<DepartureCandidate> &candidates,
                                   size_t maxRows);

}  // namespace bvg
