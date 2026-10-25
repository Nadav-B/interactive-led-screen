#pragma once

// Host stand-in for Arduino's String, backed by std::string. Covers what
// the scene code and ArduinoJson's String adapter use.

#include <cstddef>
#include <string>

// Flash-string type behind Arduino's F("..."); on the host it's just a
// tag for a plain char array.
class __FlashStringHelper;
#define F(literal) (reinterpret_cast<const __FlashStringHelper *>(literal))

class String {
public:
  String() = default;
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}

  const char *c_str() const { return s_.c_str(); }
  unsigned int length() const { return static_cast<unsigned int>(s_.size()); }
  bool isEmpty() const { return s_.empty(); }

  String substring(unsigned int from) const { return from < s_.size() ? s_.substr(from) : ""; }
  String substring(unsigned int from, unsigned int to) const {
    if (from >= s_.size() || to <= from) return String();
    return s_.substr(from, to - from);
  }

  char operator[](unsigned int i) const { return i < s_.size() ? s_[i] : '\0'; }
  char &operator[](unsigned int i) { return s_[i]; }
  char charAt(unsigned int i) const { return (*this)[i]; }

  bool concat(const String &o) { s_ += o.s_; return true; }
  bool concat(const char *o) { s_ += o ? o : ""; return true; }
  bool concat(char c) { s_ += c; return true; }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  String &operator+=(const char *o) { s_ += o ? o : ""; return *this; }
  String &operator+=(char c) { s_ += c; return *this; }

  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator==(const char *o) const { return s_ == (o ? o : ""); }
  bool operator!=(const char *o) const { return !(*this == o); }

  friend String operator+(const String &a, const String &b) { return a.s_ + b.s_; }
  friend String operator+(const String &a, const char *b) { return a.s_ + (b ? b : ""); }
  friend String operator+(const char *a, const String &b) { return (a ? a : "") + b.s_; }

private:
  std::string s_;
};
