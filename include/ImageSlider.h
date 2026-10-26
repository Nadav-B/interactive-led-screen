#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Cycles full-screen through the generated slides[] array (see
// include/slides.h, produced by image_matrix_creator.py from
// resources/images/), in the same Kurzgesagt-ish style as SunMoonDisplay:
// each photo opens through a circular iris that eases out from the
// centre with a gold ring on its edge, then a soft shine sweeps across
// it, under a gentle vignette. The first photo of each visit opens out of
// the starry navy sky (SceneStyle.h); later ones open over the previous
// photo. Shows a new photo every kSlideMs and carries on where it left
// off the next time the scene comes round.
class ImageSlider {
public:
  ImageSlider(MatrixPanel_I2S_DMA *matrix, int width, int height);
  ~ImageSlider();

  // Restarts the transition timeline at the next unseen photo. Call once
  // before the scene becomes active.
  void begin();

  // Advances and redraws, throttled to a fixed frame rate internally.
  // Safe to call every loop() iteration.
  void update();

private:
  static constexpr unsigned long kFrameIntervalMs = 33;  // ~30 fps
  static constexpr unsigned long kSlideMs = 3300;

  void render(float tSec);
  void drawPhoto(size_t index, float irisR, float shineSec);

  MatrixPanel_I2S_DMA *matrix_;
  int width_;
  int height_;
  uint8_t *frame_;  // width_ * height_ * RGB888
  size_t startIndex_ = 0;
  size_t nextIndex_ = 0;
  unsigned long startMs_ = 0;
  unsigned long lastFrameMs_ = 0;
};
