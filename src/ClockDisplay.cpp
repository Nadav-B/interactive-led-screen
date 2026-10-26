#include "ClockDisplay.h"

#include <math.h>
#include <string.h>

#include "ClockText.h"
#include "SceneStyle.h"

namespace {
constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.nist.gov";

// Layout: Pikachu top left, the time beside him with the seconds bar
// under it, and the date pill along the bottom.
constexpr int kPikaX = 0, kPikaY = 1;
constexpr int kTimeX = 19, kTimeY = 2;
constexpr int kBarX = 19, kBarY = 18, kBarW = 43;
constexpr int kPillY = 21;

// Rounded "seven-segment" digits, kDigitW x kDigitH, drawn as anti-aliased
// capsules: narrower than the 2x GFX font, so Pikachu fits beside them.
constexpr int kDigitW = 8, kDigitH = 14, kDigitGap = 2, kColonW = 4;
constexpr float kSegL = 1.0f, kSegR = 6.0f, kSegT = 1.0f, kSegM = 6.5f, kSegB = 12.0f;
constexpr float kSegRadius = 1.0f;
// Segments a..g as endpoints (x0, y0, x1, y1).
constexpr float kSegments[7][4] = {
    {kSegL, kSegT, kSegR, kSegT},  // a top
    {kSegR, kSegT, kSegR, kSegM},  // b top right
    {kSegR, kSegM, kSegR, kSegB},  // c bottom right
    {kSegL, kSegB, kSegR, kSegB},  // d bottom
    {kSegL, kSegM, kSegL, kSegB},  // e bottom left
    {kSegL, kSegT, kSegL, kSegM},  // f top left
    {kSegL, kSegM, kSegR, kSegM},  // g middle
};
// Bit n set = segment n lit, for 0-9.
constexpr uint8_t kDigitSegments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                        0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// Front-facing Pikachu, 16x19. Y yellow, y its shade, K black, W eye
// highlight, R cheek, '.' transparent.
constexpr int kPikaW = 16, kPikaH = 19;
const char *const kPikachu[kPikaH] = {
    "KK............KK",  //
    "KKY..........YKK",  //
    ".YYY........YYY.",  //
    ".YYYY......YYYY.",  //
    "..YYYYYYYYYYYYy.",  //
    "..YYYYYYYYYYYYy.",  //
    ".YYKWYYYYYYKWYYy",  //
    ".YYKKYYYYYYKKYYy",  //
    ".RRYYYYYYYYYYRRy",  //
    ".RRYYYKYYKYYYRRy",  //
    "..YYYYYKKYYYYYy.",  //
    "...YYYYYYYYYYy..",  //
    "..YYYYYYYYYYYYy.",  //
    ".YYYYYYYYYYYYYYy",  //
    ".Yy.YYYYYYYYYyYy",  //
    ".YYYYYYYYYYYYYYy",  //
    "..YYYYYYYYYYYYy.",  //
    "..YYYyYYYYyYYYy.",  //
    "...YY......YY...",  //
};
constexpr uint8_t kPikaYellow[3] = {255, 214, 40};
constexpr uint8_t kPikaShade[3] = {222, 160, 22};
constexpr uint8_t kPikaBlack[3] = {22, 16, 12};
constexpr uint8_t kPikaWhite[3] = {255, 255, 255};
constexpr uint8_t kPikaCheek[3] = {236, 58, 48};
constexpr float kBlinkEverySec = 3.2f, kBlinkSec = 0.14f;
constexpr float kSparkEverySec = 1.7f, kSparkSec = 0.22f;

float capsuleSdf(float px, float py, const float s[4]) {
  const float pax = px - s[0], pay = py - s[1], bax = s[2] - s[0], bay = s[3] - s[1];
  const float h = style::clamp01((pax * bax + pay * bay) / (bax * bax + bay * bay));
  const float dx = pax - bax * h, dy = pay - bay * h;
  return sqrtf(dx * dx + dy * dy) - kSegRadius;
}

void drawDigit(uint8_t *frame, int width, int x, int y, int digit, int clipBottom,
               const float rgb[3]) {
  const uint8_t mask = digit >= 0 && digit <= 9 ? kDigitSegments[digit] : 0x40;  // '-'
  for (int py = 0; py < kDigitH; ++py) {
    if (y + py < 0 || y + py >= clipBottom) continue;
    for (int px = 0; px < kDigitW; ++px) {
      float d = 1e6f;
      for (int s = 0; s < 7; ++s) {
        if (mask & (1 << s)) d = fminf(d, capsuleSdf(px, py, kSegments[s]));
      }
      const float cover = style::clamp01(0.5f - d);
      if (cover > 0) style::blendRgb(frame + ((y + py) * width + x + px) * 3, rgb, cover);
    }
  }
}

void drawPikachu(uint8_t *frame, int width, int height, int x0, int y0, float tSec) {
  const bool blink = fmodf(tSec, kBlinkEverySec) > kBlinkEverySec - kBlinkSec;
  for (int row = 0; row < kPikaH; ++row) {
    const int y = y0 + row;
    if (y < 0 || y >= height) continue;
    for (int col = 0; col < kPikaW; ++col) {
      const int x = x0 + col;
      if (x < 0 || x >= width) continue;
      char c = kPikachu[row][col];
      if (blink && row == 6 && (c == 'K' || c == 'W')) c = 'Y';  // lids down
      const uint8_t *rgb = nullptr;
      switch (c) {
        case 'Y': rgb = kPikaYellow; break;
        case 'y': rgb = kPikaShade; break;
        case 'K': rgb = kPikaBlack; break;
        case 'W': rgb = kPikaWhite; break;
        case 'R': rgb = kPikaCheek; break;
        default: break;
      }
      if (!rgb) continue;
      uint8_t *p = frame + (y * width + x) * 3;
      p[0] = rgb[0];
      p[1] = rgb[1];
      p[2] = rgb[2];
    }
  }

  // Cheek sparks: a quick electric crackle either side every so often.
  const float since = fmodf(tSec, kSparkEverySec);
  if (since < kSparkSec && tSec > 0.8f) {
    const float k = 1 - since / kSparkSec;
    const float spark[3] = {255, 240, 120};
    const int sparks[][2] = {{-1, 7}, {0, 6}, {-1, 10}, {16, 7}, {17, 6}, {16, 10}, {17, 11}};
    for (const auto &sp : sparks) {
      const int x = x0 + sp[0], y = y0 + sp[1];
      // Flicker: each spark pixel only on some frames.
      if (((static_cast<int>(tSec * 30) + sp[0] + sp[1]) & 1) == 0) continue;
      if (x < 0 || x >= width || y < 0 || y >= height) continue;
      style::addRgb(frame + (y * width + x) * 3, spark, k);
    }
  }
}

constexpr uint8_t kBarTrack[3] = {18, 36, 70};
constexpr float kTimeRevealSec = 0.1f;
constexpr float kPillRevealSec = 0.45f;
}  // namespace

ClockDisplay::ClockDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                           int width, int height)
    : matrix_(matrix),
      locationService_(locationService),
      width_(width),
      height_(height),
      frame_(new uint8_t[width * height * 3]) {}

ClockDisplay::~ClockDisplay() { delete[] frame_; }

void ClockDisplay::begin() { applyTimezone(); }

void ClockDisplay::applyTimezone() {
  configTzTime(locationService_->posixTimezone().c_str(), kNtpServer1, kNtpServer2);
}

void ClockDisplay::show() {
  startMs_ = millis();
  lastFrameMs_ = 0;
}

void ClockDisplay::update() {
  const unsigned long now = millis();
  if (lastFrameMs_ != 0 && now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  lastFrameMs_ = now;
  render((now - startMs_) / 1000.0f);
  style::blit(matrix_, frame_, width_, height_);
}

void ClockDisplay::render(float tSec) {
  style::drawSky(frame_, width_, height_, tSec, 0);

  struct tm timeinfo;
  const bool synced = getLocalTime(&timeinfo, 0);
  float secFrac = 0;
  if (synced) {
    if (timeinfo.tm_sec != lastSecond_) {
      lastSecond_ = timeinfo.tm_sec;
      secondStartMs_ = millis();
    }
    secFrac = style::clamp01((millis() - secondStartMs_) / 1000.0f);
  }

  style::FrameCanvas canvas(frame_, width_, height_);
  canvas.setTextWrap(false);

  // Pikachu hops in on a spring, then bobs gently.
  const float hop = (1 - style::spring(tSec * 0.9f)) * 22;
  const int bob = (static_cast<int>(tSec / 0.6f) & 1) ? 1 : 0;
  drawPikachu(frame_, width_, height_, kPikaX, kPikaY + static_cast<int>(lroundf(hop)) + bob,
              tSec);

  // Time, rising in through a clip above the seconds bar.
  if (tSec >= kTimeRevealSec) {
    const float p = style::easeOutCubic((tSec - kTimeRevealSec) / 0.5f);
    const int ty = kTimeY + static_cast<int>(lroundf((1 - p) * 16));
    const float digits[3] = {static_cast<float>(style::kSunCore[0]),
                             static_cast<float>(style::kSunCore[1]),
                             static_cast<float>(style::kSunCore[2])};
    const int d[4] = {synced ? timeinfo.tm_hour / 10 : -1, synced ? timeinfo.tm_hour % 10 : -1,
                      synced ? timeinfo.tm_min / 10 : -1, synced ? timeinfo.tm_min % 10 : -1};
    int x = kTimeX;
    for (int i = 0; i < 4; ++i) {
      drawDigit(frame_, width_, x, ty, d[i], kBarY - 1, digits);
      x += kDigitW + kDigitGap;
      if (i == 1) {
        // Colon breathes once a second, gold: two round dots.
        const float glow = 0.6f + 0.4f * (0.5f + 0.5f * cosf(secFrac * style::kTwoPi));
        const float colon[3] = {style::kGold[0] * glow, style::kGold[1] * glow,
                                style::kGold[2] * glow};
        const int cx = x;
        for (int dy : {4, 9}) {
          for (int yy = 0; yy < 2; ++yy) {
            for (int xx = 0; xx < 2; ++xx) {
              const int py = ty + dy + yy;
              if (py < 0 || py >= kBarY - 1) continue;
              style::blendRgb(frame_ + (py * width_ + cx + xx) * 3, colon, 1);
            }
          }
        }
        x += kColonW;
      }
    }
  }

  // Seconds bar: grows across the minute, gold into coral, bright head.
  const float intro = style::easeOutCubic(tSec / 0.6f);
  for (int i = 0; i < kBarW; ++i) {
    uint8_t *px = frame_ + (kBarY * width_ + kBarX + i) * 3;
    const float track[3] = {static_cast<float>(kBarTrack[0]), static_cast<float>(kBarTrack[1]),
                            static_cast<float>(kBarTrack[2])};
    style::blendRgb(px, track, intro);
  }
  if (synced) {
    const float filled = kBarW * (timeinfo.tm_sec + secFrac) / 60.0f * intro;
    for (int i = 0; i < kBarW && i < filled; ++i) {
      float c[3];
      style::lerpRgb(style::kGold, style::kSunEdge, i / float(kBarW), c);
      const float edge = style::clamp01(filled - i);  // anti-aliased tip
      style::blendRgb(frame_ + (kBarY * width_ + kBarX + i) * 3, c, edge);
    }
    const int head = static_cast<int>(filled);
    if (head < kBarW) {
      const float headRgb[3] = {255, 247, 220};
      style::addRgb(frame_ + (kBarY * width_ + kBarX + head) * 3, headRgb, 0.6f);
    }
  }

  // Date pill: weekday in gold, day and month in white, centred.
  if (tSec >= kPillRevealSec) {
    const int ty = style::openPill(canvas, 0, kPillY, width_, tSec - kPillRevealSec);
    canvas.setTextSize(1);
    char date[16] = "syncing";
    if (synced) clockface::formatDate(timeinfo, date, sizeof(date));
    const int len = static_cast<int>(strlen(date));
    canvas.setCursor((width_ - (len * 6 - 1)) / 2, ty);
    const char *rest = synced ? date + 3 : date;
    if (synced) {
      canvas.setTextColor(style::rgb565(style::kGold));
      canvas.print(String(date).substring(0, 3));
    }
    canvas.setTextColor(style::rgb565(style::kWhite));
    canvas.print(rest);
    canvas.unclip();
  }
}
