#include "LocationService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config/weather_config.h"

namespace {
// ip-api.com's free tier is HTTP-only (HTTPS needs a paid plan). "fields="
// restricts the response to just what we use.
constexpr char kApiUrl[] =
    "http://ip-api.com/json/?fields=status,message,lat,lon,city,timezone,offset";
constexpr unsigned long kRefreshIntervalMs = 6UL * 60 * 60 * 1000;  // 6 hours
// Full DST-aware fallback for Europe/Berlin, used only if the API is
// unreachable (no periodic refresh possible either, so this needs its own
// transition rules rather than a fixed offset).
constexpr char kFallbackPosixTz[] = "CET-1CEST,M3.5.0,M10.5.0/3";

// Builds a fixed-offset POSIX TZ string (no DST rules) from a UTC offset in
// seconds - e.g. 7200 (UTC+2) -> "UTC-2". POSIX TZ sign convention is
// inverted from normal usage: local time = UTC - offset.
String buildFixedOffsetTz(long utcOffsetSeconds) {
  const long posixOffsetSeconds = -utcOffsetSeconds;
  const int hours = static_cast<int>(posixOffsetSeconds / 3600);
  const int minutes = static_cast<int>(labs(posixOffsetSeconds % 3600) / 60);

  char buf[16];
  if (minutes == 0) {
    snprintf(buf, sizeof(buf), "UTC%+d", hours);
  } else {
    snprintf(buf, sizeof(buf), "UTC%+d:%02d", hours, minutes);
  }
  return String(buf);
}
}  // namespace

LocationService::LocationService() = default;

void LocationService::begin() {
  if (WiFi.status() == WL_CONNECTED) {
    fetchLocation();
  }
  lastFetchMs_ = millis();
}

bool LocationService::update() {
  const unsigned long now = millis();
  if (now - lastFetchMs_ < kRefreshIntervalMs) {
    return false;
  }
  lastFetchMs_ = now;
  return fetchLocation();
}

bool LocationService::fetchLocation() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LocationService] skipped fetch: Wi-Fi not connected");
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  if (!http.begin(kApiUrl)) {
    Serial.println("[LocationService] http.begin() failed");
    return false;
  }

  const int status = http.GET();
  Serial.printf("[LocationService] GET -> %d\n", status);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String body = http.getString();
  http.end();

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[LocationService] JSON parse failed: %s\n", err.c_str());
    return false;
  }

  const char *apiStatus = doc["status"] | "";
  if (strcmp(apiStatus, "success") != 0) {
    const char *message = doc["message"] | "unknown error";
    Serial.printf("[LocationService] API error: %s\n", message);
    return false;
  }

  latitudeValue_ = doc["lat"] | 0.0f;
  longitudeValue_ = doc["lon"] | 0.0f;
  cityName_ = String(static_cast<const char *>(doc["city"] | "Unknown"));
  utcOffsetSeconds_ = doc["offset"] | 0L;
  hasData_ = true;
  Serial.printf("[LocationService] %s: %.4f, %.4f, UTC offset %lds\n", cityName_.c_str(),
                latitudeValue_, longitudeValue_, utcOffsetSeconds_);
  return true;
}

String LocationService::latitude() const {
  return hasData_ ? String(latitudeValue_, 4) : String(WEATHER_LATITUDE);
}

String LocationService::longitude() const {
  return hasData_ ? String(longitudeValue_, 4) : String(WEATHER_LONGITUDE);
}

String LocationService::cityName() const {
  return hasData_ ? cityName_ : String(WEATHER_LOCATION_NAME);
}

String LocationService::posixTimezone() const {
  return hasData_ ? buildFixedOffsetTz(utcOffsetSeconds_) : String(kFallbackPosixTz);
}
