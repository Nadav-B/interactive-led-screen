#pragma once

#include <string>

// Fixed-offset POSIX TZ string builder. Pure and Arduino-free so it runs
// on the host (see the `native` env in platformio.ini and
// test/test_posix_tz). Used by LocationService::posixTimezone().
namespace tz {

// Builds a fixed-offset POSIX TZ string (no DST rules) from a UTC offset
// in seconds - e.g. 7200 (UTC+2) -> "UTC-2". The POSIX TZ sign
// convention is inverted from normal usage: local time = UTC - offset.
std::string buildFixedOffsetTz(long utcOffsetSeconds);

}  // namespace tz
