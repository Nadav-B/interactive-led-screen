#include "ClockText.h"

#include <cstdio>

namespace clockface {

void formatHms(const struct tm &t, char *out, size_t outSize) {
  std::snprintf(out, outSize, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
}

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

}  // namespace clockface
