#pragma once

#include <Arduino.h>

// Runtime-configurable weather location, persisted to flash (NVS, via
// Preferences) so changes survive reboots without reflashing. Falls back
// to the compile-time defaults in config/weather_config.h until the first
// time a value is saved through SettingsServer's web form.
namespace WeatherSettings {
String latitude();
String longitude();
String locationName();

// Persists new values; WeatherDisplay picks them up on its next fetch.
void save(const String &latitude, const String &longitude, const String &locationName);
}  // namespace WeatherSettings
