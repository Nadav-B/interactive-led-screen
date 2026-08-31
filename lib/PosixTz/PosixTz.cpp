#include "PosixTz.h"

#include <cstdio>
#include <cstdlib>

namespace tz {

std::string buildFixedOffsetTz(long utcOffsetSeconds) {
  const long posixOffsetSeconds = -utcOffsetSeconds;
  const int hours = static_cast<int>(posixOffsetSeconds / 3600);
  const int minutes = static_cast<int>(std::labs(posixOffsetSeconds % 3600) / 60);

  char buf[16];
  if (minutes == 0) {
    std::snprintf(buf, sizeof(buf), "UTC%+d", hours);
  } else {
    std::snprintf(buf, sizeof(buf), "UTC%+d:%02d", hours, minutes);
  }
  return std::string(buf);
}

}  // namespace tz
