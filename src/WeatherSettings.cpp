#include "WeatherSettings.h"

#include <Preferences.h>

#include "config/weather_config.h"

namespace {
constexpr char kNamespace[] = "weather";
constexpr char kKeyLat[] = "lat";
constexpr char kKeyLon[] = "lon";
constexpr char kKeyName[] = "name";

// Opened read-write (not read-only) even for reads: Preferences::begin()
// logs a scary-looking but harmless "nvs_open failed: NOT_FOUND" from the
// underlying NVS layer if the namespace doesn't exist yet and it's opened
// read-only, since read-only can't create it. Read-write avoids that.
String getOrDefault(const char *key, const char *defaultValue) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  const String value = prefs.getString(key, defaultValue);
  prefs.end();
  return value;
}
}  // namespace

namespace WeatherSettings {

String latitude() { return getOrDefault(kKeyLat, WEATHER_LATITUDE); }

String longitude() { return getOrDefault(kKeyLon, WEATHER_LONGITUDE); }

String locationName() { return getOrDefault(kKeyName, WEATHER_LOCATION_NAME); }

void save(const String &latitude, const String &longitude, const String &locationName) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putString(kKeyLat, latitude);
  prefs.putString(kKeyLon, longitude);
  prefs.putString(kKeyName, locationName);
  prefs.end();
}

}  // namespace WeatherSettings
