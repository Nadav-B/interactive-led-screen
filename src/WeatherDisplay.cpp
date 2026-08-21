#include "WeatherDisplay.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include "WeatherSettings.h"

namespace {
// Open-Meteo needs no API key. Location comes from WeatherSettings, which
// is itself backed by config/weather_config.h until changed at runtime via
// SettingsServer.
String buildApiUrl() {
  return "https://api.open-meteo.com/v1/forecast?latitude=" + WeatherSettings::latitude() +
         "&longitude=" + WeatherSettings::longitude() +
         "&current=temperature_2m,relative_humidity_2m,weather_code";
}
}  // namespace

WeatherDisplay::WeatherDisplay(MatrixPanel_I2S_DMA *matrix) : matrix_(matrix) {}

void WeatherDisplay::begin() {
  if (WiFi.status() == WL_CONNECTED) {
    fetchWeather();
  }
}

WeatherDisplay::Condition WeatherDisplay::conditionFromCode(int code) {
  // WMO weather interpretation codes, as used by Open-Meteo.
  if (code == 0) return Condition::Clear;
  if (code >= 1 && code <= 3) return Condition::PartlyCloudy;
  if (code == 45 || code == 48) return Condition::Fog;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return Condition::Rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Condition::Snow;
  if (code >= 95 && code <= 99) return Condition::Thunder;
  return Condition::Unknown;
}

bool WeatherDisplay::fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WeatherDisplay] skipped fetch: Wi-Fi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  // Defaults are tuned for plain HTTP; the TLS handshake to an external host
  // can easily take longer than that on an ESP32, especially on first boot.
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  if (!http.begin(client, buildApiUrl())) {
    Serial.println("[WeatherDisplay] http.begin() failed");
    return false;
  }

  const int status = http.GET();
  Serial.printf("[WeatherDisplay] GET -> %d\n", status);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  // Reading via getString() (rather than getStream() directly into
  // deserializeJson) avoids a known ESP32 HTTPClient issue where
  // chunked-transfer-encoding framing bytes leak into the body when read
  // straight off the stream, which ArduinoJson then rejects as InvalidInput.
  const String body = http.getString();
  http.end();

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[WeatherDisplay] JSON parse failed: %s\n", err.c_str());
    Serial.printf("[WeatherDisplay] body (first 200 chars): %.200s\n", body.c_str());
    return false;
  }

  JsonObjectConst current = doc["current"];
  if (current.isNull()) {
    Serial.println("[WeatherDisplay] response had no \"current\" object");
    return false;
  }

  temperatureC_ = current["temperature_2m"] | 0.0f;
  humidityPercent_ = current["relative_humidity_2m"] | 0;
  const int code = current["weather_code"] | -1;
  condition_ = conditionFromCode(code);
  hasData_ = true;
  Serial.printf("[WeatherDisplay] %.1fC, %d%% RH, code %d\n", temperatureC_, humidityPercent_,
                code);
  return true;
}

void WeatherDisplay::update() {
  const unsigned long now = millis();
  if (lastFetchMs_ != 0 && now - lastFetchMs_ < kRefreshIntervalMs) {
    return;
  }
  lastFetchMs_ = now;
  fetchWeather();
  render();
}

void WeatherDisplay::show() { render(); }

void WeatherDisplay::drawIcon(int cx, int cy, Condition condition) const {
  const uint16_t grey = matrix_->color565(180, 180, 190);
  const uint16_t darkGrey = matrix_->color565(110, 110, 120);
  const uint16_t yellow = matrix_->color565(255, 200, 40);
  const uint16_t blue = matrix_->color565(70, 140, 255);
  const uint16_t white = matrix_->color565(240, 240, 250);

  auto drawCloud = [&](uint16_t color) {
    matrix_->fillCircle(cx - 3, cy, 3, color);
    matrix_->fillCircle(cx + 2, cy - 1, 4, color);
    matrix_->fillRect(cx - 6, cy, 12, 4, color);
  };

  switch (condition) {
    case Condition::Clear:
      matrix_->fillCircle(cx, cy, 4, yellow);
      matrix_->drawPixel(cx - 6, cy, yellow);
      matrix_->drawPixel(cx + 6, cy, yellow);
      matrix_->drawPixel(cx, cy - 6, yellow);
      matrix_->drawPixel(cx, cy + 6, yellow);
      break;
    case Condition::PartlyCloudy:
      matrix_->fillCircle(cx - 4, cy - 3, 3, yellow);
      drawCloud(grey);
      break;
    case Condition::Fog:
      matrix_->drawFastHLine(cx - 6, cy - 3, 12, grey);
      matrix_->drawFastHLine(cx - 6, cy, 12, darkGrey);
      matrix_->drawFastHLine(cx - 6, cy + 3, 12, grey);
      break;
    case Condition::Rain:
      drawCloud(grey);
      matrix_->drawFastVLine(cx - 3, cy + 5, 3, blue);
      matrix_->drawFastVLine(cx, cy + 5, 3, blue);
      matrix_->drawFastVLine(cx + 3, cy + 5, 3, blue);
      break;
    case Condition::Snow:
      drawCloud(grey);
      matrix_->drawPixel(cx - 3, cy + 6, white);
      matrix_->drawPixel(cx, cy + 6, white);
      matrix_->drawPixel(cx + 3, cy + 6, white);
      break;
    case Condition::Thunder:
      drawCloud(darkGrey);
      matrix_->drawLine(cx, cy + 4, cx - 2, cy + 6, yellow);
      matrix_->drawLine(cx - 2, cy + 6, cx + 1, cy + 6, yellow);
      matrix_->drawLine(cx + 1, cy + 6, cx - 1, cy + 8, yellow);
      break;
    case Condition::Unknown:
    default:
      drawCloud(grey);
      break;
  }
}

void WeatherDisplay::render() const {
  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->setTextWrap(false);

  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(120, 190, 255));
  matrix_->setCursor(2, 0);
  matrix_->print(WeatherSettings::locationName());

  if (!hasData_) {
    matrix_->setTextColor(matrix_->color565(180, 60, 60));
    matrix_->setCursor(2, 14);
    matrix_->print("no data");
    return;
  }

  drawIcon(9, 16, condition_);

  matrix_->setTextSize(2);
  matrix_->setTextColor(matrix_->color565(230, 230, 230));
  char tempText[6];
  snprintf(tempText, sizeof(tempText), "%dC", static_cast<int>(lroundf(temperatureC_)));
  matrix_->setCursor(20, 11);
  matrix_->print(tempText);

  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(150, 150, 160));
  char detailText[8];
  snprintf(detailText, sizeof(detailText), "%d%% RH", humidityPercent_);
  matrix_->setCursor(2, 25);
  matrix_->print(detailText);
}
