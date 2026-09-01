#include "RatFieldAnimation.h"

#include <math.h>

namespace {
constexpr int kSkyColor[3] = {8, 10, 20};
constexpr int kGrassDark[3] = {18, 70, 24};
constexpr int kGrassLight[3] = {30, 100, 34};
constexpr int kEyeColor[3] = {15, 10, 15};
constexpr int kShadowColor[3] = {6, 26, 10};  // sits on both grass stripes
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
static_assert(sizeof(kPalette) / sizeof(kPalette[0]) == RatFieldAnimation::kPaletteCount,
              "kPalette entries must match RatFieldAnimation::kPaletteCount");

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

// Ground covered per hop-cycle frame. Advancing the cycle by distance
// rather than wall-clock keeps the stride length constant, so a fast rat
// takes quicker steps instead of skating with the same leg cadence.
constexpr float kPixelsPerHopFrame = 1.5f;

// Chance per second of a small vertical wander, so paths aren't perfectly
// straight. Scaled by dt at the call site so it's frame-rate independent.
constexpr float kWanderPerSecond = 0.4f;
}  // namespace

RatFieldAnimation::RatFieldAnimation(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : matrix_(matrix), width_(width), height_(height), groundY_(height - kGrassRows) {}

void RatFieldAnimation::begin() {
  skyColor_ = matrix_->color565(kSkyColor[0], kSkyColor[1], kSkyColor[2]);
  grassLightColor_ = matrix_->color565(kGrassLight[0], kGrassLight[1], kGrassLight[2]);
  grassDarkColor_ = matrix_->color565(kGrassDark[0], kGrassDark[1], kGrassDark[2]);
  shadowColor_ = matrix_->color565(kShadowColor[0], kShadowColor[1], kShadowColor[2]);
  eyeColor_ = matrix_->color565(kEyeColor[0], kEyeColor[1], kEyeColor[2]);
  for (int i = 0; i < kPaletteCount; ++i) {
    bodyColor_[i] =
        matrix_->color565(kPalette[i].body[0], kPalette[i].body[1], kPalette[i].body[2]);
    accentColor_[i] =
        matrix_->color565(kPalette[i].accent[0], kPalette[i].accent[1], kPalette[i].accent[2]);
  }

  randomSeed(micros());
  const unsigned long now = millis();
  for (int i = 0; i < kRatCount; ++i) {
    Rat &rat = rats_[i];
    rat.x = static_cast<float>(random(0, width_));
    rat.y = groundY_ - random(static_cast<long>(kLaneBottom), static_cast<long>(kLaneTop) + 1);
    rat.speed = random(80, 200) / 10.0f;  // 8..20 px/s
    rat.direction = random(0, 2) == 0 ? 1 : -1;
    rat.colorIndex = static_cast<uint8_t>(i % kPaletteCount);
    rat.state = GaitState::Running;
    rat.distanceTravelled = random(0, 600) / 100.0f;  // desync each rat's stride
    rat.decisionAtMs = now + random(1000, 3000);
    rat.phaseOffsetMs = random(0, 3000);
  }
  lastFrameMs_ = 0;
}

void RatFieldAnimation::update() {
  const unsigned long now = millis();
  const unsigned long elapsed = now - lastFrameMs_;
  if (elapsed < kFrameIntervalMs) {
    return;
  }
  // First frame has no baseline; after a long stall (e.g. a blocking fetch
  // on the main loop), clamp so rats step forward once rather than warp.
  const unsigned long stepMs =
      lastFrameMs_ == 0 ? kFrameIntervalMs
                        : (elapsed > kMaxFrameDeltaMs ? kMaxFrameDeltaMs : elapsed);
  const float dt = stepMs / 1000.0f;
  lastFrameMs_ = now;

  drawField();
  for (Rat &rat : rats_) {
    advance(rat, now, dt);
    drawRat(rat, now);
  }
}

void RatFieldAnimation::advance(Rat &rat, unsigned long now, float dt) {
  if (now >= rat.decisionAtMs) {
    decide(rat, now);
  }

  if (rat.state != GaitState::Running) {
    return;
  }

  const float dx = rat.speed * rat.direction * dt;
  rat.x += dx;
  rat.distanceTravelled =
      fmodf(rat.distanceTravelled + fabsf(dx), kPixelsPerHopFrame * kHopFrameCount);

  // Occasional small vertical wander so paths aren't perfectly straight.
  if (random(0, 100000) < static_cast<long>(kWanderPerSecond * dt * 100000.0f)) {
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
    rat.speed = random(80, 220) / 10.0f;  // 8..22 px/s
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
  matrix_->fillRect(0, 0, width_, groundY_, skyColor_);

  // Grass stripes are runs of up to 3 px alternating parity; draw each run
  // as one horizontal line instead of a drawPixel() per cell.
  for (int y = groundY_; y < height_; ++y) {
    int x = 0;
    while (x < width_) {
      const bool lightStripe = ((x + y) / 3) % 2 == 0;
      int runEnd = x + 1;
      while (runEnd < width_ && ((((runEnd + y) / 3) % 2 == 0) == lightStripe)) {
        ++runEnd;
      }
      matrix_->drawFastHLine(x, y, runEnd - x, lightStripe ? grassLightColor_ : grassDarkColor_);
      x = runEnd;
    }
  }
}

void RatFieldAnimation::drawRat(const Rat &rat, unsigned long now) const {
  const uint16_t bodyC = bodyColor_[rat.colorIndex];
  const uint16_t accentC = accentColor_[rat.colorIndex];
  const int dir = rat.direction;
  const unsigned long phased = now + rat.phaseOffsetMs;
  const bool blink = (phased % 3000) < 150;

  auto put = [&](int dx, int dy, uint16_t color) {
    const int px = static_cast<int>(rat.x) + dx * dir;
    const int py = static_cast<int>(rat.y) + dy;
    if (px >= 0 && px < width_ && py >= 0 && py < height_) {
      matrix_->drawPixel(px, py, color);
    }
  };

  // Ground shadow at the un-hopped foot line, drawn first so the body sits
  // on top. It stays put (and shrinks) while the rat lifts, selling the hop.
  int hopIndex = 0;
  bool airborne = false;
  if (rat.state == GaitState::Running) {
    hopIndex = static_cast<int>(rat.distanceTravelled / kPixelsPerHopFrame) % kHopFrameCount;
    airborne = kHopCycle[hopIndex].legs == LegStyle::Tuck;
  }
  put(0, 3, shadowColor_);
  put(1, 3, shadowColor_);
  if (!airborne) {
    put(-1, 3, shadowColor_);
    put(2, 3, shadowColor_);
  }

  if (rat.state == GaitState::Idle) {
    // Both feet down, ears perked, tail resting — no hop, no wiggle.
    put(0, 1, bodyC);
    put(1, 1, bodyC);
    put(2, 1, bodyC);
    put(3, 1, bodyC);
    put(2, 0, bodyC);
    put(3, 0, bodyC);
    put(4, 0, bodyC);
    put(4, -1, accentC);
    put(3, 0, blink ? bodyC : eyeColor_);
    put(0, 2, bodyC);
    put(3, 2, bodyC);
    put(-1, 1, accentC);
    put(-2, 1, accentC);
    return;
  }

  const HopFrame &hop = kHopCycle[hopIndex];
  const bool tailUp = (hop.legs != LegStyle::Wide);

  auto putHopped = [&](int dx, int dy, uint16_t c) { put(dx, dy + hop.dy, c); };

  putHopped(0, 1, bodyC);
  putHopped(1, 1, bodyC);
  putHopped(2, 1, bodyC);
  putHopped(3, 1, bodyC);
  putHopped(2, 0, bodyC);
  putHopped(3, 0, bodyC);
  putHopped(4, 0, bodyC);
  putHopped(4, -1, accentC);
  putHopped(3, 0, blink ? bodyC : eyeColor_);

  switch (hop.legs) {
    case LegStyle::Wide:
      putHopped(0, 2, bodyC);
      putHopped(3, 2, bodyC);
      break;
    case LegStyle::Narrow:
      putHopped(1, 2, bodyC);
      putHopped(2, 2, bodyC);
      break;
    case LegStyle::Tuck:
      break;  // airborne, legs tucked under the body
  }

  putHopped(-1, tailUp ? 1 : 2, accentC);
  putHopped(-2, tailUp ? 0 : 1, accentC);
  if (tailUp) {
    putHopped(-3, 0, accentC);
  }
}
