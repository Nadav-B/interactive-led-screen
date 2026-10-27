#include "Shabbat.h"

#include "SkyEvents.h"

namespace shabbat {

bool isShabbat(time_t now, int localWeekday, time_t localMidnight, double latDeg, double lonDeg) {
  if (localWeekday != 5 && localWeekday != 6) {
    return false;
  }
  const sky::RiseSet sun = sky::sunRiseSet(localMidnight, latDeg, lonDeg);
  if (localWeekday == 5) {
    const time_t start =
        sun.hasSet ? sun.set - kCandleLightingMin * 60 : localMidnight + 18 * 3600;
    return now >= start;
  }
  const time_t end = sun.hasSet ? sun.set + kNightfallMin * 60 : localMidnight + 20 * 3600;
  return now < end;
}

}  // namespace shabbat
