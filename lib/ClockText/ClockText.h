#pragma once

#include <cstddef>
#include <ctime>

// Time and date text for the scenes, split out so it runs on the host
// (see the `native` env in platformio.ini and test/test_clock_text):
// SunMoonDisplay's rise/set times use formatHm() and its day length and
// daily change formatDuration() / formatDelta(), ClockDisplay's date pill
// formatDate(). The timezone itself is static config
// (include/config/location_config.h).
namespace clockface {

// Writes a zero-padded 24-hour "HH:MM" for `t` into `out`. `outSize` must
// be at least 6; output is always NUL-terminated.
void formatHm(const struct tm &t, char *out, size_t outSize);

// Writes a short date like "Mon 26 Oct" (day not zero-padded, English
// names, independent of the C locale). `outSize` must be at least 11.
void formatDate(const struct tm &t, char *out, size_t outSize);

// Writes a duration like a day length as hours and minutes, "9h55" or
// "16h05" (minutes rounded, zero-padded; negative clamps to "0h00").
// `outSize` must be at least 7.
void formatDuration(long seconds, char *out, size_t outSize);

// Writes a small signed change like a day length's change since
// yesterday: whole minutes from 90s up ("+3m", "-2m"), seconds below
// ("+45s", "-8s", "0s"). `outSize` must be at least 7.
void formatDelta(long seconds, char *out, size_t outSize);

}  // namespace clockface
