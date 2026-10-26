// LED panel simulator: runs the firmware's real scene classes (src/*.cpp)
// against an in-memory 64x32 panel on the computer and records every
// frame, so screenshots and clips can be made without the board. Built
// and run by simulate.py, which also renders the recorded frames.
//
//   led_sim --out DIR [--fps 30] [--seconds 8] [--seed N] [--scenes a,b,...]
//
// Writes DIR/<scene>.bin per scene: a sequence of frames, each a 4-byte
// little-endian timestamp (ms since the scene started) followed by 64x32
// little-endian RGB565 pixels, row-major.

#include <Arduino.h>

#include <cstdio>
#include <set>
#include <string>

#include "BerlinDisplay.h"
#include "ClockDisplay.h"
#include "ImageSlider.h"
#include "LocationService.h"
#include "SunMoonDisplay.h"
#include "WeatherDisplay.h"

namespace {
constexpr int kWidth = 64;
constexpr int kHeight = 32;

class Recorder {
public:
  Recorder(const std::string &path) : f_(fopen(path.c_str(), "wb")) {
    if (!f_) {
      fprintf(stderr, "cannot write %s\n", path.c_str());
      exit(1);
    }
  }
  ~Recorder() { fclose(f_); }

  void frame(const MatrixPanel_I2S_DMA &matrix, unsigned long sceneMs) {
    const uint32_t t = static_cast<uint32_t>(sceneMs);
    const uint8_t ts[4] = {uint8_t(t), uint8_t(t >> 8), uint8_t(t >> 16), uint8_t(t >> 24)};
    fwrite(ts, 1, sizeof(ts), f_);
    for (uint16_t px : matrix.frame()) {
      const uint8_t le[2] = {uint8_t(px), uint8_t(px >> 8)};
      fwrite(le, 1, sizeof(le), f_);
    }
    frames_++;
  }
  int frames() const { return frames_; }

private:
  FILE *f_;
  int frames_ = 0;
};

// Calls step() once per frame for `seconds` of virtual time at `fps`,
// recording the panel after each step.
template <typename Step>
int recordAnimation(MatrixPanel_I2S_DMA &matrix, const std::string &path, int fps, int seconds,
                    Step step) {
  Recorder rec(path);
  const unsigned long start = sim::nowMs;
  const int total = fps * seconds;
  for (int i = 0; i < total; i++) {
    sim::nowMs = start + static_cast<unsigned long>(i * 1000.0 / fps + 0.5);
    step();
    rec.frame(matrix, sim::nowMs - start);
  }
  return rec.frames();
}
}  // namespace

int main(int argc, char **argv) {
  std::string outDir = ".";
  int fps = 30;
  int seconds = 8;
  unsigned long seed = 0;
  std::set<std::string> scenes = {"clock", "slides", "sunmoon", "weather", "berlin"};

  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    const char *val = i + 1 < argc ? argv[i + 1] : "";
    if (arg == "--out") outDir = val, i++;
    else if (arg == "--fps") fps = atoi(val), i++;
    else if (arg == "--seconds") seconds = atoi(val), i++;
    else if (arg == "--seed") seed = strtoul(val, nullptr, 10), i++;
    else if (arg == "--scenes") {
      scenes.clear();
      std::string list = val;
      size_t pos = 0;
      while (pos <= list.size()) {
        const size_t comma = list.find(',', pos);
        const std::string name = list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        if (!name.empty()) scenes.insert(name);
        if (comma == std::string::npos) break;
        pos = comma + 1;
      }
      i++;
    } else {
      fprintf(stderr, "unknown argument: %s\n", arg.c_str());
      return 2;
    }
  }

  // "Boot" at the computer's current time. The seed shifts the boot
  // moment.
  sim::epochAtBoot = time(nullptr);
  sim::nowMs = 5000 + seed * 7919;

  MatrixPanel_I2S_DMA matrix(kWidth, kHeight);
  LocationService location;
  location.begin();

  auto out = [&](const char *name) { return outDir + "/" + name + ".bin"; };
  auto report = [](const char *name, int frames) { printf("%s: %d frame(s)\n", name, frames); };

  if (scenes.count("clock")) {
    ClockDisplay clock(&matrix, &location, kWidth, kHeight);
    clock.begin();
    clock.show();
    report("clock", recordAnimation(matrix, out("clock"), fps, seconds, [&] { clock.update(); }));
  }

  if (scenes.count("slides")) {
    ImageSlider slider(&matrix, kWidth, kHeight);
    slider.begin();
    report("slides", recordAnimation(matrix, out("slides"), fps, seconds, [&] { slider.update(); }));
  }

  if (scenes.count("sunmoon")) {
    ClockDisplay clock(&matrix, &location);
    clock.begin();  // applies the TZ, so rise/set times show in local time
    SunMoonDisplay sunMoon(&matrix, &location, kWidth, kHeight);
    sunMoon.begin();
    report("sunmoon",
           recordAnimation(matrix, out("sunmoon"), fps, seconds, [&] { sunMoon.update(); }));
  }

  if (scenes.count("berlin")) {
    BerlinDisplay berlin(&matrix, &location, kWidth, kHeight);
    berlin.begin();
    report("berlin", recordAnimation(matrix, out("berlin"), fps, seconds, [&] { berlin.update(); }));
  }

  if (scenes.count("weather")) {
    WeatherDisplay weather(&matrix, &location, kWidth, kHeight);
    weather.begin();
    weather.show();
    report("weather",
           recordAnimation(matrix, out("weather"), fps, seconds, [&] { weather.animate(); }));
  }
  return 0;
}
