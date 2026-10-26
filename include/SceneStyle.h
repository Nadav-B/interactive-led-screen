#pragma once

#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <math.h>
#include <stdint.h>

// The shared Kurzgesagt-ish look of the clock, photo, weather and
// sun/moon scenes: the navy gradient sky with three twinkling parallax
// star layers, the glowing sun and phase-shaded moon, dark navy caption
// pills whose content rises in through a clip, the motion curves (eases,
// damped-spring pop-ins) and RGB888 framebuffer helpers. Those scenes
// compose each frame in their own width*height*3 buffer - additive glows
// need more headroom than RGB565 - and push it whole with blit(), so
// layered drawing never flickers on the panel.
namespace style {

constexpr float kTwoPi = 6.28318531f;

// Palette, straight from the Kurzgesagt-style brief.
constexpr uint8_t kSunCore[3] = {255, 247, 220};  // #fff7dc
constexpr uint8_t kGold[3] = {255, 200, 87};      // #ffc857
constexpr uint8_t kSunEdge[3] = {255, 143, 74};   // #ff8f4a
constexpr uint8_t kCoral[3] = {255, 120, 96};
constexpr uint8_t kCyan[3] = {77, 199, 235};      // #4dc7eb
constexpr uint8_t kWhite[3] = {225, 235, 255};
constexpr uint8_t kPillFill[3] = {5, 12, 30};

constexpr int kPillHeight = 11;

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

inline float smoothstep(float e0, float e1, float x) {
  const float t = clamp01((x - e0) / (e1 - e0));
  return t * t * (3 - 2 * t);
}

inline float easeOutCubic(float t) {
  t = 1 - clamp01(t);
  return 1 - t * t * t;
}

inline float easeInOutCubic(float t) {
  t = clamp01(t);
  return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2;
}

// Ease-out that overshoots slightly before settling at 1.
inline float easeOutBack(float t) {
  t = clamp01(t) - 1;
  constexpr float k = 1.4f;
  return 1 + (k + 1) * t * t * t + k * t * t;
}

// Damped spring pop-in over seconds: 0 -> overshoot -> settles at 1.
inline float spring(float t) {
  if (t <= 0) return 0;
  return 1 - expf(-5.5f * t) * cosf(13.0f * t);
}

inline void lerpRgb(const uint8_t a[3], const uint8_t b[3], float t, float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = a[i] + (b[i] - a[i]) * t;
}

// p += rgb * k, saturating.
inline void addRgb(uint8_t *p, const float rgb[3], float k) {
  for (int i = 0; i < 3; ++i) {
    const float s = p[i] + rgb[i] * k;
    p[i] = s > 255 ? 255 : static_cast<uint8_t>(s);
  }
}

// p = mix(p, c, cover).
inline void blendRgb(uint8_t *p, const float c[3], float cover) {
  for (int i = 0; i < 3; ++i) p[i] = static_cast<uint8_t>(p[i] + (c[i] - p[i]) * cover);
}

inline uint16_t rgb565(const uint8_t c[3]) {
  return ((c[0] & 0xF8) << 8) | ((c[1] & 0xFC) << 3) | (c[2] >> 3);
}

// Adafruit_GFX target that draws into a scene's RGB888 framebuffer,
// clipped to a rectangle - used for the pills, text and icons.
class FrameCanvas : public Adafruit_GFX {
public:
  FrameCanvas(uint8_t *frame, int w, int h) : Adafruit_GFX(w, h), frame_(frame) {
    clip(0, 0, w, h);
  }
  void clip(int x, int y, int w, int h) {
    cx0_ = x;
    cy0_ = y;
    cx1_ = x + w;
    cy1_ = y + h;
  }
  void unclip() { clip(0, 0, _width, _height); }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    if (x < cx0_ || y < cy0_ || x >= cx1_ || y >= cy1_) return;
    uint8_t *p = frame_ + (y * _width + x) * 3;
    p[0] = ((c >> 11) & 0x1F) << 3;
    p[1] = ((c >> 5) & 0x3F) << 2;
    p[2] = (c & 0x1F) << 3;
  }

private:
  uint8_t *frame_;
  int cx0_, cy0_, cx1_, cy1_;
};

// Fills the frame with the gradient sky and its stars. camX is the
// camera's horizontal pan in pixels; each star layer moves a fraction of
// it, nearest fastest.
void drawSky(uint8_t *frame, int width, int height, float tSec, float camX);

// Sun of radius r at (cx, cy): warm radial-gradient disc with an additive
// glow, halo rings, slowly turning ray wedges and a ring pulsing outward.
// glow scales all of that light: 1 suits the night sky; brighter skies
// want less, or it washes out.
void drawSun(uint8_t *frame, int width, int height, float cx, float cy, float r, float tSec,
             float glow = 1.0f);

// Moon of radius r at (cx, cy), lit for its phase (age 0 = new, 0.5 =
// full, waxing lit from the right): smoothstep terminator with a coral
// band, craters, a cyan rim and a cool halo that grows with illumination.
void drawMoon(uint8_t *frame, int width, int height, float cx, float cy, float r, float age,
              float illumination);

// Draws an empty pill of width w (kPillHeight tall) at (x, y), clips the
// canvas to its inside and returns the y its content should draw at: the
// content rises in from below over ~0.45s of sinceRevealSec. Call
// canvas.unclip() when done with the pill's content.
int openPill(FrameCanvas &canvas, int x, int y, int w, float sinceRevealSec);

void blit(MatrixPanel_I2S_DMA *matrix, const uint8_t *frame, int width, int height);

}  // namespace style
