#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Cycles full-screen through the generated slides[] array (see
// include/slides.h, produced by image_matrix_creator.py from
// resources/images/).
class ImageSlider {
public:
  ImageSlider(MatrixPanel_I2S_DMA *matrix, int width, int height);

  // Draws the current image full-screen and advances to the next one
  // for the following call.
  void showNext();

private:
  MatrixPanel_I2S_DMA *matrix_;
  int width_;
  int height_;
  size_t currentIndex_ = 0;
};
