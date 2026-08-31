#pragma once

#include <cstddef>
#include <string>

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

}  // namespace bvg
