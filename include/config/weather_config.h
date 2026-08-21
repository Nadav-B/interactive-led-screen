#pragma once

// Fallback location, used only if LocationService's IP geolocation lookup
// fails (e.g. no internet reachability yet, or the API is down). Normally
// the location is auto-detected - see include/LocationService.h.

#define WEATHER_LATITUDE "52.52"
#define WEATHER_LONGITUDE "13.405"
#define WEATHER_LOCATION_NAME "Berlin"
