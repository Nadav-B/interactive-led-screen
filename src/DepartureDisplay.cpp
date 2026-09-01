#include "DepartureDisplay.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "DepartureParsing.h"

namespace {
// BVG transport.rest is HTTPS-only and needs no API key. The trimming
// query params (remarks/linesOfStops/stopovers=false) keep the arrivals
// response small enough to parse comfortably on the ESP32.
constexpr char kApiBase[] = "https://v6.bvg.transport.rest";

String nearbyUrl(LocationService *loc) {
  return String(kApiBase) + "/locations/nearby?latitude=" + loc->latitude() +
         "&longitude=" + loc->longitude() + "&results=8&poi=false&linesOfStops=false";
}

// Restrict the arrivals to U-Bahn only. At stops served by frequent trams
// or buses (e.g. U Bernauer Str., which is mostly M10) the subway lines
// would otherwise be crowded out of the small results window entirely.
constexpr char kSubwayOnlyFilter[] =
    "&subway=true&suburban=false&tram=false&bus=false&ferry=false&express=false&regional=false";

String arrivalsUrl(const String &stopId, int durationMin) {
  return String(kApiBase) + "/stops/" + stopId + "/arrivals?duration=" + String(durationMin) +
         "&results=8&remarks=false&linesOfStops=false&stopovers=false&pretty=false" +
         kSubwayOnlyFilter;
}

// One blocking GET against the API, body returned via `out`. Mirrors the
// TLS setup WeatherDisplay uses (setInsecure + generous timeouts).
bool httpGetString(const String &url, String &out, const char *tag) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[%s] skipped fetch: Wi-Fi not connected\n", tag);
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) {
    Serial.printf("[%s] http.begin() failed\n", tag);
    return false;
  }

  const int status = http.GET();
  Serial.printf("[%s] GET -> %d\n", tag, status);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  out = http.getString();
  http.end();
  return true;
}

}  // namespace

DepartureDisplay::DepartureDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService)
    : matrix_(matrix), locationService_(locationService) {}

void DepartureDisplay::begin() {
  if (WiFi.status() == WL_CONNECTED) {
    fetchArrivals();
  }
  lastFetchMs_ = millis();
}

bool DepartureDisplay::resolveStop() {
  const String lat = locationService_->latitude();
  const String lon = locationService_->longitude();

  String body;
  if (!httpGetString(nearbyUrl(locationService_), body, "DepartureDisplay")) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[DepartureDisplay] nearby JSON parse failed: %s\n", err.c_str());
    return false;
  }

  for (JsonObjectConst entry : doc.as<JsonArrayConst>()) {
    if (strcmp(entry["type"] | "", "stop") != 0) {
      continue;
    }
    const char *id = entry["id"] | "";
    if (id[0] == '\0') {
      continue;
    }
    stopId_ = id;
    stopName_ = bvg::shortenName(entry["name"] | "").c_str();
    resolvedForLat_ = lat;
    resolvedForLon_ = lon;
    Serial.printf("[DepartureDisplay] nearest stop: %s (%s)\n", stopName_.c_str(), stopId_.c_str());
    return true;
  }

  Serial.println("[DepartureDisplay] nearby response had no stop");
  return false;
}

int DepartureDisplay::minutesUntil(const char *isoWhen, char *outHHMM, size_t outSize) const {
  int hh, mm;
  if (!bvg::parseIsoHourMinute(isoWhen, hh, mm, outHHMM, outSize)) {
    return -1;
  }
  struct tm now;
  if (!getLocalTime(&now, 0)) {
    return -1;  // Time not synced yet; caller falls back to the HH:MM text.
  }
  return bvg::minutesFromNow(now.tm_hour, now.tm_min, hh, mm);
}

bool DepartureDisplay::fetchArrivals() {
  if (stopId_.isEmpty() && !resolveStop()) {
    return false;
  }

  String body;
  if (!httpGetString(arrivalsUrl(stopId_, kArrivalsDurationMin), body, "DepartureDisplay")) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[DepartureDisplay] arrivals JSON parse failed: %s\n", err.c_str());
    return false;
  }

  JsonArrayConst arrivals = doc["arrivals"].as<JsonArrayConst>();
  if (arrivals.isNull()) {
    Serial.println("[DepartureDisplay] response had no \"arrivals\" array");
    return false;
  }

  int n = 0;
  for (JsonObjectConst a : arrivals) {
    if (n >= kMaxRows) {
      break;
    }
    const char *when = a["when"] | "";
    if (when[0] == '\0') {
      when = a["plannedWhen"] | "";
    }
    if (when[0] == '\0') {
      continue;
    }
    Departure &d = departures_[n];
    d.line = String(static_cast<const char *>(a["line"]["name"] | "?"));
    const char *dir = a["direction"] | "";
    if (dir[0] == '\0') {
      dir = a["provenance"] | "";
    }
    d.direction = bvg::shortenName(dir).c_str();
    d.minutes = minutesUntil(when, d.whenText, sizeof(d.whenText));
    n++;
  }

  departureCount_ = n;
  hasData_ = true;
  Serial.printf("[DepartureDisplay] %s: %d departures\n", stopName_.c_str(), departureCount_);
  return true;
}

bool DepartureDisplay::update() {
  const unsigned long now = millis();
  const unsigned long interval = hasData_ ? kRefreshIntervalMs : kRetryIntervalMs;
  if (lastFetchMs_ != 0 && now - lastFetchMs_ < interval) {
    return false;
  }
  lastFetchMs_ = now;

  // Re-resolve the stop only if the detected location has moved since the
  // id was last resolved (normally never).
  if (!stopId_.isEmpty() && (locationService_->latitude() != resolvedForLat_ ||
                             locationService_->longitude() != resolvedForLon_)) {
    resolveStop();
  }
  fetchArrivals();
  return true;
}

void DepartureDisplay::show() { render(); }

void DepartureDisplay::render() const {
  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->setTextWrap(false);
  matrix_->setTextSize(1);

  matrix_->setTextColor(matrix_->color565(120, 190, 255));
  matrix_->setCursor(1, 0);
  matrix_->print(stopName_.isEmpty() ? "Departures" : stopName_.substring(0, 10));

  if (!hasData_) {
    matrix_->setTextColor(matrix_->color565(180, 60, 60));
    matrix_->setCursor(1, 12);
    matrix_->print("no data");
    return;
  }
  if (departureCount_ == 0) {
    matrix_->setTextColor(matrix_->color565(150, 150, 160));
    matrix_->setCursor(1, 12);
    matrix_->print("none soon");
    return;
  }

  for (int i = 0; i < departureCount_; i++) {
    const Departure &d = departures_[i];
    const int y = 8 + i * 8;

    // Line badge, coloured by the transport class the name implies.
    uint16_t lineColor;
    switch (d.line[0]) {
      case 'U': lineColor = matrix_->color565(255, 170, 40); break;   // U-Bahn - amber
      case 'S': lineColor = matrix_->color565(60, 200, 90); break;    // S-Bahn - green
      case 'M': lineColor = matrix_->color565(230, 80, 80); break;    // Metro tram/bus - red
      default: lineColor = matrix_->color565(150, 160, 255); break;   // Bus - blue
    }
    matrix_->setTextColor(lineColor);
    matrix_->setCursor(1, y);
    matrix_->print(d.line.substring(0, 3));

    // Countdown, right-aligned against the panel edge.
    char countdown[6];
    if (d.minutes < 0) {
      snprintf(countdown, sizeof(countdown), "%s", d.whenText);
    } else if (d.minutes == 0) {
      snprintf(countdown, sizeof(countdown), "now");
    } else {
      snprintf(countdown, sizeof(countdown), "%d'", d.minutes > 99 ? 99 : d.minutes);
    }
    const int cdWidth = static_cast<int>(strlen(countdown)) * 6;
    matrix_->setTextColor(matrix_->color565(230, 230, 230));
    matrix_->setCursor(64 - cdWidth, y);
    matrix_->print(countdown);

    // Whatever of the headsign fits between the badge and the countdown.
    const int dirX = 1 + 3 * 6 + 2;
    const int dirChars = (64 - cdWidth - 2 - dirX) / 6;
    if (dirChars > 0 && !d.direction.isEmpty()) {
      matrix_->setTextColor(matrix_->color565(150, 150, 160));
      matrix_->setCursor(dirX, y);
      matrix_->print(d.direction.substring(0, dirChars));
    }
  }
}
