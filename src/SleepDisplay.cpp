#include "SleepDisplay.h"

#include <math.h>
#include <time.h>

#include "ClockText.h"
#include "SceneStyle.h"

namespace {
// The rat, curled up facing left on the left half; the time on the right.
constexpr float kBodyX = 15.0f, kBodyY = 22.0f, kBodyRx = 8.5f, kBodyRy = 5.0f;
constexpr float kHeadX = 7.0f, kHeadY = 23.5f, kHeadRx = 4.2f, kHeadRy = 3.4f;
constexpr float kBreathSec = 4.0f;  // one slow breath

constexpr float kZEverySec = 1.6f, kZRiseSec = 4.8f;
constexpr int kTimeX = 30, kTimeW = 34, kTimeY = 12;

// Colours are moderate rather than dim: the whole panel runs at low
// brightness at night.
constexpr uint8_t kFur[3] = {150, 138, 128};
constexpr uint8_t kFurLight[3] = {196, 186, 176};
constexpr uint8_t kFurShade[3] = {92, 84, 80};
constexpr uint8_t kPink[3] = {220, 130, 140};
constexpr uint8_t kEyeLine[3] = {40, 30, 34};
constexpr uint8_t kCushion[3] = {30, 44, 96};
constexpr uint8_t kCushionLight[3] = {48, 66, 130};
constexpr uint8_t kZ[3] = {120, 170, 255};
constexpr uint8_t kTime[3] = {90, 120, 190};

float ellipseSdf(float px, float py, float cx, float cy, float rx, float ry) {
  const float u = (px - cx) / rx, v = (py - cy) / ry;
  return (sqrtf(u * u + v * v) - 1) * fminf(rx, ry);
}
}  // namespace

SleepDisplay::SleepDisplay(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : Scene(matrix, width, height) {}

void SleepDisplay::render(float tSec) {
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      uint8_t *p = frame_.at(x, y);
      p[0] = p[1] = p[2] = 0;
    }
  }
  drawCushion();
  drawRat(tSec);
  drawZs(tSec);
  drawTime();
}

void SleepDisplay::drawCushion() {
  for (int y = 24; y < height_; ++y) {
    for (int x = 0; x < 30; ++x) {
      const float cover = style::coverage(ellipseSdf(x, y, 15, 29, 15, 3.5f));
      if (cover <= 0) continue;
      float c[3];
      style::lerpRgb(kCushionLight, kCushion, (y - 26) / 4.0f, c);
      frame_.blend(x, y, c, cover);
    }
  }
}

void SleepDisplay::drawRat(float tSec) {
  // Breathing: the body swells a little and settles, slowly.
  const float breath = 0.5f - 0.5f * cosf(tSec * style::kTwoPi / kBreathSec);
  const float ry = kBodyRy + 0.5f * breath;
  const float bodyY = kBodyY - 0.3f * breath;

  // Tail first (the body covers its root): out of the rump, curling round
  // the back and tucked along the cushion under the belly - well clear of
  // the head.
  for (float a = -0.9f; a < 1.9f; a += 0.04f) {
    const int x = static_cast<int>(lroundf(19 + 7 * cosf(a)));
    const int y = static_cast<int>(lroundf(25.5f + 2.7f * sinf(a)));
    frame_.blend(x, y, kPink, 0.8f);
  }

  // Body, then head, each lit from above.
  for (int y = 14; y < 30; ++y) {
    for (int x = 0; x < 26; ++x) {
      const float body = ellipseSdf(x, y, kBodyX, bodyY, kBodyRx, ry);
      const float head = ellipseSdf(x, y, kHeadX, kHeadY, kHeadRx, kHeadRy);
      const float d = fminf(body, head);
      const float cover = style::coverage(d);
      if (cover <= 0) continue;
      float c[3];
      const float top = body < head ? (y - (bodyY - ry)) / (2 * ry) : (y - (kHeadY - kHeadRy)) / (2 * kHeadRy);
      style::lerpRgb(kFurLight, kFur, top * 1.6f, c);
      if (d > -0.8f) {  // outline
        float shade[3];
        style::toFloat(kFurShade, shade);
        style::lerpRgb(c, shade, 0.6f, c);
      }
      frame_.blend(x, y, c, cover);
    }
  }

  // Ear: a pink-lined round ear on top of the head.
  for (int y = 17; y <= 21; ++y) {
    for (int x = 6; x <= 12; ++x) {
      const float outer = style::circleSdf(x, y, 9.0f, 19.5f, 2.2f);
      if (style::coverage(outer) <= 0) continue;
      const bool inner = style::circleSdf(x, y, 9.0f, 19.7f, 1.1f) < 0;
      frame_.blend(x, y, inner ? kPink : kFurShade, style::coverage(outer));
    }
  }
  // Closed eye (a little curved line), pink nose at the tip of the snout,
  // and a whisker.
  frame_.blend(5, 23, kEyeLine);
  frame_.blend(6, 23, kEyeLine);
  frame_.blend(7, 22, kEyeLine);
  frame_.blend(3, 25, kPink);
  frame_.blend(2, 24, kFurShade, 0.6f);
}

void SleepDisplay::drawZs(float tSec) {
  // A "z" leaves the rat's head every kZEverySec and drifts up and to the
  // right, growing from a small 'z' to a capital 'Z' and fading out.
  style::FrameCanvas canvas(frame_);
  canvas.setTextWrap(false);
  canvas.setTextSize(1);
  const int newest = static_cast<int>(tSec / kZEverySec);
  for (int i = newest; i >= 0 && i > newest - 4; --i) {
    const float age = tSec - i * kZEverySec;
    if (age < 0 || age > kZRiseSec) continue;
    const float k = age / kZRiseSec;
    const float fade = sinf(k * 3.14159f);  // in, then out
    const int x = static_cast<int>(lroundf(10 + k * 12 + sinf(age * 2.0f) * 1.2f));
    const int y = static_cast<int>(lroundf(14 - k * 14));
    const uint8_t c[3] = {static_cast<uint8_t>(kZ[0] * fade), static_cast<uint8_t>(kZ[1] * fade),
                          static_cast<uint8_t>(kZ[2] * fade)};
    const uint16_t color = style::rgb565(c);
    canvas.drawChar(x, y, k < 0.45f ? 'z' : 'Z', color, color, 1);
  }
}

void SleepDisplay::drawTime() {
  struct tm local;
  if (!getLocalTime(&local, 0)) return;
  char text[6];
  clockface::formatHm(local, text, sizeof(text));

  style::FrameCanvas canvas(frame_);
  const uint16_t color = style::rgb565(kTime);
  // "HH:MM" with a narrow 2px colon, centred in the right-hand area.
  const int width = style::textWidth(4) + 4;
  int x = kTimeX + (kTimeW - width) / 2;
  for (const char *c = text; *c; ++c) {
    if (*c == ':') {
      canvas.drawPixel(x + 1, kTimeY + 2, color);
      canvas.drawPixel(x + 1, kTimeY + 4, color);
      x += 4;
    } else {
      canvas.drawChar(x, kTimeY, *c, color, color, 1);
      x += 6;
    }
  }
}
