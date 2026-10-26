#pragma once

#include <ctime>

// Sun and moon rise/set times plus the moon's phase, computed locally from
// the configured coordinates and the current (NTP-synced) time - no API.
// Low-precision almanac formulas (good to a minute or two at mid
// latitudes), split out from SunMoonDisplay so it runs on the host (see
// the `native` env in platformio.ini and test/test_sky_events).
namespace sky {

// Rise/set inside one 24h window. A body can skip either event on a given
// day (the moon does every month or so; the sun does near the poles), in
// which case the matching has* flag is false and the time is 0.
struct RiseSet {
  bool hasRise = false;
  bool hasSet = false;
  time_t rise = 0;
  time_t set = 0;
};

struct MoonPhase {
  // 0 = new, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter.
  double age = 0.0;
  // Illuminated fraction of the disc, 0..1.
  double illumination = 0.0;
};

// Events in [dayStart, dayStart + 24h). dayStart is a Unix timestamp -
// callers pass local midnight to get "today". latDeg/lonDeg are decimal
// degrees, north and east positive.
RiseSet sunRiseSet(time_t dayStart, double latDeg, double lonDeg);
RiseSet moonRiseSet(time_t dayStart, double latDeg, double lonDeg);

MoonPhase moonPhase(time_t t);

// The sun's altitude above the horizon at time t, in degrees (geometric,
// no refraction): negative at night, about -6 at the end of civil dusk.
double sunAltitudeDeg(time_t t, double latDeg, double lonDeg);

}  // namespace sky
