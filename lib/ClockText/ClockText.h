#pragma once

#include <cstddef>
#include <ctime>

// Formatting for the clock scene, split out from ClockDisplay so it runs
// on the host (see the `native` env in platformio.ini and
// test/test_clock_text). ClockDisplay::render() draws formatDate() (the
// time itself is drawn digit by digit); the timezone itself is static config
// (include/config/location_config.h).
namespace clockface {

// Writes a zero-padded 24-hour "HH:MM:SS" for `t` into `out`. `outSize`
// must be at least 9 (8 digits/colons + NUL); output is always
// NUL-terminated.
void formatHms(const struct tm &t, char *out, size_t outSize);

// Writes a zero-padded 24-hour "HH:MM". `outSize` must be at least 6.
void formatHm(const struct tm &t, char *out, size_t outSize);

// Writes a short date like "Mon 26 Oct" (day not zero-padded, English
// names, independent of the C locale). `outSize` must be at least 11.
void formatDate(const struct tm &t, char *out, size_t outSize);

}  // namespace clockface
