#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Procedurally animates a handful of small pixel-art rats hopping,
// pausing, and wandering back and forth across a striped grass field
// under an open sky. Loops forever; no bitmap assets involved.
class RatFieldAnimation {
public:
  RatFieldAnimation(MatrixPanel_I2S_DMA *matrix, int width, int height);

  // Seeds rat starting positions/behavior. Call once before first use.
  void begin();

  // Advances and redraws the animation, throttled to a fixed frame rate
  // internally. Safe to call every loop() iteration.
  void update();

public:
  // Number of neon body/accent colour pairs rats are drawn in.
  static constexpr int kPaletteCount = 3;

private:
  enum class GaitState { Running, Idle };

  struct Rat {
    float x;
    float y;
    float speed;                   // pixels per second
    float distanceTravelled;       // drives the hop cycle, for a constant stride
    int8_t direction;              // +1 = facing/moving right, -1 = left
    uint8_t colorIndex;            // which neon palette entry this rat uses
    GaitState state;
    unsigned long decisionAtMs;    // next time to reconsider behavior
    unsigned long phaseOffsetMs;   // per-rat offset so the blink desyncs
  };

  static constexpr int kRatCount = 3;
  static constexpr unsigned long kFrameIntervalMs = 33;     // ~30 fps
  static constexpr unsigned long kMaxFrameDeltaMs = 100;    // clamp dt after a stall

  void drawField();
  void drawRat(const Rat &rat, unsigned long now) const;
  void advance(Rat &rat, unsigned long now, float dt);
  void decide(Rat &rat, unsigned long now);

  MatrixPanel_I2S_DMA *matrix_;
  int width_;
  int height_;
  int groundY_;
  Rat rats_[kRatCount];
  unsigned long lastFrameMs_ = 0;

  // Resolved once in begin(); color565() per pixel per frame otherwise adds up.
  uint16_t skyColor_ = 0;
  uint16_t grassLightColor_ = 0;
  uint16_t grassDarkColor_ = 0;
  uint16_t shadowColor_ = 0;
  uint16_t eyeColor_ = 0;
  uint16_t bodyColor_[kPaletteCount] = {};
  uint16_t accentColor_[kPaletteCount] = {};
};
