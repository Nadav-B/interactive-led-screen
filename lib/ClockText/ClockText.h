#pragma once

#include <cstddef>
#include <ctime>

// Time and date text for the scenes, split out so it runs on the host
// (see the `native` env in platformio.ini and test/test_clock_text):
// SunMoonDisplay's rise/set times use formatHm(), ClockDisplay's date pill
// formatDate(). The timezone itself is static config
// (include/config/location_config.h).
namespace clockface {

// Writes a zero-padded 24-hour "HH:MM" for `t` into `out`. `outSize` must
// be at least 6; output is always NUL-terminated.
void formatHm(const struct tm &t, char *out, size_t outSize);

// Writes a short date like "Mon 26 Oct" (day not zero-padded, English
// names, independent of the C locale). `outSize` must be at least 11.
void formatDate(const struct tm &t, char *out, size_t outSize);

}  // namespace clockface
