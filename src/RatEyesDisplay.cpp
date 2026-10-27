#include "RatEyesDisplay.h"

#include <math.h>

#include "SceneStyle.h"

#if __has_include("wet-rat_bitmap.h")
#include "wet-rat_bitmap.h"
#define HAVE_WET_RAT 1
#else
#define HAVE_WET_RAT 0
#endif

namespace {
// Where the rat's eyes are in the 64x32 photo.
constexpr float kEyeX[2] = {21.3f, 39.6f};
constexpr float kEyeY = 7.6f;
constexpr float kEyeR = 2.2f;  // resting size, about the real eye

// Heartbeats: a "ba-dum" (a big beat, then a smaller one just after), a
// few in a row, then a pause - each swelling the eyes and letting them
// shrink straight back to their own small size.
constexpr float kCycleSec = 5.0f;
constexpr float kFirstBeatAt = 0.8f, kBeatEverySec = 0.9f;
constexpr int kBeats = 4;
constexpr float kDumAfterSec = 0.24f;  // the second, smaller beat
constexpr float kBaR = 9.0f, kDumR = 6.5f;
constexpr float kBeatWidthSec = 0.075f;

constexpr uint8_t kSclera[3] = {245, 245, 250};
constexpr uint8_t kPupil[3] = {10, 8, 12};
constexpr uint8_t kRim[3] = {60, 50, 60};
constexpr uint8_t kHeart[3] = {240, 40, 90};

// The eyes' radius t seconds into a cycle.
float eyeRadius(float t) {
  float r = kEyeR;
  for (int i = 0; i < kBeats; ++i) {
    const float ba = t - (kFirstBeatAt + i * kBeatEverySec);
    const float dum = ba - kDumAfterSec;
    r += (kBaR - kEyeR) * expf(-(ba * ba) / (kBeatWidthSec * kBeatWidthSec));
    r += (kDumR - kEyeR) * expf(-(dum * dum) / (kBeatWidthSec * kBeatWidthSec));
  }
  return r;
}

// Whether (u, v) - in pupil-radius units, v pointing down - is inside a
// heart: the classic (x^2 + y^2 - 1)^3 - x^2 y^3 <= 0 curve.
bool insideHeart(float u, float v) {
  const float x = u * 1.15f, y = -v * 1.15f + 0.25f;
  const float a = x * x + y * y - 1;
  return a * a * a - x * x * y * y * y <= 0;
}
}  // namespace

RatEyesDisplay::RatEyesDisplay(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : Scene(matrix, width, height) {}

bool RatEyesDisplay::isActive() { return HAVE_WET_RAT != 0; }

void RatEyesDisplay::render(float tSec) {
  const float t = fmodf(tSec, kCycleSec);
  drawPhoto(0, 0);
  const float r = eyeRadius(t);
  for (int e = 0; e < 2; ++e) {
    // Swelling eyes drift down a little, so even at their biggest they
    // stay on the panel.
    drawEye(kEyeX[e], kEyeY + (r - kEyeR) * 0.6f, r, tSec, e * 1.7f);
  }
}

void RatEyesDisplay::drawPhoto(int dx, int dy) {
#if HAVE_WET_RAT
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      // Shaken off the edge: repeat the nearest edge pixel.
      const int sx = x - dx < 0 ? 0 : (x - dx >= width_ ? width_ - 1 : x - dx);
      const int sy = y - dy < 0 ? 0 : (y - dy >= height_ ? height_ - 1 : y - dy);
      style::rgb888(pgm_read_word(&wet_ratBitmap[sy * width_ + sx]), frame_.at(x, y));
    }
  }
#endif
}

void RatEyesDisplay::drawEye(float cx, float cy, float radius, float tSec, float jitterPhase) {
  // At rest it's the rat's own shiny black eye; as it swells, white
  // appears round a pupil that jitters about - googly - and the pupil
  // turns into a pink heart: in love.
  const float white = style::smoothstep(kEyeR + 0.3f, kEyeR + 1.8f, radius);
  const float pupilR = radius * (1 - 0.55f * white);
  const float jitter = white * radius * 0.18f;
  const float px = cx + jitter * sinf(tSec * 23 + jitterPhase);
  const float py = cy + jitter * cosf(tSec * 19 + jitterPhase);

  const int x0 = static_cast<int>(cx - radius) - 2, x1 = static_cast<int>(cx + radius) + 2;
  const int y0 = static_cast<int>(cy - radius) - 2, y1 = static_cast<int>(cy + radius) + 2;
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const float ball = style::circleSdf(x, y, cx, cy, radius);
      const float cover = style::coverage(ball);
      if (cover <= 0) continue;
      const bool heart = white > 0.5f;
      const bool pupil = heart ? insideHeart((x - px) / pupilR, (y - py) / pupilR)
                               : style::circleSdf(x, y, px, py, pupilR) < 0;
      float c[3];
      if (pupil) {
        style::toFloat(heart ? kHeart : kPupil, c);
      } else {
        style::lerpRgb(kPupil, kSclera, white, c);
      }
      if (ball > -0.8f && white > 0) {  // a dark rim once it's a big eyeball
        float rim[3];
        style::toFloat(kRim, rim);
        style::lerpRgb(c, rim, 0.6f * white, c);
      }
      frame_.blend(x, y, c, cover);
    }
  }
  // Shine on the pupil, top left.
  const float glint[3] = {255, 255, 255};
  frame_.add(static_cast<int>(lroundf(px - pupilR * 0.45f)),
             static_cast<int>(lroundf(py - pupilR * 0.45f)), glint, 0.9f);
}
