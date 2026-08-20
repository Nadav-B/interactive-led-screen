#include "RatFieldAnimation.h"

namespace {
constexpr int kSkyColor[3] = {8, 10, 20};
constexpr int kGrassDark[3] = {18, 70, 24};
constexpr int kGrassLight[3] = {30, 100, 34};
constexpr int kEyeColor[3] = {15, 10, 15};
constexpr int kGrassRows = 9;
constexpr float kLaneTop = 3.0f;     // rows above ground a rat's anchor can drift to
constexpr float kLaneBottom = 1.0f;

// Neon body/accent pairs, one per rat.
struct Palette {
  int body[3];
  int accent[3];
};
constexpr Palette kPalette[] = {
    {{255, 70, 160}, {255, 150, 210}},   // hot pink
    {{60, 220, 230}, {160, 255, 250}},   // cyan
    {{255, 160, 40}, {255, 210, 120}},   // orange
};
constexpr int kPaletteCount = sizeof(kPalette) / sizeof(kPalette[0]);

// Hop cycle: vertical offset (negative = up) and leg stance per phase.
enum class LegStyle { Wide, Narrow, Tuck };
struct HopFrame {
  int dy;
  LegStyle legs;
};
constexpr HopFrame kHopCycle[] = {
    {0, LegStyle::Wide},     // landed / crouch
    {-1, LegStyle::Narrow},  // push off
    {-2, LegStyle::Tuck},    // airborne peak
    {-1, LegStyle::Narrow},  // falling
};
constexpr int kHopFrameCount = sizeof(kHopCycle) / sizeof(kHopCycle[0]);
constexpr unsigned long kHopFrameMs = 100;
}  // namespace

RatFieldAnimation::RatFieldAnimation(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : matrix_(matrix), width_(width), height_(height), groundY_(height - kGrassRows) {}

void RatFieldAnimation::begin() {
  randomSeed(micros());
  const unsigned long now = millis();
  for (int i = 0; i < kRatCount; ++i) {
    Rat &rat = rats_[i];
    rat.x = static_cast<float>(random(0, width_));
    rat.y = groundY_ - random(static_cast<long>(kLaneBottom), static_cast<long>(kLaneTop) + 1);
    rat.speed = random(80, 200) / 100.0f;
    rat.direction = random(0, 2) == 0 ? 1 : -1;
    rat.colorIndex = static_cast<uint8_t>(i % kPaletteCount);
    rat.state = GaitState::Running;
    rat.decisionAtMs = now + random(1000, 3000);
    rat.phaseOffsetMs = random(0, 3000);
  }
  lastFrameMs_ = 0;
}

void RatFieldAnimation::update() {
  const unsigned long now = millis();
  if (now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  lastFrameMs_ = now;

  drawField();
  for (Rat &rat : rats_) {
    advance(rat, now);
    drawRat(rat, now);
  }
}

void RatFieldAnimation::advance(Rat &rat, unsigned long now) {
  if (now >= rat.decisionAtMs) {
    decide(rat, now);
  }

  if (rat.state != GaitState::Running) {
    return;
  }

  rat.x += rat.speed * rat.direction;

  // Occasional small vertical wander so paths aren't perfectly straight.
  if (random(0, 100) < 4) {
    rat.y += random(-1, 2);
    if (rat.y > groundY_ - kLaneBottom) rat.y = groundY_ - kLaneBottom;
    if (rat.y < groundY_ - kLaneTop) rat.y = groundY_ - kLaneTop;
  }

  if (rat.direction > 0 && rat.x > width_ + 5) {
    rat.x = -8.0f;
  } else if (rat.direction < 0 && rat.x < -8.0f) {
    rat.x = width_ + 5.0f;
  }
}

void RatFieldAnimation::decide(Rat &rat, unsigned long now) {
  if (rat.state == GaitState::Running) {
    if (random(0, 100) < 35) {
      // Stop to sniff around for a bit.
      rat.state = GaitState::Idle;
      rat.decisionAtMs = now + random(500, 1400);
      return;
    }
    // Keep running, but change pace and occasionally turn around.
    rat.speed = random(80, 220) / 100.0f;
    if (random(0, 100) < 20) {
      rat.direction = static_cast<int8_t>(-rat.direction);
    }
    rat.decisionAtMs = now + random(1500, 3500);
  } else {
    rat.state = GaitState::Running;
    rat.decisionAtMs = now + random(1500, 3500);
  }
}

void RatFieldAnimation::drawField() {
  matrix_->fillScreen(matrix_->color565(kSkyColor[0], kSkyColor[1], kSkyColor[2]));
  for (int y = groundY_; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      const bool lightStripe = ((x + y) / 3) % 2 == 0;
      const int *c = lightStripe ? kGrassLight : kGrassDark;
      matrix_->drawPixel(x, y, matrix_->color565(c[0], c[1], c[2]));
    }
  }
}

void RatFieldAnimation::drawRat(const Rat &rat, unsigned long now) const {
  const Palette &palette = kPalette[rat.colorIndex];
  const int *body = palette.body;
  const int *accent = palette.accent;
  const int dir = rat.direction;
  const unsigned long phased = now + rat.phaseOffsetMs;
  const bool blink = (phased % 3000) < 150;

  auto put = [&](int dx, int dy, const int *c) {
    const int px = static_cast<int>(rat.x) + dx * dir;
    const int py = static_cast<int>(rat.y) + dy;
    if (px >= 0 && px < width_ && py >= 0 && py < height_) {
      matrix_->drawPixel(px, py, matrix_->color565(c[0], c[1], c[2]));
    }
  };

  if (rat.state == GaitState::Idle) {
    // Both feet down, ears perked, tail resting — no hop, no wiggle.
    put(0, 1, body);
    put(1, 1, body);
    put(2, 1, body);
    put(3, 1, body);
    put(2, 0, body);
    put(3, 0, body);
    put(4, 0, body);
    put(4, -1, accent);
    put(3, 0, blink ? body : kEyeColor);
    put(0, 2, body);
    put(3, 2, body);
    put(-1, 1, accent);
    put(-2, 1, accent);
    return;
  }

  const int hopIndex = static_cast<int>((phased / kHopFrameMs) % kHopFrameCount);
  const HopFrame &hop = kHopCycle[hopIndex];
  const bool tailUp = (hop.legs != LegStyle::Wide);

  auto putHopped = [&](int dx, int dy, const int *c) { put(dx, dy + hop.dy, c); };

  putHopped(0, 1, body);
  putHopped(1, 1, body);
  putHopped(2, 1, body);
  putHopped(3, 1, body);
  putHopped(2, 0, body);
  putHopped(3, 0, body);
  putHopped(4, 0, body);
  putHopped(4, -1, accent);
  putHopped(3, 0, blink ? body : kEyeColor);

  switch (hop.legs) {
    case LegStyle::Wide:
      putHopped(0, 2, body);
      putHopped(3, 2, body);
      break;
    case LegStyle::Narrow:
      putHopped(1, 2, body);
      putHopped(2, 2, body);
      break;
    case LegStyle::Tuck:
      break;  // airborne, legs tucked under the body
  }

  putHopped(-1, tailUp ? 1 : 2, accent);
  putHopped(-2, tailUp ? 0 : 1, accent);
  if (tailUp) {
    putHopped(-3, 0, accent);
  }
}
