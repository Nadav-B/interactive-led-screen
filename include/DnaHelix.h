#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// A rotating DNA double-helix, rendered procedurally with no bitmap assets.
// The helix axis runs left-to-right across the full width of the panel; the
// two backbones are sine waves a half-turn out of phase that swing through
// almost the whole height, joined by vertical "base-pair" rungs. The whole
// ribbon spins about its long axis over time. Depth (the cosine of each
// strand's phase) drives per-node brightness and thickness, so the front of
// the helix reads as nearer and the beads look like little cubes catching
// the light. Fills the screen; loops forever.
class DnaHelix {
public:
  DnaHelix(MatrixPanel_I2S_DMA *matrix, int width, int height);

  // Resets the rotation phase. Call once before the scene becomes active.
  void begin();

  // Advances and redraws, throttled to a fixed frame rate internally.
  // Safe to call every loop() iteration.
  void update();

private:
  static constexpr unsigned long kFrameIntervalMs = 33;    // ~30 fps
  static constexpr unsigned long kMaxFrameDeltaMs = 100;   // clamp dt after a stall
  static constexpr float kColumnPhase = 0.34f;   // radians of twist per screen column
  static constexpr float kSpinRate = 2.2f;       // radians/sec about the long axis
  static constexpr float kAmplitudeFrac = 0.92f; // strand swing as a fraction of half-height
  static constexpr int kRungEvery = 3;           // columns between base-pair rungs

  uint16_t shade(const int rgb[3], float k) const;
  void drawNode(int x, int y, float depth, const int rgb[3]) const;

  MatrixPanel_I2S_DMA *matrix_;
  int width_;
  int height_;
  int centerY_;
  float amplitude_;
  float phase_ = 0.0f;
  unsigned long lastFrameMs_ = 0;
};
