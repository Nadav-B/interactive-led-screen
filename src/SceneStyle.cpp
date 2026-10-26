#include "SceneStyle.h"

namespace style {
namespace {
constexpr uint8_t kSkyTop[3] = {8, 29, 44};      // #081d2c
constexpr uint8_t kSkyBottom[3] = {16, 35, 72};  // #102348

// How far each layer moves per pixel of camera pan, and how bright it is.
struct StarLayer {
  float parallax;
  int count;
  float brightness;
};
constexpr StarLayer kLayers[] = {{0.2f, 14, 70.0f}, {0.45f, 9, 120.0f}, {0.8f, 5, 210.0f}};
constexpr int kMaxStars = 14 + 9 + 5;

struct Star {
  float x, y, phase, speed;
  int layer;
};
Star gStars[kMaxStars];
int gStarCount = 0;

// Fixed seed, so the sky is the same every time a scene comes round.
uint32_t gSeed = 0x5eed1234u;
float rand01() {
  gSeed = gSeed * 1664525u + 1013904223u;
  return (gSeed >> 8) / 16777216.0f;
}

void seedStars(int width, int height) {
  for (int l = 0; l < 3; ++l) {
    // Each layer spans the panel plus however far it travels in a
    // one-panel pan.
    const float span = width + width * kLayers[l].parallax;
    for (int i = 0; i < kLayers[l].count; ++i) {
      Star &s = gStars[gStarCount++];
      s.x = rand01() * span;
      s.y = rand01() * (height - 1);
      s.phase = rand01() * kTwoPi;
      s.speed = 1.2f + rand01() * 2.5f;
      s.layer = l;
    }
  }
}
}  // namespace

void drawSky(uint8_t *frame, int width, int height, float tSec, float camX) {
  if (gStarCount == 0) {
    seedStars(width, height);
  }

  for (int y = 0; y < height; ++y) {
    float c[3];
    lerpRgb(kSkyTop, kSkyBottom, y / float(height - 1), c);
    uint8_t *row = frame + y * width * 3;
    for (int x = 0; x < width; ++x) {
      row[x * 3] = c[0];
      row[x * 3 + 1] = c[1];
      row[x * 3 + 2] = c[2];
    }
  }

  const float starRgb[3] = {0.85f, 0.92f, 1.0f};  // slightly blue-white
  auto add = [&](int x, int y, float v) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    addRgb(frame + (y * width + x) * 3, starRgb, v);
  };
  for (int i = 0; i < gStarCount; ++i) {
    const Star &s = gStars[i];
    const StarLayer &layer = kLayers[s.layer];
    const int x = static_cast<int>(lroundf(s.x - camX * layer.parallax));
    const int y = static_cast<int>(s.y);
    const float v = layer.brightness * (0.55f + 0.45f * sinf(tSec * s.speed + s.phase));
    add(x, y, v);
    if (s.layer == 2) {  // near layer: little four-point sparkles
      const float arm = v * 0.3f * (0.5f + 0.5f * sinf(tSec * s.speed * 1.7f + s.phase));
      add(x - 1, y, arm);
      add(x + 1, y, arm);
      add(x, y - 1, arm);
      add(x, y + 1, arm);
    }
  }
}

namespace {
constexpr uint8_t kMoonLit[3] = {236, 232, 214};
constexpr uint8_t kMoonDark[3] = {24, 32, 66};  // night side, navy
constexpr int kRayCount = 18;
}  // namespace

void drawSun(uint8_t *frame, int width, int height, float cx, float cy, float r0, float tSec,
             float glowScale) {
  if (r0 <= 0.2f) return;

  const float rot = tSec * 0.35f;
  const float pulse = fmodf(tSec, 3.2f) / 3.2f;  // ring expanding from the sun
  const float pulseR = r0 + pulse * 34.0f;
  const float pulseK = 0.4f * (1 - pulse) * (1 - pulse);
  const float glow[3] = {255.0f * glowScale, 165.0f * glowScale, 80.0f * glowScale};

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float dx = x - cx, dy = y - cy;
      const float r = sqrtf(dx * dx + dy * dy);
      uint8_t *p = frame + (y * width + x) * 3;

      // Additive glow: falloff, halo rings at multiples of R, ray wedges.
      if (r > r0 - 1) {
        const float out = r - r0;
        float g = 0.6f * expf(-out / 4.0f);
        const float rings[] = {1.45f, 1.9f, 2.5f};
        for (float k : rings) g += 0.13f * clamp01(1 - fabsf(r - k * r0) / 0.9f);
        float a = (atan2f(dy, dx) + rot) * kRayCount / kTwoPi;
        a -= floorf(a);
        if (a < 0.4f) g += 0.16f * clamp01(1 - out / (2.4f * r0));
        g += pulseK * clamp01(1 - fabsf(r - pulseR) / 1.2f);
        addRgb(p, glow, g);
      }

      // Disc: warm radial gradient, anti-aliased edge.
      const float cover = clamp01(r0 - r + 0.5f);
      if (cover > 0) {
        const float f = r / r0;
        float c[3];
        if (f < 0.55f) {
          lerpRgb(kSunCore, kGold, f / 0.55f, c);
        } else {
          lerpRgb(kGold, kSunEdge, clamp01((f - 0.55f) / 0.45f), c);
        }
        blendRgb(p, c, cover);
      }
    }
  }
}

void drawMoon(uint8_t *frame, int width, int height, float cx, float cy, float r0, float age,
              float illumination) {
  if (r0 <= 0.2f) return;

  // Light direction from the phase: new moon lit from behind, full from
  // the front, waxing from the right (as seen from the north).
  const float ang = age * kTwoPi;
  const float lx = sinf(ang), lz = -cosf(ang);
  const float glowK = 0.18f + 0.3f * illumination;
  const float glow[3] = {110.0f, 160.0f, 255.0f};

  // A few craters, in disc-normalised coords: x, y, radius.
  const float craters[][3] = {{-0.3f, -0.35f, 0.22f}, {0.35f, 0.15f, 0.28f},
                              {-0.15f, 0.45f, 0.16f}, {0.1f, -0.6f, 0.12f}};

  const int x0 = static_cast<int>(cx - r0 * 3), x1 = static_cast<int>(cx + r0 * 3) + 1;
  for (int y = 0; y < height; ++y) {
    for (int x = x0 < 0 ? 0 : x0; x < width && x <= x1; ++x) {
      const float dx = x - cx, dy = y - cy;
      const float r = sqrtf(dx * dx + dy * dy);
      uint8_t *p = frame + (y * width + x) * 3;

      if (r > r0 - 1) {  // cool atmosphere-ish halo, brighter when fuller
        addRgb(p, glow, glowK * expf(-(r - r0) / 3.0f));
      }

      const float cover = clamp01(r0 - r + 0.5f);
      if (cover <= 0) continue;
      const float nx = dx / r0, ny = dy / r0;
      const float nz = sqrtf(fmaxf(0, 1 - nx * nx - ny * ny));
      const float dot = nx * lx + nz * lz;
      const float lit = smoothstep(-0.12f, 0.12f, dot);

      float surface[3];
      lerpRgb(kMoonDark, kMoonLit, lit, surface);
      for (const auto &cr : craters) {
        const float ddx = nx - cr[0], ddy = ny - cr[1];
        if (ddx * ddx + ddy * ddy < cr[2] * cr[2]) {
          for (float &s : surface) s *= 0.8f;
        }
      }
      // Coral band along the terminator, cyan rim on the lit edge.
      const float band = clamp01(1 - fabsf(dot) / 0.18f) * 0.45f;
      const float rim = (r > r0 - 1.3f) ? 0.55f * lit : 0;
      float c[3];
      for (int i = 0; i < 3; ++i) {
        c[i] = surface[i];
        c[i] += (kCoral[i] - c[i]) * band;
        c[i] += (kCyan[i] - c[i]) * rim;
      }
      blendRgb(p, c, cover);
    }
  }
}

int openPill(FrameCanvas &canvas, int x, int y, int w, float sinceRevealSec) {
  canvas.unclip();
  canvas.fillRoundRect(x, y, w, kPillHeight, 4, rgb565(kPillFill));
  // Content rises into the pill through a clip rather than fading in.
  const int rise =
      static_cast<int>(lroundf((1 - easeOutCubic(sinceRevealSec / 0.45f)) * kPillHeight));
  canvas.clip(x, y + 1, w, kPillHeight - 2);
  return y + 2 + rise;
}

void blit(MatrixPanel_I2S_DMA *matrix, const uint8_t *frame, int width, int height) {
  for (int y = 0; y < height; ++y) {
    const uint8_t *row = frame + y * width * 3;
    for (int x = 0; x < width; ++x) {
      matrix->drawPixelRGB888(x, y, row[x * 3], row[x * 3 + 1], row[x * 3 + 2]);
    }
  }
}

}  // namespace style
