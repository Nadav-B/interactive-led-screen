#pragma once

#include "SceneStyle.h"

// Cycles full-screen through the generated slides[] array (see
// include/slides.h, produced by image_matrix_creator.py from
// resources/images/), in the same Kurzgesagt-ish style as SunMoonDisplay:
// each photo opens through a circular iris that eases out from the
// centre with a gold ring on its edge, then a soft shine sweeps across
// it, under a gentle vignette. The first photo of each visit opens out of
// the starry navy sky (SceneStyle.h); later ones open over the previous
// photo. Shows a new photo every kSlideMs and carries on where it left
// off the next time the scene comes round.
class ImageSlider : public style::Scene {
public:
  ImageSlider(MatrixPanel_I2S_DMA *matrix, int width, int height);

private:
  static constexpr unsigned long kSlideMs = 3300;

  // Picks up at the next unseen photo.
  void onEnter() override { startIndex_ = nextIndex_; }
  void render(float tSec) override;
  void drawPhoto(size_t index, float irisR, float shineSec);

  size_t startIndex_ = 0;
  size_t nextIndex_ = 0;
};
