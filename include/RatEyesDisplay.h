#pragma once

#include "SceneStyle.h"

// The wet rat from the shower, looking straight at you - until its eyes
// start beating like a heart: "ba-dum", swelling into big googly eyes with
// pink love-heart pupils and shrinking straight back to their own small
// size, a few beats in a row and then a pause. Twice per visit.
//
// The photo is resources/scenes/wet-rat.png, turned into
// include/wet-rat_bitmap.h by image_matrix_creator.py. Unlike the private
// slides, both are committed; if the header is ever missing the scene
// just stays out of the rotation.
class RatEyesDisplay : public style::Scene {
public:
  RatEyesDisplay(MatrixPanel_I2S_DMA *matrix, int width, int height);

  bool isActive() override;

private:
  void render(float tSec) override;
  void drawPhoto(int dx, int dy);
  void drawEye(float cx, float cy, float radius, float tSec, float jitterPhase);
};
