#include "DnaHelix.h"

#include <math.h>

namespace {
constexpr int kStrandA[3] = {40, 220, 255};   // cyan backbone
constexpr int kStrandB[3] = {255, 90, 220};   // magenta backbone

// Base-pair rung colours, cycled along the ladder for an A/T/G/C feel.
constexpr int kRungPalette[][3] = {
    {255, 80, 80},    // red
    {90, 200, 255},   // blue
    {120, 240, 130},  // green
    {255, 210, 90},   // amber
};
constexpr int kRungColors = sizeof(kRungPalette) / sizeof(kRungPalette[0]);

constexpr float kTwoPi = 6.28318531f;
}  // namespace

DnaHelix::DnaHelix(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : matrix_(matrix),
      width_(width),
      height_(height),
      centerY_(height / 2),
      amplitude_((height / 2) * kAmplitudeFrac) {}

void DnaHelix::begin() {
  phase_ = 0.0f;
  lastFrameMs_ = 0;
}

uint16_t DnaHelix::shade(const int rgb[3], float k) const {
  if (k < 0.0f) k = 0.0f;
  if (k > 1.0f) k = 1.0f;
  return matrix_->color565(static_cast<int>(rgb[0] * k), static_cast<int>(rgb[1] * k),
                           static_cast<int>(rgb[2] * k));
}

void DnaHelix::drawNode(int x, int y, float depth, const int rgb[3]) const {
  if (x < 0 || x >= width_) {
    return;
  }
  // depth: -1 (far) .. +1 (near). Brighter and chunkier as it comes forward.
  const float k = 0.26f + 0.74f * (depth * 0.5f + 0.5f);
  const uint16_t c = shade(rgb, k);
  if (y >= 0 && y < height_) {
    matrix_->drawPixel(x, y, c);
  }
  if (depth > 0.1f) {  // fatten the near beads into little cube-ish blocks
    if (y - 1 >= 0) matrix_->drawPixel(x, y - 1, c);
    if (y + 1 < height_) matrix_->drawPixel(x, y + 1, c);
    if (depth > 0.55f && x + 1 < width_) {
      matrix_->drawPixel(x + 1, y, c);
      if (y + 1 < height_) matrix_->drawPixel(x + 1, y + 1, c);
    }
  }
  if (depth > 0.75f && y - 2 >= 0) {
    matrix_->drawPixel(x, y - 2, shade(rgb, 1.0f));  // top-edge highlight
  }
}

void DnaHelix::update() {
  const unsigned long now = millis();
  const unsigned long elapsed = now - lastFrameMs_;
  if (elapsed < kFrameIntervalMs) {
    return;
  }
  // First frame has no baseline; after a stall (e.g. a blocking fetch on the
  // main loop) clamp so the helix turns once rather than jumping.
  const unsigned long stepMs =
      lastFrameMs_ == 0 ? kFrameIntervalMs
                        : (elapsed > kMaxFrameDeltaMs ? kMaxFrameDeltaMs : elapsed);
  lastFrameMs_ = now;
  phase_ += kSpinRate * (stepMs / 1000.0f);
  if (phase_ > kTwoPi) {
    phase_ -= kTwoPi;
  }

  matrix_->fillRect(0, 0, width_, height_, matrix_->color565(2, 3, 10));

  int rungIndex = 0;
  for (int x = 0; x < width_; ++x) {
    const float a = x * kColumnPhase + phase_;
    const float swing = amplitude_ * sinf(a);
    const float depthA = cosf(a);  // strand B is a half-turn behind, so -depthA
    const int yA = centerY_ + static_cast<int>(lroundf(swing));
    const int yB = centerY_ - static_cast<int>(lroundf(swing));

    // Rung first, so the beads draw over it. Tallest and most face-on (so
    // brightest) at full swing; edge-on and dim through the cross-over.
    if (x % kRungEvery == 0) {
      const int *rc = kRungPalette[rungIndex % kRungColors];
      ++rungIndex;
      const float k = 0.12f + 0.55f * (1.0f - fabsf(depthA));
      const uint16_t c = shade(rc, k);
      int y0 = yA < yB ? yA : yB;
      int y1 = yA < yB ? yB : yA;
      if (y0 < 0) y0 = 0;
      if (y1 >= height_) y1 = height_ - 1;
      if (y1 >= y0) {
        matrix_->drawFastVLine(x, y0, y1 - y0 + 1, c);
      }
    }

    // Then the backbones, nearer strand last so it wins the overlap.
    if (depthA >= 0.0f) {  // strand A is in front
      drawNode(x, yB, -depthA, kStrandB);
      drawNode(x, yA, depthA, kStrandA);
    } else {
      drawNode(x, yA, depthA, kStrandA);
      drawNode(x, yB, -depthA, kStrandB);
    }
  }
}
