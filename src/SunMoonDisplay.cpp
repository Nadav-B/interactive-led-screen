#include "SunMoonDisplay.h"

#include <math.h>

#include "ClockText.h"

namespace {
// Sun page: disc cut off by the left edge. Moon page: one panel width on.
constexpr float kSunX = -1.0f, kSunY = 16.0f, kSunR = 11.0f;
constexpr float kMoonX = 64.0f + 52.0f, kMoonY = 16.0f, kMoonR = 9.0f;
constexpr int kPillW = 38;
}  // namespace

SunMoonDisplay::SunMoonDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                               int width, int height)
    : Scene(matrix, width, height), locationService_(locationService) {}

void SunMoonDisplay::onEnter() {
  struct tm now;
  if (!getLocalTime(&now, 0)) {
    return;  // NTP not synced yet; retried on the next visit.
  }
  if (now.tm_year == eventsYear_ && now.tm_yday == eventsYday_) {
    return;
  }
  struct tm midnight = now;
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  const time_t dayStart = mktime(&midnight);
  const double lat = locationService_->latitudeDeg();
  const double lon = locationService_->longitudeDeg();

  sun_ = sky::sunRiseSet(dayStart, lat, lon);
  moon_ = sky::moonRiseSet(dayStart, lat, lon);
  phase_ = sky::moonPhase(mktime(&now));
  eventsYear_ = now.tm_year;
  eventsYday_ = now.tm_yday;
  Serial.printf("[SunMoonDisplay] sun %ld-%ld moon %ld-%ld, phase %.2f (%.0f%% lit)\n",
                static_cast<long>(sun_.rise), static_cast<long>(sun_.set),
                static_cast<long>(moon_.rise), static_cast<long>(moon_.set), phase_.age,
                phase_.illumination * 100);
}

void SunMoonDisplay::render(float tSec) {
  const float panT = (tSec * 1000.0f - kPanStartMs) / kPanDurationMs;
  const float camX = width_ * style::easeInOutCubic(panT);

  style::drawSky(frame_, tSec, camX);

  const float sunX = kSunX - camX;
  const float sunR = kSunR * style::spring(tSec);
  if (sunX + sunR * 3.4f >= 0) {  // until panned away
    style::drawSun(frame_, sunX, kSunY, sunR, tSec);
  }

  // The moon pops in as the pan lands.
  const float moonX = kMoonX - camX;
  const float moonPopSec = tSec - (kPanStartMs + kPanDurationMs * 0.55f) / 1000.0f;
  const float moonR = kMoonR * style::spring(moonPopSec);
  if (moonX - moonR * 3 <= width_) {
    style::drawMoon(frame_, moonX, kMoonY, moonR, static_cast<float>(phase_.age),
                    static_cast<float>(phase_.illumination));
  }

  // No events yet (clock not synced) leaves every has* false: "--:--".
  const float moonRevealSec = (kPanStartMs + kPanDurationMs) / 1000.0f;
  drawPill(25, 3, camX, 0.35f, tSec, true, sun_.rise, sun_.hasRise, style::kGold);
  drawPill(25, 18, camX, 0.55f, tSec, false, sun_.set, sun_.hasSet, style::kSunEdge);
  drawPill(64 + 1, 3, camX, moonRevealSec, tSec, true, moon_.rise, moon_.hasRise,
           style::kWhite);
  drawPill(64 + 1, 18, camX, moonRevealSec + 0.2f, tSec, false, moon_.set, moon_.hasSet,
           style::kCyan);
}

void SunMoonDisplay::drawPill(int canvasX, int y, float camX, float revealSec, float tSec,
                              bool up, time_t when, bool has, const uint8_t rgb[3]) {
  const int x = canvasX - static_cast<int>(lroundf(camX));
  if (tSec < revealSec || x + kPillW <= 0 || x >= width_) return;

  style::FrameCanvas canvas(frame_);
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
    clockface::formatHm(local, text, sizeof(text));
  }
  canvas.setTextWrap(false);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  canvas.setCursor(x + 8, ty);
  canvas.print(text);
}
