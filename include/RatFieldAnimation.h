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

private:
  enum class GaitState { Running, Idle };

  struct Rat {
    float x;
    float y;
    float speed;
    int8_t direction;              // +1 = facing/moving right, -1 = left
    uint8_t colorIndex;            // which neon palette entry this rat uses
    GaitState state;
    unsigned long decisionAtMs;    // next time to reconsider behavior
    unsigned long phaseOffsetMs;   // per-rat offset so hop/blink desync
  };

  static constexpr int kRatCount = 3;
  static constexpr unsigned long kFrameIntervalMs = 100;

  void drawField();
  void drawRat(const Rat &rat, unsigned long now) const;
  void advance(Rat &rat, unsigned long now);
  void decide(Rat &rat, unsigned long now);

  MatrixPanel_I2S_DMA *matrix_;
  int width_;
  int height_;
  int groundY_;
  Rat rats_[kRatCount];
  unsigned long lastFrameMs_ = 0;
};
