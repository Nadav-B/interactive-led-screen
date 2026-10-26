#include "LocationService.h"

// location_config.h pins a physical location, so it's git-ignored. On a
// fresh checkout it won't exist - fall back to the committed template.
// Copy include/config/location_config.h.example to location_config.h and
// edit it to set your own.
#if __has_include("config/location_config.h")
#include "config/location_config.h"
#else
#include "config/location_config.h.example"
#endif

#include <stdlib.h>

namespace {
constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.nist.gov";
}  // namespace

LocationService::LocationService() = default;

void LocationService::begin() {
  Serial.printf("[LocationService] %s @ %s, %s  TZ=%s\n", LOCATION_NAME, LOCATION_LATITUDE,
                LOCATION_LONGITUDE, LOCATION_POSIX_TZ);
  configTzTime(LOCATION_POSIX_TZ, kNtpServer1, kNtpServer2);
}

String LocationService::latitude() const { return String(LOCATION_LATITUDE); }

String LocationService::longitude() const { return String(LOCATION_LONGITUDE); }

double LocationService::latitudeDeg() const { return atof(LOCATION_LATITUDE); }

double LocationService::longitudeDeg() const { return atof(LOCATION_LONGITUDE); }

String LocationService::cityName() const { return String(LOCATION_NAME); }

