#pragma once

#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <math.h>
#include <stdint.h>

// The shared Kurzgesagt-ish look of every scene: the navy gradient sky
// with three twinkling parallax star layers, the glowing sun and
// phase-shaded moon, dark navy caption pills whose content rises in
// through a clip, the motion curves (eases, damped-spring pop-ins), and
// the Scene base class that gives each scene its frame loop. Scenes
// compose each frame in an RGB888 framebuffer - additive glows need more
// headroom than RGB565 - and push it whole, so layered drawing never
// flickers on the panel.
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

// --- Colour -----------------------------------------------------------------

inline void toFloat(const uint8_t c[3], float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = c[i];
}

// out = mix(a, b, t), t clamped to 0..1.
inline void lerpRgb(const float a[3], const float b[3], float t, float out[3]) {
  t = clamp01(t);
  for (int i = 0; i < 3; ++i) out[i] = a[i] + (b[i] - a[i]) * t;
}
inline void lerpRgb(const uint8_t a[3], const uint8_t b[3], float t, float out[3]) {
  float fa[3], fb[3];
  toFloat(a, fa);
  toFloat(b, fb);
  lerpRgb(fa, fb, t, out);
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

inline void rgb888(uint16_t c, uint8_t out[3]) {
  out[0] = ((c >> 11) & 0x1F) << 3;
  out[1] = ((c >> 5) & 0x3F) << 2;
  out[2] = (c & 0x1F) << 3;
}

// --- Shapes -----------------------------------------------------------------
// Signed distances (negative inside); coverage() turns one into an
// anti-aliased 0..1 pixel coverage.

inline float circleSdf(float px, float py, float cx, float cy, float r) {
  const float dx = px - cx, dy = py - cy;
  return sqrtf(dx * dx + dy * dy) - r;
}

// Distance to the segment a-b, minus radius r: a capsule.
inline float capsuleSdf(float px, float py, float ax, float ay, float bx, float by, float r) {
  const float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
  const float h = clamp01((pax * bax + pay * bay) / (bax * bax + bay * bay));
  const float dx = pax - bax * h, dy = pay - bay * h;
  return sqrtf(dx * dx + dy * dy) - r;
}

inline float coverage(float sdf) { return clamp01(0.5f - sdf); }

// --- Framebuffer ------------------------------------------------------------

// A width*height RGB888 frame with bounds-checked pixel writes.
struct Frame {
  uint8_t *data;
  int width;
  int height;

  bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
  uint8_t *at(int x, int y) const { return data + (y * width + x) * 3; }

  // Pixel = mix(pixel, rgb, cover).
  void blend(int x, int y, const float rgb[3], float cover = 1) const {
    if (contains(x, y)) blendRgb(at(x, y), rgb, cover);
  }
  void blend(int x, int y, const uint8_t rgb[3], float cover = 1) const {
    float f[3];
    toFloat(rgb, f);
    blend(x, y, f, cover);
  }
  // Pixel += rgb * k, saturating.
  void add(int x, int y, const float rgb[3], float k) const {
    if (contains(x, y)) addRgb(at(x, y), rgb, k);
  }
  void add(int x, int y, const uint8_t rgb[3], float k) const {
    float f[3];
    toFloat(rgb, f);
    add(x, y, f, k);
  }
};

// Adafruit_GFX target that draws into a Frame, clipped to a rectangle -
// used for the pills, text and icons.
class FrameCanvas : public Adafruit_GFX {
public:
  explicit FrameCanvas(const Frame &frame)
      : Adafruit_GFX(frame.width, frame.height), frame_(frame) {
    unclip();
  }
  void clip(int x, int y, int w, int h) {
    cx0_ = x;
    cy0_ = y;
    cx1_ = x + w;
    cy1_ = y + h;
  }
  void unclip() { clip(0, 0, _width, _height); }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (!frame_.contains(x, y)) return;
    if (x < cx0_ || y < cy0_ || x >= cx1_ || y >= cy1_) return;
    rgb888(c, frame_.at(x, y));
  }

private:
  Frame frame_;
  int cx0_, cy0_, cx1_, cy1_;
};

// --- Scenes -----------------------------------------------------------------

// Base for every full-screen scene: owns the frame loop. begin() restarts
// the scene's timeline when it comes on screen; update(), called every
// loop() iteration, renders at a fixed frame rate into the framebuffer
// and pushes it to the panel. Only one scene draws at a time, and each
// redraws the whole frame, so all of them share one framebuffer.
class Scene {
public:
  Scene(MatrixPanel_I2S_DMA *matrix, int width, int height);
  virtual ~Scene() = default;
  Scene(const Scene &) = delete;
  Scene &operator=(const Scene &) = delete;

  void begin();
  void update();

  // Whether the scene belongs in the rotation right now; main.cpp skips
  // it when not. Most scenes always do - ShabbatDisplay only on Shabbat.
  virtual bool isActive() { return true; }

protected:
  static constexpr unsigned long kFrameIntervalMs = 33;  // ~30 fps

  // Called by begin(), for per-visit setup (e.g. data that won't change
  // while the scene is up).
  virtual void onEnter() {}
  // Draws the whole frame for tSec seconds into the scene's visit.
  virtual void render(float tSec) = 0;

  MatrixPanel_I2S_DMA *matrix_;
  Frame frame_;
  int width_;
  int height_;

private:
  unsigned long startMs_ = 0;
  unsigned long lastFrameMs_ = 0;
};

// --- Shared drawing ---------------------------------------------------------

// Fills the frame with the gradient sky and its stars. camX is the
// camera's horizontal pan in pixels; each star layer moves a fraction of
// it, nearest fastest.
void drawSky(const Frame &frame, float tSec, float camX);

// Sun of radius r at (cx, cy): warm radial-gradient disc with an additive
// glow, halo rings, slowly turning ray wedges and a ring pulsing outward.
// glow scales all of that light: 1 suits the night sky; brighter skies
// want less, or it washes out.
void drawSun(const Frame &frame, float cx, float cy, float r, float tSec, float glow = 1.0f);

// Moon of radius r at (cx, cy), lit for its phase (age 0 = new, 0.5 =
// full, waxing lit from the right): smoothstep terminator with a coral
// band, craters, a cyan rim and a cool halo that grows with illumination.
void drawMoon(const Frame &frame, float cx, float cy, float r, float age, float illumination);

// Draws an empty pill of width w (kPillHeight tall) at (x, y), clips the
// canvas to its inside and returns the y its content should draw at: the
// content rises in from below over ~0.45s of sinceRevealSec. Call
// canvas.unclip() when done with the pill's content.
int openPill(FrameCanvas &canvas, int x, int y, int w, float sinceRevealSec);

// Where the text of a pill with an icon at its left (centred 4px in,
// up to 3px either side) should start, for text textW pixels wide: two
// clear columns after the icon, moved left only as far as needed for
// longer text to fit inside the pill.
inline int pillTextX(int x, int w, int textW) {
  const int preferred = x + 10;
  const int fit = x + w - 1 - textW;  // last text column just inside the pill
  return preferred < fit ? preferred : (fit < x + 8 ? x + 8 : fit);
}

// Width in pixels of n characters of the default 6px font, without the
// blank column after the last one.
inline int textWidth(int chars) { return chars > 0 ? 6 * chars - 1 : 0; }

}  // namespace style
