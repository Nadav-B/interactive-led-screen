#pragma once

#include <ctime>

// When Shabbat is on, for the scene that only shows then (ShabbatDisplay):
// from candle lighting - kCandleLightingMin before Friday's sunset - to
// nightfall, kNightfallMin after Saturday's sunset (a common
// approximation of three stars out). Sunset comes from lib/SkyEvents for
// the configured coordinates. Split out, like SkyEvents, so it runs on
// the host (see test/test_shabbat).
namespace shabbat {

constexpr long kCandleLightingMin = 18;
constexpr long kNightfallMin = 42;

// localWeekday is struct tm's tm_wday (0 = Sunday ... 5 = Friday,
// 6 = Saturday) for `now`, and localMidnight the Unix time that local day
// started - passed in so this needs no timezone. latDeg/lonDeg are decimal
// degrees, north and east positive. Where the sun doesn't set (polar
// summer) it falls back to Friday 18:00 until Saturday 20:00 local.
bool isShabbat(time_t now, int localWeekday, time_t localMidnight, double latDeg, double lonDeg);

}  // namespace shabbat
