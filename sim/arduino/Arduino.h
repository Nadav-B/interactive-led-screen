#pragma once

// Host stand-in for the Arduino-ESP32 core: just the parts the scene code
// (src/*.cpp) and Adafruit GFX actually use, so they compile unchanged on
// a desktop for the simulator (see simulate.py). Time is virtual - it only
// moves when the simulator advances it - so animations render at an exact
// frame rate however fast the computer is.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "WString.h"
#include "Print.h"

#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define pgm_read_pointer(addr) (*(void *const *)(addr))

using std::max;
using std::min;

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define radians(deg) ((deg) * PI / 180.0)
#define degrees(rad) ((rad) * 180.0 / PI)

// --- Virtual time ----------------------------------------------------------

namespace sim {
// Milliseconds since "boot"; advanced by the simulator, never by the clock.
extern unsigned long nowMs;
// Wall-clock time (Unix seconds) that nowMs == 0 corresponds to.
extern time_t epochAtBoot;
}  // namespace sim

inline unsigned long millis() { return sim::nowMs; }
inline unsigned long micros() { return sim::nowMs * 1000UL; }
inline void delay(unsigned long ms) { sim::nowMs += ms; }

// ESP32 core time helpers (esp32-hal-time). The simulator's clock is the
// computer's clock at start-up, advanced in virtual time.
inline void configTzTime(const char *tz, const char *, const char * = nullptr,
                         const char * = nullptr) {
  setenv("TZ", tz, 1);
  tzset();
}
inline bool getLocalTime(struct tm *info, uint32_t = 5000) {
  const time_t now = sim::epochAtBoot + static_cast<time_t>(sim::nowMs / 1000);
  localtime_r(&now, info);
  return true;
}

// --- Random ----------------------------------------------------------------

void randomSeed(unsigned long seed);
long random(long howBig);
long random(long howSmall, long howBig);

// --- Serial ----------------------------------------------------------------

// Firmware log lines go to stderr, prefixed so they stand out from the
// simulator's own output.
class HardwareSerial : public Print {
public:
  void begin(unsigned long) {}
  size_t write(uint8_t c) override { return fputc(c, stderr) == EOF ? 0 : 1; }
};
extern HardwareSerial Serial;
