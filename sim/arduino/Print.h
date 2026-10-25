#pragma once

// Host stand-in for Arduino's Print base class (Serial and Adafruit GFX
// both derive from it): every print variant funnels into write(uint8_t).

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "WString.h"

class Print {
public:
  virtual ~Print() = default;

  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buf, size_t n) {
    size_t written = 0;
    while (n--) written += write(*buf++);
    return written;
  }
  size_t write(const char *s) { return s ? write(reinterpret_cast<const uint8_t *>(s), strlen(s)) : 0; }

  size_t print(const char *s) { return write(s); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(int v) { return printf("%d", v); }
  size_t print(unsigned int v) { return printf("%u", v); }
  size_t print(long v) { return printf("%ld", v); }
  size_t print(unsigned long v) { return printf("%lu", v); }
  size_t print(double v, int digits = 2) { return printf("%.*f", digits, v); }

  size_t println() { return write("\r\n"); }
  template <typename T>
  size_t println(const T &v) { return print(v) + println(); }

  size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3))) {
    char buf[512];
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return n > 0 ? write(reinterpret_cast<const uint8_t *>(buf), strlen(buf)) : 0;
  }
};
