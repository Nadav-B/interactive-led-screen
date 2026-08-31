#pragma once

#include <cstddef>
#include <ctime>

// Formatting for the clock scene, split out from ClockDisplay so it runs
// on the host (see the `native` env in platformio.ini and
// test/test_clock_text). ClockDisplay::render() calls formatHms(); the
// timezone side of "time" lives in lib/PosixTz.
namespace clockface {

// Writes a zero-padded 24-hour "HH:MM:SS" for `t` into `out`. `outSize`
// must be at least 9 (8 digits/colons + NUL); output is always
// NUL-terminated.
void formatHms(const struct tm &t, char *out, size_t outSize);

}  // namespace clockface
