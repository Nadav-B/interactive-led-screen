#include "ClockText.h"

#include <cstdio>

namespace clockface {

void formatHm(const struct tm &t, char *out, size_t outSize) {
  std::snprintf(out, outSize, "%02d:%02d", t.tm_hour, t.tm_min);
}

void formatDate(const struct tm &t, char *out, size_t outSize) {
  static const char *const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char *day = t.tm_wday >= 0 && t.tm_wday < 7 ? kDays[t.tm_wday] : "???";
  const char *month = t.tm_mon >= 0 && t.tm_mon < 12 ? kMonths[t.tm_mon] : "???";
  std::snprintf(out, outSize, "%s %d %s", day, t.tm_mday, month);
}

void formatDuration(long seconds, char *out, size_t outSize) {
  if (seconds < 0) seconds = 0;
  const long minutes = (seconds + 30) / 60;
  std::snprintf(out, outSize, "%ldh%02ld", minutes / 60, minutes % 60);
}

void formatDelta(long seconds, char *out, size_t outSize) {
  const char sign = seconds > 0 ? '+' : (seconds < 0 ? '-' : '\0');
  const long mag = seconds < 0 ? -seconds : seconds;
  if (mag >= 90) {
    std::snprintf(out, outSize, "%c%ldm", sign, (mag + 30) / 60);
  } else if (sign) {
    std::snprintf(out, outSize, "%c%lds", sign, mag);
  } else {
    std::snprintf(out, outSize, "0s");
  }
}

}  // namespace clockface
