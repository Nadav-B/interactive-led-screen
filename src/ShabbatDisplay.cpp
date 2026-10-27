#include "ShabbatDisplay.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include "Shabbat.h"
#include "SceneStyle.h"

namespace {
// Layout: candles on the left, greeting top right, challah below it, all
// standing on a tablecloth along the bottom.
constexpr int kClothTop = 29;
constexpr float kCandleX[2] = {4.0f, 11.0f};
constexpr int kCandleTop = 19, kCandleBottom = 25;  // wax body rows
constexpr float kFlameBaseY = 17.2f;
constexpr float kIgniteSec[2] = {0.5f, 1.0f};

constexpr float kChallahX = 40.0f, kChallahY = 25.0f;
constexpr int kKnobs = 7;
constexpr float kKnobSpacing = 4.6f;

constexpr int kGreetingX = 18, kGreetingW = 46;  // the area right of the candles
constexpr float kTypeStartSec = 1.4f, kTypePerCharSec = 0.09f;

constexpr uint8_t kCloth[3] = {196, 192, 212};
constexpr uint8_t kClothShade[3] = {140, 136, 162};
constexpr uint8_t kWax[3] = {206, 200, 186};
constexpr uint8_t kWaxShade[3] = {150, 144, 132};
constexpr uint8_t kFlameRoot[3] = {120, 150, 255};  // the blue at a flame's base
constexpr uint8_t kSilver[3] = {196, 200, 214};
constexpr uint8_t kSilverShade[3] = {124, 128, 148};
constexpr uint8_t kWick[3] = {40, 34, 30};
constexpr uint8_t kCrust[3] = {204, 128, 50};
constexpr uint8_t kCrustLight[3] = {240, 182, 96};
constexpr uint8_t kCrustDark[3] = {128, 70, 28};
constexpr uint8_t kSesame[3] = {250, 236, 200};

uint32_t hash(int x, int y) {
  uint32_t h = static_cast<uint32_t>(x * 374761393 + y * 668265263);
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
}  // namespace

ShabbatDisplay::ShabbatDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                               int width, int height)
    : Scene(matrix, width, height), locationService_(locationService) {}

bool ShabbatDisplay::isActive() {
  struct tm local;
  if (!getLocalTime(&local, 0)) {
    return false;  // Clock not synced: can't tell, so stay out of the rotation.
  }
  struct tm midnight = local;
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  return shabbat::isShabbat(mktime(&local), local.tm_wday, mktime(&midnight),
                            locationService_->latitudeDeg(), locationService_->longitudeDeg());
}

void ShabbatDisplay::render(float tSec) {
  style::drawSky(frame_, tSec, 0);
  drawTablecloth();
  drawChallah(tSec);
  drawCandle(kCandleX[0], kIgniteSec[0], 0.0f, tSec);
  drawCandle(kCandleX[1], kIgniteSec[1], 2.1f, tSec);
  drawGreeting(tSec);
}

void ShabbatDisplay::drawTablecloth() {
  for (int y = kClothTop; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      // A woven texture: every few pixels a slightly darker thread.
      const bool thread = y == kClothTop || ((x + y) % 5 == 0);
      frame_.blend(x, y, thread ? kClothShade : kCloth);
    }
  }
}

void ShabbatDisplay::drawCandle(float cx, float igniteSec, float phase, float tSec) {
  const int x0 = static_cast<int>(cx);

  // Silver candlestick: a foot on the cloth, a short stem, a cup.
  for (int x = x0 - 2; x <= x0 + 2; ++x) frame_.blend(x, kClothTop - 1, kSilverShade);
  for (int x = x0 - 1; x <= x0 + 1; ++x) frame_.blend(x, kClothTop - 2, kSilver);
  frame_.blend(x0, kCandleBottom + 2, kSilver);
  for (int x = x0 - 2; x <= x0 + 2; ++x) {
    frame_.blend(x, kCandleBottom + 1, x == x0 + 2 ? kSilverShade : kSilver);
  }

  // White candle, shaded on its right, with the wick on top.
  for (int y = kCandleTop; y <= kCandleBottom; ++y) {
    frame_.blend(x0 - 1, y, kWax);
    frame_.blend(x0, y, kWax);
    frame_.blend(x0 + 1, y, kWaxShade);
  }
  frame_.blend(x0, kCandleTop - 1, kWick);

  // The flame catches with a little spring, then flickers: it sways, and
  // its height and brightness wobble on two unrelated frequencies.
  const float lit = style::spring((tSec - igniteSec) * 1.4f);
  if (lit <= 0) return;
  const float flicker = 0.85f + 0.1f * sinf(tSec * 11.0f + phase) + 0.05f * sinf(tSec * 23.0f + phase * 2);
  const float sway = 0.45f * sinf(tSec * 3.1f + phase) + 0.2f * sinf(tSec * 7.3f + phase);
  const float height = 6.5f * lit * flicker;
  const float baseX = cx + 0.5f, tipX = baseX + sway, tipY = kFlameBaseY - height;

  // Warm light on everything around it.
  const float glow[3] = {255.0f, 170.0f, 70.0f};
  for (int y = 4; y < height_; ++y) {
    for (int x = x0 - 9; x <= x0 + 9; ++x) {
      const float r = style::circleSdf(x, y, baseX, kFlameBaseY - 1.5f, 0);
      frame_.add(x, y, glow, 0.45f * lit * flicker * expf(-r / 3.6f));
    }
  }

  // A teardrop: round base, tapering to the swaying tip.
  for (int y = static_cast<int>(tipY) - 1; y <= kFlameBaseY + 2; ++y) {
    for (int x = x0 - 2; x <= x0 + 3; ++x) {
      const float base = style::circleSdf(x, y, baseX, kFlameBaseY - 0.8f, 1.9f * lit);
      const float body = style::capsuleSdf(x, y, baseX, kFlameBaseY - 0.8f, tipX, tipY, 0.6f);
      const float cover = style::coverage(fminf(base, body + 0.4f));
      if (cover <= 0) continue;
      // A blue root just above the wick, bright yellow through the body,
      // orange at the tip.
      const float k = style::clamp01((kFlameBaseY - y) / (height + 0.5f));
      const float yellow[3] = {255, 226, 110};
      float c[3];
      if (k < 0.12f) {
        style::lerpRgb(kFlameRoot, style::kGold, k / 0.12f, c);
      } else if (k < 0.5f) {
        float gold[3];
        style::toFloat(style::kGold, gold);
        style::lerpRgb(gold, yellow, (k - 0.12f) / 0.38f, c);
      } else {
        float orange[3];
        style::toFloat(style::kSunEdge, orange);
        style::lerpRgb(yellow, orange, (k - 0.5f) / 0.5f, c);
      }
      frame_.blend(x, y, c, cover);
    }
  }
}

void ShabbatDisplay::drawChallah(float tSec) {
  // Rises onto the table with a little bounce.
  const float pop = style::spring(tSec * 1.1f - 0.2f);
  if (pop <= 0) return;
  const float lift = (1 - fminf(pop, 1.0f)) * 6;
  const float scale = fminf(pop, 1.08f);

  // A braid seen from the side: a row of plump knobs (ellipses tilted
  // alternately left and right), smaller towards the tapered ends. Each
  // pixel takes the knob it's deepest inside; where two knobs meet - the
  // nearest two are about equally deep - it's a dark crease.
  struct Knob {
    float x, y, rx, ry, cosA, sinA;
  };
  Knob knobs[kKnobs];
  const float firstX = kChallahX - kKnobSpacing * (kKnobs - 1) / 2;
  for (int i = 0; i < kKnobs; ++i) {
    const int fromEnd = i < kKnobs - 1 - i ? i : kKnobs - 1 - i;  // 0 at the ends
    const float size = (fromEnd == 0 ? 0.7f : (fromEnd == 1 ? 0.88f : 1.0f)) * scale;
    const float angle = (i % 2 ? 0.45f : -0.45f);
    knobs[i] = {firstX + i * kKnobSpacing, kChallahY + lift + (1 - size) * 2.5f, 3.1f * size,
                3.6f * size, cosf(angle), sinf(angle)};
  }

  const int x0 = static_cast<int>(firstX - 4), x1 = static_cast<int>(firstX + kKnobSpacing * kKnobs + 4);
  for (int y = static_cast<int>(kChallahY - 5); y < kClothTop; ++y) {
    for (int x = x0; x <= x1; ++x) {
      float d1 = 1e9f, d2 = 1e9f;
      int nearest = 0;
      for (int i = 0; i < kKnobs; ++i) {
        const Knob &k = knobs[i];
        const float dx = x - k.x, dy = y - k.y;
        const float u = (dx * k.cosA + dy * k.sinA) / k.rx;
        const float v = (-dx * k.sinA + dy * k.cosA) / k.ry;
        const float d = (sqrtf(u * u + v * v) - 1) * fminf(k.rx, k.ry);
        if (d < d1) {
          d2 = d1;
          d1 = d;
          nearest = i;
        } else if (d < d2) {
          d2 = d;
        }
      }
      const float cover = style::coverage(d1);
      if (cover <= 0) continue;
      const Knob &k = knobs[nearest];
      const float light = style::clamp01(0.65f - ((x - k.x) + (y - k.y)) / (2.2f * k.ry));
      float c[3];
      style::lerpRgb(kCrust, kCrustLight, light, c);
      const bool crease = d2 < 0.5f && d2 - d1 < 0.9f;
      if (crease || d1 > -0.7f) {  // creases and the loaf's outline
        float dark[3];
        style::toFloat(kCrustDark, dark);
        style::lerpRgb(c, dark, crease ? 0.75f : 0.45f, c);
      } else if (y < k.y - 0.5f && (hash(x, y) % 9) == 0) {
        style::toFloat(kSesame, c);  // sesame on the tops of the knobs
      }
      frame_.blend(x, y, c, cover);
    }
  }
}

void ShabbatDisplay::drawGreeting(float tSec) {
  // "Shabbat" over "Shalom", each centred in the space right of the
  // candles, typed in letter by letter with a bright cursor-like flash on
  // the newest one.
  const char *const lines[2] = {"Shabbat", "Shalom"};
  const int lineY[2] = {2, 11};
  style::FrameCanvas canvas(frame_);
  canvas.setTextWrap(false);
  canvas.setTextSize(1);

  const int typed = static_cast<int>((tSec - kTypeStartSec) / kTypePerCharSec);
  int index = 0;
  for (int l = 0; l < 2; ++l) {
    const int len = static_cast<int>(strlen(lines[l]));
    const int x = kGreetingX + (kGreetingW - style::textWidth(len)) / 2;
    for (int c = 0; c < len; ++c, ++index) {
      if (index > typed) return;
      const bool newest = index == typed;
      const uint16_t color = style::rgb565(newest ? style::kSunCore : style::kGold);
      canvas.drawChar(x + c * 6, lineY[l], lines[l][c], color, color, 1);
    }
  }
}
