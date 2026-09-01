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

LocationService::LocationService() = default;

void LocationService::begin() {
  Serial.printf("[LocationService] %s @ %s, %s  TZ=%s\n", LOCATION_NAME, LOCATION_LATITUDE,
                LOCATION_LONGITUDE, LOCATION_POSIX_TZ);
}

String LocationService::latitude() const { return String(LOCATION_LATITUDE); }

String LocationService::longitude() const { return String(LOCATION_LONGITUDE); }

String LocationService::cityName() const { return String(LOCATION_NAME); }

String LocationService::posixTimezone() const { return String(LOCATION_POSIX_TZ); }
