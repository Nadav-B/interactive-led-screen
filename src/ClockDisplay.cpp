#include "ClockDisplay.h"

#include <math.h>
#include <string.h>

#include "ClockText.h"

namespace {
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

void drawDigit(const style::Frame &frame, int x, int y, int digit, int clipBottom,
               const uint8_t rgb[3]) {
  const uint8_t mask = digit >= 0 && digit <= 9 ? kDigitSegments[digit] : 0x40;  // '-'
  for (int py = 0; py < kDigitH; ++py) {
    if (y + py < 0 || y + py >= clipBottom) continue;
    for (int px = 0; px < kDigitW; ++px) {
      float d = 1e6f;
      for (int s = 0; s < 7; ++s) {
        if (!(mask & (1 << s))) continue;
        const float *seg = kSegments[s];
        d = fminf(d, style::capsuleSdf(px, py, seg[0], seg[1], seg[2], seg[3], kSegRadius));
      }
      const float cover = style::coverage(d);
      if (cover > 0) frame.blend(x + px, y + py, rgb, cover);
    }
  }
}

void drawPikachu(const style::Frame &frame, int x0, int y0, float tSec) {
  const bool blink = fmodf(tSec, kBlinkEverySec) > kBlinkEverySec - kBlinkSec;
  for (int row = 0; row < kPikaH; ++row) {
    for (int col = 0; col < kPikaW; ++col) {
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
      if (rgb) frame.blend(x0 + col, y0 + row, rgb);
    }
  }

  // Cheek sparks: a quick electric crackle either side every so often.
  const float since = fmodf(tSec, kSparkEverySec);
  if (since < kSparkSec && tSec > 0.8f) {
    const float k = 1 - since / kSparkSec;
    const float spark[3] = {255, 240, 120};
    const int sparks[][2] = {{-1, 7}, {0, 6}, {-1, 10}, {16, 7}, {17, 6}, {16, 10}, {17, 11}};
    for (const auto &sp : sparks) {
      // Flicker: each spark pixel only on some frames.
      if (((static_cast<int>(tSec * 30) + sp[0] + sp[1]) & 1) == 0) continue;
      frame.add(x0 + sp[0], y0 + sp[1], spark, k);
    }
  }
}

constexpr uint8_t kBarTrack[3] = {18, 36, 70};
constexpr float kTimeRevealSec = 0.1f;
constexpr float kPillRevealSec = 0.45f;
}  // namespace

ClockDisplay::ClockDisplay(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : Scene(matrix, width, height) {}

void ClockDisplay::render(float tSec) {
  style::drawSky(frame_, tSec, 0);

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

  style::FrameCanvas canvas(frame_);
  canvas.setTextWrap(false);

  // Pikachu hops in on a spring, then bobs gently.
  const float hop = (1 - style::spring(tSec * 0.9f)) * 22;
  const int bob = (static_cast<int>(tSec / 0.6f) & 1) ? 1 : 0;
  drawPikachu(frame_, kPikaX, kPikaY + static_cast<int>(lroundf(hop)) + bob, tSec);

  // Time, rising in through a clip above the seconds bar.
  if (tSec >= kTimeRevealSec) {
    const float p = style::easeOutCubic((tSec - kTimeRevealSec) / 0.5f);
    const int ty = kTimeY + static_cast<int>(lroundf((1 - p) * 16));
    const int d[4] = {synced ? timeinfo.tm_hour / 10 : -1, synced ? timeinfo.tm_hour % 10 : -1,
                      synced ? timeinfo.tm_min / 10 : -1, synced ? timeinfo.tm_min % 10 : -1};
    int x = kTimeX;
    for (int i = 0; i < 4; ++i) {
      drawDigit(frame_, x, ty, d[i], kBarY - 1, style::kSunCore);
      x += kDigitW + kDigitGap;
      if (i == 1) {
        // Colon breathes once a second, gold: two round dots.
        const float glow = 0.6f + 0.4f * (0.5f + 0.5f * cosf(secFrac * style::kTwoPi));
        const float colon[3] = {style::kGold[0] * glow, style::kGold[1] * glow,
                                style::kGold[2] * glow};
        for (int dy : {4, 9}) {
          for (int yy = 0; yy < 2; ++yy) {
            const int py = ty + dy + yy;
            if (py >= kBarY - 1) continue;  // still rising in under the clip
            frame_.blend(x, py, colon);
            frame_.blend(x + 1, py, colon);
          }
        }
        x += kColonW;
      }
    }
  }

  // Seconds bar: grows across the minute, gold into coral, bright head.
  const float intro = style::easeOutCubic(tSec / 0.6f);
  for (int i = 0; i < kBarW; ++i) frame_.blend(kBarX + i, kBarY, kBarTrack, intro);
  if (synced) {
    const float filled = kBarW * (timeinfo.tm_sec + secFrac) / 60.0f * intro;
    for (int i = 0; i < kBarW && i < filled; ++i) {
      float c[3];
      style::lerpRgb(style::kGold, style::kSunEdge, i / float(kBarW), c);
      const float edge = style::clamp01(filled - i);  // anti-aliased tip
      frame_.blend(kBarX + i, kBarY, c, edge);
    }
    const int head = static_cast<int>(filled);
    if (head < kBarW) frame_.add(kBarX + head, kBarY, style::kSunCore, 0.6f);
  }

  // Date pill: weekday in gold, day and month in white, centred.
  if (tSec >= kPillRevealSec) {
    const int ty = style::openPill(canvas, 0, kPillY, width_, tSec - kPillRevealSec);
    canvas.setTextSize(1);
    char date[16] = "syncing";
    if (synced) clockface::formatDate(timeinfo, date, sizeof(date));
    const int len = static_cast<int>(strlen(date));
    canvas.setCursor((width_ - (len * 6 - 1)) / 2, ty);
    const char *rest = date;
    if (synced) {  // weekday in gold
      canvas.setTextColor(style::rgb565(style::kGold));
      canvas.write(reinterpret_cast<const uint8_t *>(date), 3);
      rest = date + 3;
    }
    canvas.setTextColor(style::rgb565(style::kWhite));
    canvas.print(rest);
    canvas.unclip();
  }
}
