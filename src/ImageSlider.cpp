#include "ImageSlider.h"

#include "slides.h"

ImageSlider::ImageSlider(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : matrix_(matrix), width_(width), height_(height) {}

void ImageSlider::showNext() {
  if (SLIDE_COUNT == 0) {
    return;
  }

  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->drawRGBBitmap(0, 0, slides[currentIndex_], width_, height_);
  currentIndex_ = (currentIndex_ + 1) % SLIDE_COUNT;
}
