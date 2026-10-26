#include "WeatherDisplay.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <time.h>

#include "SceneStyle.h"
#include "SkyEvents.h"

namespace {
// Open-Meteo needs no API key. Location comes from LocationService
// (include/config/location_config.h).
String buildApiUrl(LocationService *locationService) {
  return "https://api.open-meteo.com/v1/forecast?latitude=" + locationService->latitude() +
         "&longitude=" + locationService->longitude() +
         "&current=temperature_2m,relative_humidity_2m,weather_code,is_day";
}

// The condition art is centred in the left third; pills fill the right.
constexpr float kArtX = 12.0f, kArtY = 16.0f;
constexpr int kPillX = 25, kPillW = 38;

constexpr uint8_t kCloudLight[3] = {240, 244, 255};
constexpr uint8_t kCloudShade[3] = {165, 180, 212};
constexpr uint8_t kNightLight[3] = {150, 165, 205};  // clouds after dark: cooler, dimmer
constexpr uint8_t kNightShade[3] = {92, 105, 145};
constexpr uint8_t kStormLight[3] = {125, 135, 168};
constexpr uint8_t kStormShade[3] = {78, 86, 118};
constexpr float kFogRgb[3] = {150, 165, 192};
constexpr float kFlashRgb[3] = {255, 255, 255};

// Signed distance from (px, py) to the segment a-b, minus radius r.
float capsuleSdf(float px, float py, float ax, float ay, float bx, float by, float r) {
  const float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
  const float h = style::clamp01((pax * bax + pay * bay) / (bax * bax + bay * bay));
  const float dx = pax - bax * h, dy = pay - bay * h;
  return sqrtf(dx * dx + dy * dy) - r;
}

float circleSdf(float px, float py, float cx, float cy, float r) {
  const float dx = px - cx, dy = py - cy;
  return sqrtf(dx * dx + dy * dy) - r;
}
}  // namespace

WeatherDisplay::WeatherDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                               int width, int height)
    : matrix_(matrix),
      locationService_(locationService),
      width_(width),
      height_(height),
      frame_(new uint8_t[width * height * 3]) {}

WeatherDisplay::~WeatherDisplay() { delete[] frame_; }

void WeatherDisplay::begin() {
  if (WiFi.status() == WL_CONNECTED) {
    fetchWeather();
  }
  lastFetchMs_ = millis();
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
  if (!http.begin(client, buildApiUrl(locationService_))) {
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
  isDay_ = (current["is_day"] | 1) != 0;
  hasData_ = true;
  Serial.printf("[WeatherDisplay] %.1fC, %d%% RH, code %d, %s\n", temperatureC_,
                humidityPercent_, code, isDay_ ? "day" : "night");
  return true;
}

bool WeatherDisplay::update() {
  const unsigned long now = millis();
  const unsigned long interval = hasData_ ? kRefreshIntervalMs : kRetryIntervalMs;
  if (lastFetchMs_ != 0 && now - lastFetchMs_ < interval) {
    return false;
  }
  lastFetchMs_ = now;
  fetchWeather();
  return true;
}

void WeatherDisplay::show() {
  startMs_ = millis();
  lastFrameMs_ = 0;
}

void WeatherDisplay::animate() {
  const unsigned long now = millis();
  if (lastFrameMs_ != 0 && now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  lastFrameMs_ = now;
  render((now - startMs_) / 1000.0f);
  style::blit(matrix_, frame_, width_, height_);
}

void WeatherDisplay::render(float tSec) {
  style::drawSky(frame_, width_, height_, tSec, 0);
  drawCondition(tSec);
  drawPills(tSec);
}

// Flat two-tone capsule cloud, about 18px wide at scale 1: three puffs on
// a capsule base, light on top and a cooler shade underneath.
void WeatherDisplay::drawCloud(float cx, float cy, float scale, bool stormy) {
  if (scale <= 0.05f) return;
  const uint8_t *light = stormy ? kStormLight : (isDay_ ? kCloudLight : kNightLight);
  const uint8_t *shade = stormy ? kStormShade : (isDay_ ? kCloudShade : kNightShade);
  const float s = scale;
  const int x0 = static_cast<int>(cx - 10 * s), x1 = static_cast<int>(cx + 10 * s) + 1;
  const int y0 = static_cast<int>(cy - 7 * s), y1 = static_cast<int>(cy + 6 * s) + 1;
  for (int y = y0 < 0 ? 0 : y0; y < height_ && y <= y1; ++y) {
    for (int x = x0 < 0 ? 0 : x0; x < width_ && x <= x1; ++x) {
      float d = capsuleSdf(x, y, cx - 5.5f * s, cy + 2.5f * s, cx + 6 * s, cy + 2.5f * s, 2.8f * s);
      d = fminf(d, circleSdf(x, y, cx - 3.5f * s, cy + 0.5f * s, 3.4f * s));
      d = fminf(d, circleSdf(x, y, cx + 1.0f * s, cy - 1.8f * s, 4.4f * s));
      d = fminf(d, circleSdf(x, y, cx + 5.5f * s, cy + 1.0f * s, 3.0f * s));
      const float cover = style::clamp01(0.5f - d);
      if (cover <= 0) continue;
      float c[3];
      style::lerpRgb(light, shade, style::smoothstep(cy + 1.0f * s, cy + 3.0f * s, y), c);
      style::blendRgb(frame_ + (y * width_ + x) * 3, c, cover);
    }
  }
}

void WeatherDisplay::drawCondition(float tSec) {
  const float pop = style::spring(tSec);
  const float drift = sinf(tSec * 0.9f) * 1.5f;

  // Sun by day; by night the moon, lit for its real current phase.
  auto drawSky = [&](float cx, float cy, float r) {
    if (isDay_) {
      style::drawSun(frame_, width_, height_, cx, cy, r * pop, tSec);
    } else {
      const sky::MoonPhase phase = sky::moonPhase(time(nullptr));
      style::drawMoon(frame_, width_, height_, cx, cy, r * pop, static_cast<float>(phase.age),
                      static_cast<float>(phase.illumination));
    }
  };

  // Falling particles under a cloud: rain streaks or swaying snow.
  auto drawFall = [&](float speed, bool snow, const uint8_t rgb[3]) {
    const float xs[] = {6, 10, 14, 18, 8, 16};
    for (int i = 0; i < 6; ++i) {
      const float fall = fmodf(tSec * speed + i * 4.7f, 12.0f);
      const float k = (1 - fall / 12.0f) * pop;
      const float fc[3] = {static_cast<float>(rgb[0]), static_cast<float>(rgb[1]),
                           static_cast<float>(rgb[2])};
      const int y = static_cast<int>(kArtY + 4 + fall);
      int x = static_cast<int>(xs[i] - fall * 0.25f);
      if (snow) x = static_cast<int>(xs[i] + sinf(tSec * 2 + i) * 1.2f);
      // Rain: a 3px streak fading up its tail. Snow: a single flake.
      const float tail[] = {1.0f, 0.6f, 0.3f};
      for (int j = 0; j < (snow ? 1 : 3); ++j) {
        const int yy = y - j;
        if (x < 0 || x >= width_ || yy < 0 || yy >= height_) continue;
        style::addRgb(frame_ + (yy * width_ + x) * 3, fc, fminf(1, k * 1.3f) * tail[j]);
      }
    }
  };

  switch (condition_) {
    case Condition::Clear:
      drawSky(kArtX, kArtY, 7);
      break;
    case Condition::PartlyCloudy:
      drawSky(kArtX - 4, kArtY - 6, 6);
      drawCloud(kArtX + 2 + drift, kArtY + 5, 0.8f * pop, false);
      break;
    case Condition::Fog:
      drawSky(kArtX, kArtY - 2, 6);
      for (int i = 0; i < 3; ++i) {
        const float off = sinf(tSec * 0.8f + i * 2.1f) * 3;
        const int y = static_cast<int>(kArtY - 3 + i * 5);
        for (int x = 0; x < 24; ++x) {
          const float k = 0.75f * pop * style::clamp01(1 - fabsf(x - kArtX - off) / 11.0f);
          for (int dy = 0; dy < 2; ++dy) {
            style::addRgb(frame_ + ((y + dy) * width_ + x) * 3, kFogRgb, k * (dy ? 0.5f : 1));
          }
        }
      }
      break;
    case Condition::Rain:
      drawFall(14, false, style::kCyan);
      drawCloud(kArtX + drift * 0.5f, kArtY - 3, pop, false);
      break;
    case Condition::Snow:
      drawFall(5, true, style::kWhite);
      drawCloud(kArtX + drift * 0.5f, kArtY - 3, pop, false);
      break;
    case Condition::Thunder: {
      drawFall(16, false, style::kCyan);
      drawCloud(kArtX + drift * 0.5f, kArtY - 3, pop, true);
      // Lightning every 2.4s: a bolt and a quick white flash.
      const float since = fmodf(tSec, 2.4f);
      if (since < 0.15f && tSec > 0.5f) {
        style::FrameCanvas canvas(frame_, width_, height_);
        const uint16_t bolt = style::rgb565(style::kGold);
        const int bx = static_cast<int>(kArtX), by = static_cast<int>(kArtY + 2);
        canvas.drawLine(bx + 1, by, bx - 2, by + 4, bolt);
        canvas.drawLine(bx - 2, by + 4, bx + 1, by + 4, bolt);
        canvas.drawLine(bx + 1, by + 4, bx - 1, by + 9, bolt);
      }
      const float flash = 0.35f * (1 - style::clamp01(since / 0.2f));
      if (flash > 0 && tSec > 0.5f) {
        for (int i = 0; i < width_ * height_; ++i) style::addRgb(frame_ + i * 3, kFlashRgb, flash);
      }
      break;
    }
    case Condition::Unknown:
    default:  // overcast: two clouds drifting at different depths
      drawCloud(kArtX - 3 - drift * 0.6f, kArtY - 4, 0.7f * pop, true);
      drawCloud(kArtX + 1 + drift, kArtY + 3, 0.9f * pop, false);
      break;
  }
}

void WeatherDisplay::drawPills(float tSec) {
  style::FrameCanvas canvas(frame_, width_, height_);
  canvas.setTextWrap(false);
  canvas.setTextSize(1);

  if (tSec >= 0.35f) {
    // Temperature: sun/snowflake icon, then e.g. "11°C" (247 is the degree
    // sign in the GFX default font).
    const int ty = style::openPill(canvas, kPillX, 3, kPillW, tSec - 0.35f);
    const uint8_t *tint = !hasData_ || temperatureC_ >= 15  ? style::kGold
                          : temperatureC_ >= 5             ? style::kWhite
                                                           : style::kCyan;
    const uint16_t color = style::rgb565(tint);
    // Icon in the temperature's tint: a little sun when it's mild or
    // warm, a snowflake when it's cold.
    const int ix = kPillX + 4, iy = ty + 3;
    if (hasData_ && temperatureC_ < 5) {
      canvas.drawFastVLine(ix, iy - 3, 7, color);
      canvas.drawFastHLine(ix - 2, iy, 5, color);
      canvas.drawLine(ix - 2, iy - 2, ix + 2, iy + 2, color);
      canvas.drawLine(ix - 2, iy + 2, ix + 2, iy - 2, color);
    } else {
      canvas.fillCircle(ix, iy, 1, color);
      for (int k = 0; k < 4; ++k) {  // short rays, straight and diagonal
        const int dx[] = {0, 3, 0, -3}, dy[] = {-3, 0, 3, 0};
        canvas.drawPixel(ix + dx[k], iy + dy[k], color);
      }
      canvas.drawPixel(ix - 2, iy - 2, color);
      canvas.drawPixel(ix + 2, iy - 2, color);
      canvas.drawPixel(ix - 2, iy + 2, color);
      canvas.drawPixel(ix + 2, iy + 2, color);
    }
    char text[8] = "--";
    if (hasData_) {
      snprintf(text, sizeof(text), "%d", static_cast<int>(lroundf(temperatureC_)));
    }
    canvas.setTextColor(color);
    canvas.setCursor(kPillX + 8, ty);
    canvas.print(text);
    canvas.write(247);
    canvas.print('C');
  }

  if (tSec >= 0.55f) {
    // Humidity: a droplet, then e.g. "66%".
    const int ty = style::openPill(canvas, kPillX, 18, kPillW, tSec - 0.55f);
    const uint16_t drop = style::rgb565(style::kCyan);
    const int ix = kPillX + 4;
    canvas.drawFastVLine(ix, ty, 2, drop);
    canvas.drawFastHLine(ix - 1, ty + 2, 3, drop);
    canvas.fillRect(ix - 2, ty + 3, 5, 3, drop);
    canvas.drawFastHLine(ix - 1, ty + 6, 3, drop);
    char text[8] = "--%";
    if (hasData_) {
      snprintf(text, sizeof(text), "%d%%", humidityPercent_);
    }
    canvas.setTextColor(style::rgb565(style::kWhite));
    canvas.setCursor(kPillX + 8, ty);
    canvas.print(text);
  }
  canvas.unclip();
}
