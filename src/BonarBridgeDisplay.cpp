#include "BonarBridgeDisplay.h"

#include <math.h>
#include <time.h>

#include "SceneStyle.h"
#include "SkyEvents.h"

namespace {
// Bonar Bridge, Sutherland. The sky is computed here rather than at the
// configured location.
constexpr double kLatDeg = 57.891;
constexpr double kLonDeg = -4.351;

// Vertical layout: the Kyle fills the bottom, the bridge deck sits just
// above it.
constexpr int kWaterTop = 22;
constexpr int kDeckY = 19;  // road surface; underside at kDeckY + 1

// The arch: from one abutment to the other, peaking kArchRise above the
// deck at the centre.
constexpr float kArchLeft = 10.0f, kArchRight = 54.0f;
constexpr float kArchRise = 8.0f;
constexpr int kHangerEvery = 4;

// The camera drifts this far either way; layers move a fraction of it.
constexpr float kDriftPx = 3.0f;
constexpr float kFarHillParallax = 0.25f, kNearHillParallax = 0.5f, kVillageParallax = 0.7f;

// Sun / moon position, up in the right-hand sky.
constexpr float kSunX = 52.0f, kSunY = 6.0f;

constexpr float kCarEverySec = 6.0f, kCarStartSec = 2.0f, kCarSpeed = 14.0f;  // px/s
constexpr float kLeapEverySec = 4.5f, kLeapStartSec = 1.2f, kLeapSec = 0.9f;
constexpr float kLeapHeight = 7.0f;

// Sky gradients (top, bottom) for day and golden hour - a little paler and
// cooler than Berlin's; night is the shared starry sky.
constexpr uint8_t kDayTop[3] = {58, 112, 178};
constexpr uint8_t kDayBottom[3] = {168, 206, 230};
constexpr uint8_t kGoldenTop[3] = {52, 54, 110};
constexpr uint8_t kGoldenBottom[3] = {255, 150, 100};

constexpr uint8_t kCloudDay[3] = {236, 240, 248};
constexpr uint8_t kCloudGolden[3] = {255, 186, 150};
constexpr uint8_t kCloudNight[3] = {44, 54, 88};

// Hills: heather far off, green and pine nearer; each by day, at golden
// hour and at night.
constexpr uint8_t kFarDay[3] = {112, 98, 150};
constexpr uint8_t kFarGolden[3] = {120, 72, 104};
constexpr uint8_t kFarNight[3] = {20, 26, 56};
constexpr uint8_t kNearDay[3] = {46, 92, 64};
constexpr uint8_t kNearGolden[3] = {62, 52, 64};
constexpr uint8_t kNearNight[3] = {10, 18, 38};
constexpr uint8_t kHeather[3] = {170, 96, 170};
constexpr uint8_t kPine[3] = {20, 56, 40};

constexpr uint8_t kWaterDayTop[3] = {74, 128, 176};
constexpr uint8_t kWaterDayBottom[3] = {28, 70, 118};
constexpr uint8_t kWaterGoldenTop[3] = {150, 96, 120};
constexpr uint8_t kWaterGoldenBottom[3] = {40, 40, 84};
constexpr uint8_t kWaterNightTop[3] = {16, 30, 64};
constexpr uint8_t kWaterNightBottom[3] = {6, 12, 30};

// The arch rib is two-tone - a sunlit top edge over a dark body - so it
// reads against both the pale sky and the hills behind it; at night both
// tones lift so it stands out against the dark.
constexpr uint8_t kSteelDay[3] = {66, 78, 108};
constexpr uint8_t kSteelNight[3] = {128, 142, 180};
constexpr uint8_t kSteelLitDay[3] = {228, 234, 246};
constexpr uint8_t kSteelLitNight[3] = {160, 174, 210};
constexpr uint8_t kDeck[3] = {120, 126, 146};
constexpr uint8_t kDeckUnder[3] = {62, 68, 90};
constexpr uint8_t kStone[3] = {156, 146, 128};
constexpr uint8_t kStoneShade[3] = {108, 100, 88};

constexpr uint8_t kCottage[3] = {232, 230, 222};
constexpr uint8_t kCottageNight[3] = {70, 76, 104};
constexpr uint8_t kSlate[3] = {58, 62, 78};
constexpr uint8_t kWindow[3] = {255, 196, 90};
constexpr uint8_t kSaltireBlue[3] = {0, 94, 184};
constexpr uint8_t kSaltireWhite[3] = {240, 244, 250};
constexpr uint8_t kPole[3] = {190, 190, 196};

constexpr uint8_t kCarRed[3] = {200, 44, 40};
constexpr uint8_t kSalmon[3] = {214, 206, 214};
constexpr uint8_t kSalmonBelly[3] = {240, 150, 140};
constexpr uint8_t kSpray[3] = {200, 230, 255};

uint32_t hash(int x, int y) {
  uint32_t h = static_cast<uint32_t>(x * 374761393 + y * 668265263);
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// Ridge lines: the y of each hill layer's top edge at canvas x.
float farRidge(float x) { return 11.5f - 2.5f * sinf(x * 0.09f + 1.0f) - 1.5f * sinf(x * 0.23f); }
float nearRidge(float x) { return 16.0f - 2.0f * sinf(x * 0.13f + 2.0f) - sinf(x * 0.31f + 0.5f); }

// The arch's centre line: y above the deck at canvas x (only meaningful
// between the abutments).
float archY(float x) {
  const float c = (kArchLeft + kArchRight) / 2, half = (kArchRight - kArchLeft) / 2;
  const float u = (x - c) / half;
  return kDeckY - kArchRise * (1 - u * u);
}

// Mix of a colour across day / golden hour / night.
void timeOfDay(const uint8_t day[3], const uint8_t golden[3], const uint8_t night[3],
               float goldenK, float nightK, float out[3]) {
  float d[3], n[3];
  style::lerpRgb(day, golden, goldenK, d);
  style::toFloat(night, n);
  style::lerpRgb(d, n, nightK, out);
}
}  // namespace

BonarBridgeDisplay::BonarBridgeDisplay(MatrixPanel_I2S_DMA *matrix, int width, int height)
    : Scene(matrix, width, height) {}

void BonarBridgeDisplay::onEnter() {
  struct tm local;
  if (!getLocalTime(&local, 0)) {
    return;  // Clock not synced yet: keep the last sky (night at first).
  }
  const time_t now = mktime(&local);
  const double alt = sky::sunAltitudeDeg(now, kLatDeg, kLonDeg);
  // Same thresholds as Berlin: night below civil dusk, golden hour from a
  // little below the horizon to ~10 degrees up.
  night_ = style::smoothstep(-1.0f, -8.0f, static_cast<float>(alt));
  golden_ = 1 - style::smoothstep(2.0f, 12.0f, static_cast<float>(alt));
  const sky::MoonPhase phase = sky::moonPhase(now);
  moonAge_ = static_cast<float>(phase.age);
  moonIllumination_ = static_cast<float>(phase.illumination);
}

void BonarBridgeDisplay::render(float tSec) {
  const float camX = kDriftPx * sinf(tSec * 0.3f);
  drawSky(tSec, camX);
  drawClouds(tSec);
  drawHills(camX);
  drawVillage(tSec, camX);
  drawWater(tSec);
  drawBridge(tSec);
  drawCar(tSec);
  drawSalmon(tSec);
}

void BonarBridgeDisplay::drawSky(float tSec, float camX) {
  // Night first (stars, navy), then the daylit gradient over it as far as
  // the sun is up.
  if (night_ > 0) style::drawSky(frame_, tSec, camX);
  const float day = 1 - night_;
  if (day > 0) {
    for (int y = 0; y < kWaterTop; ++y) {
      const float f = y / float(kWaterTop - 1);
      float dayRgb[3], goldRgb[3], c[3];
      style::lerpRgb(kDayTop, kDayBottom, f, dayRgb);
      style::lerpRgb(kGoldenTop, kGoldenBottom, f * f, goldRgb);
      style::lerpRgb(dayRgb, goldRgb, golden_, c);
      for (int x = 0; x < width_; ++x) frame_.blend(x, y, c, day);
    }
  }

  // Low and big near the horizon - setting behind the hills - small and
  // high at midday; the moon in its real phase at night.
  const float pop = style::spring(tSec * 0.8f);
  if (night_ < 0.5f) {
    style::drawSun(frame_, kSunX, kSunY + golden_ * 5, (3.5f + golden_) * pop, tSec,
                   0.25f + 0.35f * golden_);
  } else {
    style::drawMoon(frame_, kSunX, kSunY, 4.0f * pop, moonAge_, moonIllumination_);
  }
}

void BonarBridgeDisplay::drawClouds(float tSec) {
  // Two soft clouds drifting right, wrapping around: it's the Highlands.
  float c[3];
  timeOfDay(kCloudDay, kCloudGolden, kCloudNight, golden_, night_, c);
  const float span = width_ + 24.0f;
  const float clouds[][3] = {{0.0f, 4.0f, 1.6f}, {38.0f, 8.0f, 1.0f}};  // x, y, speed
  for (const auto &cl : clouds) {
    const float cx = fmodf(cl[0] + tSec * cl[2], span) - 12;
    const float cy = cl[1];
    for (int y = static_cast<int>(cy) - 4; y <= static_cast<int>(cy) + 2; ++y) {
      for (int x = static_cast<int>(cx) - 8; x <= static_cast<int>(cx) + 8; ++x) {
        float d = style::capsuleSdf(x, y, cx - 5, cy, cx + 5, cy, 1.6f);
        d = fminf(d, style::circleSdf(x, y, cx - 1.5f, cy - 1.5f, 2.6f));
        d = fminf(d, style::circleSdf(x, y, cx + 2.5f, cy - 1.0f, 2.0f));
        const float cover = style::coverage(d);
        if (cover > 0) frame_.blend(x, y, c, cover * 0.75f);
      }
    }
  }
}

void BonarBridgeDisplay::drawHills(float camX) {
  float far[3], near[3];
  timeOfDay(kFarDay, kFarGolden, kFarNight, golden_, night_, far);
  timeOfDay(kNearDay, kNearGolden, kNearNight, golden_, night_, near);
  const float farShift = camX * kFarHillParallax, nearShift = camX * kNearHillParallax;

  for (int x = 0; x < width_; ++x) {
    // Far heather hills, anti-aliased along the ridge.
    const float fr = farRidge(x - farShift);
    for (int y = static_cast<int>(fr); y < kWaterTop; ++y) {
      frame_.blend(x, y, far, style::clamp01(y + 1 - fr));
    }

    // Nearer slopes, flecked with heather by day.
    const int sx = static_cast<int>(lroundf(x - nearShift));
    const float nr = nearRidge(x - nearShift);
    for (int y = static_cast<int>(nr); y < kWaterTop; ++y) {
      frame_.blend(x, y, near, style::clamp01(y + 1 - nr));
      if ((hash(sx, y) & 15) == 0) frame_.blend(x, y, kHeather, 0.45f * (1 - night_));
    }

    // A pine every so often along the near ridge: a 3-wide, 4-tall spike.
    const int col = sx + 64;
    if (col % 9 == 4) {
      const int top = static_cast<int>(nr) - 4;
      for (int row = 0; row < 4; ++row) {
        const int halfW = row / 2;
        for (int dx = -halfW; dx <= halfW; ++dx) frame_.blend(x + dx, top + row, kPine);
      }
    }
  }
}

void BonarBridgeDisplay::drawVillage(float tSec, float camX) {
  const int dx = static_cast<int>(lroundf(camX * kVillageParallax));
  float wall[3];
  timeOfDay(kCottage, kCottage, kCottageNight, golden_, night_, wall);

  // White cottages with slate roofs on the banks either side of the arch,
  // standing just above the road.
  const int base = kDeckY - 1;
  const int cottages[] = {4, 56, 60};
  for (int cx : cottages) {
    const int x0 = cx + dx;
    for (int y = base - 2; y <= base; ++y) {
      for (int x = x0; x < x0 + 4; ++x) frame_.blend(x, y, wall);
    }
    for (int x = x0 - 1; x < x0 + 5; ++x) frame_.blend(x, base - 3, kSlate);
    for (int x = x0; x < x0 + 4; ++x) frame_.blend(x, base - 4, kSlate);
    if (night_ > 0.3f) {
      frame_.blend(x0 + 1, base - 1, kWindow, night_);
      if ((hash(cx, 1) & 1) != 0) frame_.blend(x0 + 2, base - 1, kWindow, night_);
    }
  }

  // The Saltire on a pole at the left edge, above the cottage; only its
  // loose end ripples, so the white X stays legible.
  const int poleX = 1 + dx;
  for (int y = 5; y < base - 4; ++y) frame_.blend(poleX, y, kPole);
  const float dim = 1 - 0.5f * night_;
  for (int col = 0; col < 7; ++col) {
    const float loose = col < 4 ? 0 : (col - 3) / 3.0f;
    const int wave = static_cast<int>(lroundf(0.8f * loose * sinf(tSec * 4.0f - col * 0.9f)));
    for (int row = 0; row < 5; ++row) {
      // White diagonals of the X across the 7x5 blue field.
      const float d1 = fabsf(row * 1.5f - col), d2 = fabsf((4 - row) * 1.5f - col);
      const uint8_t *c = (d1 < 0.8f || d2 < 0.8f) ? kSaltireWhite : kSaltireBlue;
      float shaded[3];
      for (int i = 0; i < 3; ++i) shaded[i] = c[i] * dim;
      frame_.blend(poleX + 1 + col, 5 + row + wave, shaded);
    }
  }
}

void BonarBridgeDisplay::drawWater(float tSec) {
  float top[3], bottom[3];
  timeOfDay(kWaterDayTop, kWaterGoldenTop, kWaterNightTop, golden_, night_, top);
  timeOfDay(kWaterDayBottom, kWaterGoldenBottom, kWaterNightBottom, golden_, night_, bottom);
  const float glint[3] = {255, 245, 225};
  // The sun's (or moon's) light, streaked across the ripples below it.
  float light[3];
  style::lerpRgb(style::kGold, style::kWhite, night_, light);

  for (int y = kWaterTop; y < height_; ++y) {
    const float f = (y - kWaterTop) / float(height_ - 1 - kWaterTop);
    float c[3];
    style::lerpRgb(top, bottom, f, c);
    for (int x = 0; x < width_; ++x) {
      frame_.blend(x, y, c);
      // Ripples: short bright dashes sliding along each row.
      const float ripple = sinf(x * 0.55f + tSec * 2.2f + y * 1.7f) *
                           sinf(x * 0.21f - tSec * 1.3f + y * 0.6f);
      if (ripple > 0.6f) frame_.add(x, y, glint, 0.18f * (ripple - 0.6f) / 0.4f);
      // Reflection of the sun/moon: a broken column below it.
      const float wobble = 1.2f * sinf(y * 1.9f + tSec * 3.0f);
      const float streak = style::clamp01(1 - fabsf(x - kSunX - wobble) / 2.5f);
      if (streak > 0 && ripple > -0.2f) frame_.add(x, y, light, 0.35f * streak * (1 - f * 0.6f));
    }
  }
}

void BonarBridgeDisplay::drawBridge(float tSec) {
  float steel[3], lit[3];
  style::lerpRgb(kSteelDay, kSteelNight, night_, steel);
  style::lerpRgb(kSteelLitDay, kSteelLitNight, night_, lit);

  // The arch grows out from both abutments, meeting in the middle; then
  // the hangers drop in.
  const float half = (kArchRight - kArchLeft) / 2;
  const float grow = style::easeOutCubic(tSec / 1.4f) * half;
  const float hangers = style::smoothstep(1.0f, 1.6f, tSec);

  // Reflection first, faint and wobbling in the water below.
  for (int x = static_cast<int>(kArchLeft); x <= static_cast<int>(kArchRight); ++x) {
    if (fminf(x - kArchLeft, kArchRight - x) > grow) continue;
    const float ry = 2 * kWaterTop - archY(x) + 1;
    const int rx = x + static_cast<int>(lroundf(0.8f * sinf(ry * 1.4f + tSec * 3.0f)));
    if (ry < height_) frame_.blend(rx, static_cast<int>(ry), steel, 0.3f);
  }
  for (int x = 0; x < width_; ++x) frame_.blend(x, 2 * kWaterTop - kDeckY, kDeckUnder, 0.35f);

  // Deck across the whole Kyle, with lamps along it after dark.
  for (int x = 0; x < width_; ++x) {
    frame_.blend(x, kDeckY, kDeck);
    frame_.blend(x, kDeckY + 1, kDeckUnder);
    if (night_ > 0.3f && x % 8 == 4) {
      frame_.add(x, kDeckY - 1, kWindow, 0.9f * night_);
      frame_.add(x, kDeckY - 2, kWindow, 0.25f * night_);
    }
  }

  // Stone abutments on each bank, down into the water.
  const int abutments[] = {static_cast<int>(kArchLeft) - 3, static_cast<int>(kArchRight)};
  for (int ax : abutments) {
    for (int y = kDeckY + 1; y <= kWaterTop + 1; ++y) {
      for (int x = ax; x < ax + 3; ++x) frame_.blend(x, y, x == ax + 2 ? kStoneShade : kStone);
    }
  }

  // The arch: a 2px steel rib, lit edge on top, anti-aliased by its
  // sub-pixel y.
  for (int x = static_cast<int>(kArchLeft); x <= static_cast<int>(kArchRight); ++x) {
    if (fminf(x - kArchLeft, kArchRight - x) > grow) continue;
    const float y = archY(x);
    const int yi = static_cast<int>(floorf(y));
    const float frac = y - yi;
    frame_.blend(x, yi, lit, 1 - frac);
    frame_.blend(x, yi + 1, frac > 0.5f ? lit : steel);
    frame_.blend(x, yi + 2, steel, frac * 0.6f);

    // Vertical hangers from the rib down to the deck.
    if (hangers > 0 && (x - static_cast<int>(kArchLeft)) % kHangerEvery == 0 &&
        x > kArchLeft && x < kArchRight) {
      for (int hy = yi + 2; hy < kDeckY; ++hy) frame_.blend(x, hy, steel, 0.7f * hangers);
    }
  }
}

void BonarBridgeDisplay::drawCar(float tSec) {
  const float t = tSec - kCarStartSec;
  if (t < 0) return;
  const float run = fmodf(t, kCarEverySec);
  const int x0 = static_cast<int>(lroundf(-4 + run * kCarSpeed));  // left to right
  if (x0 > width_) return;

  const int y = kDeckY - 1;
  for (int i = 0; i < 4; ++i) frame_.blend(x0 + i, y, kCarRed);
  frame_.blend(x0 + 1, y - 1, kCarRed);
  frame_.blend(x0 + 2, y - 1, kCarRed);
  if (night_ > 0.3f) {
    // Headlight beam ahead, red tail light behind.
    frame_.add(x0 + 4, y, kWindow, night_);
    frame_.add(x0 + 5, y, kWindow, 0.5f * night_);
    frame_.add(x0 + 6, y, kWindow, 0.2f * night_);
    const float tail[3] = {255, 30, 20};
    frame_.add(x0, y, tail, night_);
  } else {
    const float glass[3] = {150, 200, 230};
    frame_.blend(x0 + 2, y - 1, glass);
  }
}

void BonarBridgeDisplay::drawSalmon(float tSec) {
  const float t = tSec - kLeapStartSec;
  if (t < 0) return;
  const int leap = static_cast<int>(t / kLeapEverySec);
  const float since = fmodf(t, kLeapEverySec);
  // Each leap from a different spot in the water, clear of the abutments.
  const float startX = 14 + (hash(leap, 7) % 30);
  const float baseY = kWaterTop + 3;

  // Splash where it leaves and where it lands: a few drops thrown up and
  // falling back.
  auto splash = [&](float sx, float age) {
    if (age < 0 || age > 0.5f) return;
    const float k = 1 - age / 0.5f;
    for (int d = -2; d <= 2; ++d) {
      const float px = sx + d * (0.6f + age * 4);
      const float py = baseY - 1 - (2.5f - fabsf(d)) * (age * 6 - age * age * 14);
      frame_.add(static_cast<int>(lroundf(px)), static_cast<int>(lroundf(py)), kSpray, 0.8f * k);
    }
  };
  const float travel = 8.0f;
  splash(startX, since);
  splash(startX + travel, since - kLeapSec);

  if (since > kLeapSec) return;
  // A parabolic arc; the fish points along its direction of travel.
  const float p = since / kLeapSec;
  const float x = startX + travel * p;
  const float y = baseY - kLeapHeight * 4 * p * (1 - p);
  const float slope = -kLeapHeight * 4 * (1 - 2 * p) / travel;  // dy/dx
  const float len = sqrtf(1 + slope * slope);
  const float ux = 1 / len, uy = slope / len;
  for (int i = -2; i <= 1; ++i) {  // tail .. head
    const int px = static_cast<int>(lroundf(x + ux * i));
    const int py = static_cast<int>(lroundf(y + uy * i));
    if (py >= kWaterTop + 1) continue;  // still under the surface
    frame_.blend(px, py, i == -2 ? kSalmonBelly : kSalmon);
  }
}
