#include "SunMoonDisplay.h"

#include <math.h>
#include <string.h>

#include "ClockText.h"

namespace {
// Sun page: disc cut off by the left edge. Daylight page: one panel width
// on, the dial in its left third, pills where the sun page's are.
constexpr float kSunX = -1.0f, kSunY = 16.0f, kSunR = 11.0f;
constexpr float kDialX = 64.0f + 12.0f, kDialY = 16.0f;
constexpr float kDialR = 8.5f, kDialThickness = 1.6f;  // ring centre radius, half-width
constexpr int kPillX = 25, kPillW = 38;

constexpr uint8_t kNightArc[3] = {34, 52, 104};
constexpr uint8_t kHand[3] = {225, 235, 255};

float dayFraction(time_t t) {
  struct tm local;
  localtime_r(&t, &local);
  return (local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec) / 86400.0f;
}

time_t localMidnight(const struct tm &day, int dayOffset) {
  struct tm midnight = day;
  midnight.tm_mday += dayOffset;  // mktime normalises across month ends
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  return mktime(&midnight);
}
}  // namespace

SunMoonDisplay::SunMoonDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                               int width, int height)
    : Scene(matrix, width, height), locationService_(locationService) {}

void SunMoonDisplay::onEnter() {
  struct tm now;
  if (!getLocalTime(&now, 0)) {
    return;  // NTP not synced yet; retried on the next visit.
  }
  nowFrac_ = (now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec) / 86400.0f;
  if (now.tm_year == eventsYear_ && now.tm_yday == eventsYday_) {
    return;
  }
  const double lat = locationService_->latitudeDeg();
  const double lon = locationService_->longitudeDeg();

  sun_ = sky::sunRiseSet(localMidnight(now, 0), lat, lon);
  const sky::RiseSet yesterday = sky::sunRiseSet(localMidnight(now, -1), lat, lon);
  hasDayLength_ = sun_.hasRise && sun_.hasSet && yesterday.hasRise && yesterday.hasSet;
  if (hasDayLength_) {
    dayLengthSec_ = static_cast<long>(sun_.set - sun_.rise);
    dayLengthDeltaSec_ = dayLengthSec_ - static_cast<long>(yesterday.set - yesterday.rise);
    riseFrac_ = dayFraction(sun_.rise);
    setFrac_ = dayFraction(sun_.set);
  }
  eventsYear_ = now.tm_year;
  eventsYday_ = now.tm_yday;
  Serial.printf("[SunMoonDisplay] sun %ld-%ld, daylight %lds (%+lds since yesterday)\n",
                static_cast<long>(sun_.rise), static_cast<long>(sun_.set), dayLengthSec_,
                dayLengthDeltaSec_);
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

  // The daylight page reveals as the pan lands.
  const float dayRevealSec = (kPanStartMs + kPanDurationMs * 0.55f) / 1000.0f;
  const float dialX = kDialX - camX;
  if (dialX - kDialR - 4 <= width_) {
    drawDial(dialX, kDialY, tSec, dayRevealSec);
  }

  // No events yet (clock not synced) leaves every has* false: "--:--".
  char rise[6] = "--:--", set[6] = "--:--";
  struct tm local;
  if (sun_.hasRise) {
    localtime_r(&sun_.rise, &local);
    clockface::formatHm(local, rise, sizeof(rise));
  }
  if (sun_.hasSet) {
    localtime_r(&sun_.set, &local);
    clockface::formatHm(local, set, sizeof(set));
  }
  drawPill(kPillX, 3, camX, 0.35f, tSec, Icon::RiseArrow, rise, style::kGold);
  drawPill(kPillX, 18, camX, 0.55f, tSec, Icon::SetArrow, set, style::kSunEdge);

  // Day length, then its change since yesterday: an up-arrow in gold once
  // the days are lengthening, a down-arrow in cyan while they shorten.
  char length[7] = "--h--", delta[7] = "--";
  if (hasDayLength_) {
    clockface::formatDuration(dayLengthSec_, length, sizeof(length));
    clockface::formatDelta(dayLengthDeltaSec_, delta, sizeof(delta));
  }
  const bool gaining = dayLengthDeltaSec_ >= 0;
  const float pageRevealSec = (kPanStartMs + kPanDurationMs) / 1000.0f;
  drawPill(64 + kPillX, 3, camX, pageRevealSec, tSec, Icon::Sun, length, style::kGold);
  drawPill(64 + kPillX, 18, camX, pageRevealSec + 0.2f, tSec,
           gaining ? Icon::RiseArrow : Icon::SetArrow, delta,
           gaining ? style::kGold : style::kCyan);
}

void SunMoonDisplay::drawDial(float cx, float cy, float tSec, float revealSec) {
  const float since = tSec - revealSec;
  if (since <= 0) return;
  const float ring = style::easeOutBack(since / 0.5f);           // ring pops in
  const float sweep = style::easeInOutCubic((since - 0.3f) / 0.9f);  // then the day arc
  const float radius = kDialR * ring;

  // Midnight at the bottom, noon at the top, going clockwise - so the
  // daylight arc sits over the top like the sun's path.
  const float dayEnd = riseFrac_ + (setFrac_ - riseFrac_) * sweep;
  const float glow[3] = {255.0f, 170.0f, 80.0f};
  const int x0 = static_cast<int>(cx - kDialR - 3), x1 = static_cast<int>(cx + kDialR + 3);
  for (int y = static_cast<int>(cy - kDialR - 3); y <= cy + kDialR + 3; ++y) {
    for (int x = x0; x <= x1; ++x) {
      if (!frame_.contains(x, y)) continue;
      const float dx = x - cx, dy = y - cy;
      const float r = sqrtf(dx * dx + dy * dy);
      float f = atan2f(-dx, dy) / style::kTwoPi;
      if (f < 0) f += 1;
      const bool day = hasDayLength_ && sweep > 0 && f >= riseFrac_ && f <= dayEnd;

      // Soft glow outside the daylight arc.
      if (day && r > radius) {
        frame_.add(x, y, glow, 0.25f * expf(-(r - radius - kDialThickness) / 1.5f));
      }
      const float cover = style::coverage(fabsf(r - radius) - kDialThickness);
      if (cover <= 0) continue;
      float c[3];
      if (day) {
        // Gold through the morning, white-hot at noon, orange by sunset.
        const float k = (f - riseFrac_) / (setFrac_ - riseFrac_);
        if (k < 0.5f) {
          style::lerpRgb(style::kGold, style::kSunCore, k * 2, c);
        } else {
          style::lerpRgb(style::kSunCore, style::kSunEdge, (k - 0.5f) * 2, c);
        }
      } else {
        style::toFloat(kNightArc, c);
      }
      frame_.blend(x, y, c, cover);
    }
  }

  // The hand: from the centre to the ring at the current time, with a dot
  // on the ring that pulses gently.
  if (sweep <= 0) return;
  const float a = nowFrac_ * style::kTwoPi;
  const float hx = -sinf(a), hy = cosf(a);  // inverse of the atan2 mapping above
  for (float s = 0; s <= radius - 2.5f; s += 0.5f) {
    frame_.blend(static_cast<int>(lroundf(cx + hx * s)), static_cast<int>(lroundf(cy + hy * s)),
                 kHand, 0.8f * sweep);
  }
  const float pulse = 0.75f + 0.25f * sinf(tSec * 4.0f);
  const int dotX = static_cast<int>(lroundf(cx + hx * radius));
  const int dotY = static_cast<int>(lroundf(cy + hy * radius));
  const float white[3] = {255, 255, 255};
  frame_.blend(dotX, dotY, white, sweep);
  frame_.add(dotX - 1, dotY, white, 0.35f * pulse * sweep);
  frame_.add(dotX + 1, dotY, white, 0.35f * pulse * sweep);
  frame_.add(dotX, dotY - 1, white, 0.35f * pulse * sweep);
  frame_.add(dotX, dotY + 1, white, 0.35f * pulse * sweep);
}

void SunMoonDisplay::drawPill(int canvasX, int y, float camX, float revealSec, float tSec,
                              Icon icon, const char *text, const uint8_t rgb[3]) {
  const int x = canvasX - static_cast<int>(lroundf(camX));
  if (tSec < revealSec || x + kPillW <= 0 || x >= width_) return;

  style::FrameCanvas canvas(frame_);
  const int ty = style::openPill(canvas, x, y, kPillW, tSec - revealSec);
  const uint16_t color = style::rgb565(rgb);

  const int ix = x + 4;
  if (icon == Icon::Sun) {
    // A sun on the horizon: half disc on a line, three short rays.
    canvas.drawFastHLine(ix - 1, ty + 3, 3, color);
    canvas.drawFastHLine(ix - 2, ty + 4, 5, color);
    canvas.drawFastHLine(ix - 2, ty + 5, 5, color);
    canvas.drawFastHLine(ix - 3, ty + 6, 7, color);
    canvas.drawPixel(ix, ty + 1, color);
    canvas.drawPixel(ix - 3, ty + 2, color);
    canvas.drawPixel(ix + 3, ty + 2, color);
  } else {
    // Arrow: up for rise (and lengthening days), down for set.
    const bool up = icon == Icon::RiseArrow;
    canvas.drawFastVLine(ix, ty, 7, color);
    const int tip = up ? ty : ty + 6;
    const int dir = up ? 1 : -1;
    canvas.drawPixel(ix - 1, tip + dir, color);
    canvas.drawPixel(ix + 1, tip + dir, color);
    canvas.drawPixel(ix - 2, tip + 2 * dir, color);
    canvas.drawPixel(ix + 2, tip + 2 * dir, color);
  }

  // Colons are drawn as a narrow 2px column rather than a full 6px
  // glyph, so "06:53" leaves room for the gap after the icon.
  int width = 0;
  for (const char *c = text; *c; ++c) width += *c == ':' ? 3 : 6;
  canvas.setTextWrap(false);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  int cx = style::pillTextX(x, kPillW, width - 1);
  for (const char *c = text; *c; ++c) {
    if (*c == ':') {
      canvas.drawPixel(cx, ty + 2, color);
      canvas.drawPixel(cx, ty + 4, color);
      cx += 3;
    } else {
      canvas.drawChar(cx, ty, *c, color, color, 1);
      cx += 6;
    }
  }
}
