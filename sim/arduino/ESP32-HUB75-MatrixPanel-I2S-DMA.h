#pragma once

// Host stand-in for the HUB75 DMA panel driver: an Adafruit GFX canvas
// that keeps the 64x32 RGB565 frame in memory instead of driving LEDs.
// Everything the scenes draw (text, shapes, bitmaps) goes through Adafruit
// GFX down to drawPixel()/fillScreen(), exactly as on the real panel.

#include <Adafruit_GFX.h>

#include <vector>

class MatrixPanel_I2S_DMA : public Adafruit_GFX {
public:
  MatrixPanel_I2S_DMA(int16_t w, int16_t h)
      : Adafruit_GFX(w, h), frame_(static_cast<size_t>(w) * h, 0) {}

  bool begin() { return true; }
  void setBrightness8(uint8_t) {}

  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    frame_[static_cast<size_t>(y) * width() + x] = color;
  }
  void fillScreen(uint16_t color) override { std::fill(frame_.begin(), frame_.end(), color); }

  // The current frame, row-major RGB565.
  const std::vector<uint16_t> &frame() const { return frame_; }

private:
  std::vector<uint16_t> frame_;
};
