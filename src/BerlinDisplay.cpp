#include "BerlinDisplay.h"

#include <math.h>
#include <time.h>

#include "SceneStyle.h"
#include "SkyEvents.h"

namespace {
// Vertical layout: viaduct along the bottom, the train on top of it.
constexpr int kViaductTop = 27;
constexpr int kTrainTop = 22, kTrainH = 5, kTrainLen = 24;
constexpr float kTrainSpeed = 16.0f;  // px/s, right to left
constexpr float kTrainEverySec = 5.5f;
constexpr float kTrainStartSec = 1.5f;

// The camera drifts this far either way; layers move a fraction of it.
constexpr float kDriftPx = 3.0f;
constexpr float kRoofParallax = 0.35f, kLandmarkParallax = 0.6f, kTowerParallax = 0.8f;

// Fernsehturm, in canvas x: shaft, sphere, antenna.
constexpr float kTowerX = 43.0f;
constexpr float kSphereY = 10.0f, kSphereR = 4.6f;

// Sky gradients (top, bottom) for day and golden hour; night is the
// shared starry sky.
constexpr uint8_t kDayTop[3] = {38, 104, 186};
constexpr uint8_t kDayBottom[3] = {130, 196, 236};
constexpr uint8_t kGoldenTop[3] = {46, 52, 112};
constexpr uint8_t kGoldenBottom[3] = {255, 138, 88};

constexpr uint8_t kRoofNight[3] = {14, 22, 50};
constexpr uint8_t kRoofDay[3] = {66, 96, 150};
constexpr uint8_t kRoofGolden[3] = {58, 46, 92};
constexpr uint8_t kWindow[3] = {255, 196, 90};
constexpr uint8_t kConcrete[3] = {214, 218, 228};
constexpr uint8_t kConcreteShade[3] = {150, 156, 176};
constexpr uint8_t kSphereLight[3] = {236, 240, 248};
constexpr uint8_t kSphereDark[3] = {104, 114, 140};
constexpr uint8_t kBand[3] = {48, 56, 82};
constexpr uint8_t kAntennaRed[3] = {225, 52, 44};
constexpr uint8_t kAntennaWhite[3] = {240, 240, 245};
constexpr uint8_t kDomeGreen[3] = {78, 150, 128};
constexpr uint8_t kDomeShade[3] = {48, 104, 92};
constexpr uint8_t kStone[3] = {176, 164, 140};
constexpr uint8_t kBrick[3] = {178, 74, 56};
constexpr uint8_t kBrickShade[3] = {124, 50, 40};
constexpr uint8_t kViaduct[3] = {150, 78, 58};
constexpr uint8_t kViaductShade[3] = {98, 50, 40};
constexpr uint8_t kTrainRed[3] = {196, 40, 36};
constexpr uint8_t kTrainOchre[3] = {236, 170, 36};
constexpr uint8_t kTrainWindowDay[3] = {40, 50, 70};

// Rooftop heights above the viaduct, one per 2px column pair, over a strip
// wide enough for the drift.
constexpr int kRoofs[] = {7, 7, 9, 9, 9, 5, 5, 11, 11, 8, 8, 8, 6, 10, 10, 10, 7, 7, 12, 12,
                          9, 9, 6, 6, 6, 8, 8, 11, 11, 11, 7, 7, 9, 9, 5, 5, 10, 10};
constexpr int kRoofCount = sizeof(kRoofs) / sizeof(kRoofs[0]);

uint32_t hash(int x, int y) {
  uint32_t h = static_cast<uint32_t>(x * 374761393 + y * 668265263);
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

void mixRgb(const uint8_t a[3], const uint8_t b[3], float t, float out[3]) {
  style::lerpRgb(a, b, style::clamp01(t), out);
}
}  // namespace

BerlinDisplay::BerlinDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService,
                             int width, int height)
    : matrix_(matrix),
      locationService_(locationService),
      width_(width),
      height_(height),
      frame_(new uint8_t[width * height * 3]) {}

BerlinDisplay::~BerlinDisplay() { delete[] frame_; }

void BerlinDisplay::begin() {
  startMs_ = millis();
  lastFrameMs_ = 0;

  struct tm local;
  if (!getLocalTime(&local, 0)) {
    return;  // Clock not synced yet: keep the last sky (night at first).
  }
  const time_t now = mktime(&local);
  const double alt = sky::sunAltitudeDeg(now, atof(locationService_->latitude().c_str()),
                                         atof(locationService_->longitude().c_str()));
  // Night below civil dusk; golden hour from a little below the horizon
  // to ~10 degrees up.
  night_ = style::smoothstep(-1.0f, -8.0f, static_cast<float>(alt));
  golden_ = 1 - style::smoothstep(2.0f, 12.0f, static_cast<float>(alt));
  const sky::MoonPhase phase = sky::moonPhase(now);
  moonAge_ = static_cast<float>(phase.age);
  moonIllumination_ = static_cast<float>(phase.illumination);
}

void BerlinDisplay::update() {
  const unsigned long now = millis();
  if (lastFrameMs_ != 0 && now - lastFrameMs_ < kFrameIntervalMs) {
    return;
  }
  lastFrameMs_ = now;
  render((now - startMs_) / 1000.0f);
  style::blit(matrix_, frame_, width_, height_);
}

void BerlinDisplay::put(int x, int y, const float rgb[3], float cover) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  style::blendRgb(frame_ + (y * width_ + x) * 3, rgb, cover);
}

void BerlinDisplay::add(int x, int y, const float rgb[3], float k) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  style::addRgb(frame_ + (y * width_ + x) * 3, rgb, k);
}

void BerlinDisplay::putRgb(int x, int y, const uint8_t rgb[3]) {
  const float c[3] = {static_cast<float>(rgb[0]), static_cast<float>(rgb[1]),
                      static_cast<float>(rgb[2])};
  put(x, y, c);
}

void BerlinDisplay::render(float tSec) {
  const float camX = kDriftPx * sinf(tSec * 0.3f);
  drawSky(tSec);
  drawRooftops(camX);
  drawLandmarks(camX);
  drawTower(tSec, camX);
  drawViaduct();
  drawTrain(tSec);
}

void BerlinDisplay::drawSky(float tSec) {
  // Night first (stars, navy), then the daylit gradient over it as far as
  // the sun is up.
  style::drawSky(frame_, width_, height_, tSec, 0);
  const float day = 1 - night_;
  if (day > 0) {
    for (int y = 0; y < height_; ++y) {
      const float f = y / float(height_ - 1);
      float dayRgb[3], goldRgb[3], c[3];
      mixRgb(kDayTop, kDayBottom, f, dayRgb);
      mixRgb(kGoldenTop, kGoldenBottom, f * f, goldRgb);
      for (int i = 0; i < 3; ++i) c[i] = dayRgb[i] + (goldRgb[i] - dayRgb[i]) * golden_;
      for (int x = 0; x < width_; ++x) put(x, y, c, day);
    }
  }

  // Sun by day, the moon in its real phase by night, up in the left sky.
  const float pop = style::spring(tSec * 0.8f);
  if (night_ < 0.5f) {
    // Low and big near the horizon, small and high at midday; left of the
    // Rotes Rathaus either way.
    style::drawSun(frame_, width_, height_, 6, 6 + golden_ * 6, (3.5f + golden_) * pop, tSec,
                   0.25f + 0.35f * golden_);
  } else {
    style::drawMoon(frame_, width_, height_, 6, 6, 4.0f * pop, moonAge_, moonIllumination_);
  }
}

void BerlinDisplay::drawRooftops(float camX) {
  float dayRoof[3], roof[3];
  mixRgb(kRoofDay, kRoofGolden, golden_, dayRoof);
  for (int i = 0; i < 3; ++i) roof[i] = dayRoof[i] + (kRoofNight[i] - dayRoof[i]) * night_;
  const int shift = static_cast<int>(lroundf(camX * kRoofParallax)) - 6;
  for (int x = 0; x < width_; ++x) {
    const int col = ((x - shift) / 2) % kRoofCount;
    const int top = kViaductTop - kRoofs[col < 0 ? col + kRoofCount : col];
    for (int y = top; y < kViaductTop; ++y) {
      put(x, y, roof);
      // Lit windows after dark: a sparse grid, a few switched off.
      if (night_ > 0.3f && (x - shift) % 3 == 1 && (y - top) % 3 == 2 &&
          (hash(x - shift, y) & 3) != 0) {
        const float w[3] = {static_cast<float>(kWindow[0]), static_cast<float>(kWindow[1]),
                            static_cast<float>(kWindow[2])};
        put(x, y, w, 0.55f * night_);
      }
    }
  }
}

void BerlinDisplay::drawLandmarks(float camX) {
  const int dx = static_cast<int>(lroundf(camX * kLandmarkParallax));

  // Rotes Rathaus: square red-brick clock tower, flag pole on top.
  const int rx = 13 + dx;
  for (int y = 14; y < kViaductTop; ++y) {
    for (int x = rx; x < rx + 4; ++x) putRgb(x, y, x == rx + 3 ? kBrickShade : kBrick);
  }
  for (int x = rx - 1; x < rx + 5; ++x) putRgb(x, 20, kBrickShade);  // cornice
  putRgb(rx + 1, 13, kBrick);
  putRgb(rx + 2, 13, kBrickShade);
  putRgb(rx + 1, 11, kAntennaWhite);
  putRgb(rx + 1, 12, kAntennaWhite);
  if (night_ > 0.3f) putRgb(rx + 1, 16, kWindow);  // the lit clock face

  // Berliner Dom: stone body, big green dome with a lantern.
  const int domeX = 26 + dx;
  for (int y = 21; y < kViaductTop; ++y) {
    for (int x = domeX - 6; x <= domeX + 6; ++x) putRgb(x, y, kStone);
  }
  const float cx = domeX, cy = 21, r = 5.2f;
  for (int y = 15; y <= 21; ++y) {
    for (int x = domeX - 6; x <= domeX + 6; ++x) {
      const float ddx = x - cx, ddy = y - cy;
      const float cover = style::clamp01(r - sqrtf(ddx * ddx + ddy * ddy) + 0.5f);
      if (cover <= 0) continue;
      float c[3];
      mixRgb(kDomeGreen, kDomeShade, (ddx + r) / (2 * r) - 0.2f, c);
      put(x, y, c, cover);
    }
  }
  putRgb(domeX, 14, kDomeGreen);  // lantern
  putRgb(domeX, 13, style::kGold);  // the cross
}

void BerlinDisplay::drawTower(float tSec, float camX) {
  // Rises into place on scene entry.
  const int rise = static_cast<int>(lroundf((1 - style::easeOutBack(tSec / 1.1f)) * 32));
  const float tx = kTowerX + camX * kTowerParallax;
  const int x0 = static_cast<int>(lroundf(tx));

  // Shaft: 3px at the foot, 2px under the sphere; lit left, shaded right.
  for (int y = static_cast<int>(kSphereY + kSphereR); y < kViaductTop; ++y) {
    const int yy = y + rise;
    const bool wide = y > 21;
    putRgb(x0 - 1, yy, kConcrete);
    putRgb(x0, yy, wide ? kConcrete : kConcreteShade);
    if (wide) putRgb(x0 + 1, yy, kConcreteShade);
  }

  // Sphere: silver, lit from the upper left, with the dark window band.
  for (int y = static_cast<int>(kSphereY - kSphereR) - 1; y <= kSphereY + kSphereR + 1; ++y) {
    for (int x = x0 - 6; x <= x0 + 6; ++x) {
      const float ddx = x - tx + 0.5f, ddy = y - kSphereY;
      const float d = sqrtf(ddx * ddx + ddy * ddy);
      const float cover = style::clamp01(kSphereR - d + 0.5f);
      if (cover <= 0) continue;
      float c[3];
      const float light = style::clamp01(0.75f - (ddx + ddy) / (2.2f * kSphereR));
      mixRgb(kSphereDark, kSphereLight, light, c);
      if (y == static_cast<int>(kSphereY) || y == static_cast<int>(kSphereY) + 1) {
        for (int i = 0; i < 3; ++i) c[i] = kBand[i];
        if (night_ > 0.3f && ((x + static_cast<int>(tSec * 2)) & 1)) {
          for (int i = 0; i < 3; ++i) c[i] = kWindow[i] * (0.4f + 0.5f * night_);
        }
      }
      put(x, y + rise, c, cover);
    }
  }
  // The "Pope's revenge": sunlight on the sphere glints as a little cross.
  const float glint = powf(0.5f + 0.5f * sinf(tSec * 1.7f), 6) * (1 - night_ * 0.7f);
  if (glint > 0.05f) {
    const float w[3] = {255, 255, 255};
    const int gx = x0 - 2, gy = static_cast<int>(kSphereY) - 2 + rise;
    add(gx, gy, w, glint);
    add(gx - 1, gy, w, glint * 0.5f);
    add(gx + 1, gy, w, glint * 0.5f);
    add(gx, gy - 1, w, glint * 0.5f);
    add(gx, gy + 1, w, glint * 0.5f);
  }

  // Upper shaft and the red-and-white antenna, aviation light on top.
  const int sphereTop = static_cast<int>(kSphereY - kSphereR);
  putRgb(x0, sphereTop - 1 + rise, kConcrete);
  putRgb(x0, sphereTop - 2 + rise, kConcrete);
  for (int y = 0; y < sphereTop - 2; ++y) {
    putRgb(x0, y + rise, y % 2 ? kAntennaWhite : kAntennaRed);
  }
  if (fmodf(tSec, 1.2f) < 0.5f && rise == 0) {
    const float red[3] = {255, 40, 30};
    add(x0, 0, red, 1);
    add(x0 - 1, 0, red, 0.35f);
    add(x0 + 1, 0, red, 0.35f);
    add(x0, 1, red, 0.35f);
  }
}

void BerlinDisplay::drawViaduct() {
  // Brick arches along the bottom: solid deck, arch openings below.
  for (int y = kViaductTop; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      const int ax = x % 8;
      const bool deck = y == kViaductTop;
      const bool pier = ax < 2;
      const bool archTop = y == kViaductTop + 1 && (ax == 2 || ax == 7);
      if (deck || pier || archTop) {
        putRgb(x, y, deck ? kViaductShade : kViaduct);
      }
    }
  }
}

void BerlinDisplay::drawTrain(float tSec) {
  const float t = tSec - kTrainStartSec;
  if (t < 0) return;
  const float run = fmodf(t, kTrainEverySec);
  const int x0 = static_cast<int>(lroundf(width_ - run * kTrainSpeed));
  if (x0 + kTrainLen < 0) return;

  for (int i = 0; i < kTrainLen; ++i) {
    const int x = x0 + i;
    const bool gap = i == kTrainLen / 2;  // between the two cars
    if (gap) continue;
    const int nose = i == 0 ? 1 : 0;  // rounded front
    for (int row = nose; row < kTrainH; ++row) {
      const int y = kTrainTop + row;
      const uint8_t *c = row < 2 ? kTrainOchre : kTrainRed;
      putRgb(x, y, c);
    }
    // Windows along the ochre band, lit at night.
    if (i % 3 == 1) {
      float w[3];
      mixRgb(kTrainWindowDay, kWindow, night_, w);
      put(x, kTrainTop + 1, w);
    }
  }
  // Headlight.
  const float head[3] = {255, 250, 220};
  put(x0, kTrainTop + 3, head);
}
