#include "SunMoonDisplay.h"

#include <math.h>

#include "SceneStyle.h"

namespace {
// Sun page: disc cut off by the left edge. Moon page: one panel width on.
constexpr float kSunX = -1.0f, kSunY = 16.0f, kSunR = 11.0f;
constexpr float kMoonX = 64.0f + 52.0f, kMoonY = 16.0f, kMoonR = 9.0f;
constexpr int kPillW = 38;
}  // namespace

SunMoonDisplay::SunMoonDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                               int width, int height)
    : matrix_(matrix),
      locationService_(locationService),
      width_(width),
      height_(height),
      frame_(new uint8_t[width * height * 3]) {}

SunMoonDisplay::~SunMoonDisplay() { delete[] frame_; }

void SunMoonDisplay::refreshEvents() {
  struct tm now;
  if (!getLocalTime(&now, 0)) {
    return;  // NTP not synced yet; begin() retries next time round.
  }
  if (hasEvents_ && now.tm_year == eventsYear_ && now.tm_yday == eventsYday_) {
    return;
  }
  struct tm midnight = now;
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  const time_t dayStart = mktime(&midnight);
  const double lat = atof(locationService_->latitude().c_str());
  const double lon = atof(locationService_->longitude().c_str());

  sun_ = sky::sunRiseSet(dayStart, lat, lon);
  moon_ = sky::moonRiseSet(dayStart, lat, lon);
  phase_ = sky::moonPhase(time(nullptr));
  hasEvents_ = true;
  eventsYear_ = now.tm_year;
  eventsYday_ = now.tm_yday;
  Serial.printf("[SunMoonDisplay] sun %ld-%ld moon %ld-%ld, phase %.2f (%.0f%% lit)\n",
                static_cast<long>(sun_.rise), static_cast<long>(sun_.set),
                static_cast<long>(moon_.rise), static_cast<long>(moon_.set), phase_.age,
                phase_.illumination * 100);
}

void SunMoonDisplay::begin() {
  refreshEvents();
  startMs_ = millis();
  lastFrameMs_ = 0;
}

void SunMoonDisplay::update() {
  const unsigned long now = millis();
  if (lastFrameMs_ != 0 && now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  lastFrameMs_ = now;
  render((now - startMs_) / 1000.0f);
  style::blit(matrix_, frame_, width_, height_);
}

void SunMoonDisplay::render(float tSec) {
  const float panT = (tSec * 1000.0f - kPanStartMs) / kPanDurationMs;
  const float camX = width_ * style::easeInOutCubic(panT);

  style::drawSky(frame_, width_, height_, tSec, camX);
  drawSun(tSec, camX);
  drawMoon(tSec, camX);

  const float moonRevealSec = (kPanStartMs + kPanDurationMs) / 1000.0f;
  drawPill(25, 3, camX, 0.35f, tSec, true, sun_.rise, hasEvents_ && sun_.hasRise, style::kGold);
  drawPill(25, 18, camX, 0.55f, tSec, false, sun_.set, hasEvents_ && sun_.hasSet,
           style::kSunEdge);
  drawPill(64 + 1, 3, camX, moonRevealSec, tSec, true, moon_.rise, hasEvents_ && moon_.hasRise,
           style::kWhite);
  drawPill(64 + 1, 18, camX, moonRevealSec + 0.2f, tSec, false, moon_.set,
           hasEvents_ && moon_.hasSet, style::kCyan);
}

void SunMoonDisplay::drawSun(float tSec, float camX) {
  const float cx = kSunX - camX;
  const float r = kSunR * style::spring(tSec);
  if (cx + r * 3.4f < 0) return;  // panned away
  style::drawSun(frame_, width_, height_, cx, kSunY, r, tSec);
}

void SunMoonDisplay::drawMoon(float tSec, float camX) {
  const float cx = kMoonX - camX;
  // Pops in as the pan lands.
  const float popSec = tSec - (kPanStartMs + kPanDurationMs * 0.55f) / 1000.0f;
  const float r = kMoonR * style::spring(popSec);
  if (cx - r * 3 > width_) return;
  style::drawMoon(frame_, width_, height_, cx, kMoonY, r, static_cast<float>(phase_.age),
                  static_cast<float>(phase_.illumination));
}

void SunMoonDisplay::drawPill(int canvasX, int y, float camX, float revealSec, float tSec,
                              bool up, time_t when, bool has, const uint8_t rgb[3]) {
  const int x = canvasX - static_cast<int>(lroundf(camX));
  if (tSec < revealSec || x + kPillW <= 0 || x >= width_) return;

  style::FrameCanvas canvas(frame_, width_, height_);
  const int ty = style::openPill(canvas, x, y, kPillW, tSec - revealSec);
  const uint16_t color = style::rgb565(rgb);

  // Arrow: up for rise, down for set.
  const int ax = x + 4;
  canvas.drawFastVLine(ax, ty, 7, color);
  const int tip = up ? ty : ty + 6;
  const int dir = up ? 1 : -1;
  canvas.drawPixel(ax - 1, tip + dir, color);
  canvas.drawPixel(ax + 1, tip + dir, color);
  canvas.drawPixel(ax - 2, tip + 2 * dir, color);
  canvas.drawPixel(ax + 2, tip + 2 * dir, color);

  char text[6] = "--:--";
  if (has) {
    struct tm local;
    localtime_r(&when, &local);
    snprintf(text, sizeof(text), "%02d:%02d", local.tm_hour, local.tm_min);
  }
  canvas.setTextWrap(false);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  canvas.setCursor(x + 8, ty);
  canvas.print(text);
}
