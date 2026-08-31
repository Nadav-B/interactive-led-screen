#include "ClockText.h"

#include <cstdio>

namespace clockface {

void formatHms(const struct tm &t, char *out, size_t outSize) {
  std::snprintf(out, outSize, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
}

}  // namespace clockface
