#pragma once

// Host stand-in for the ESP32 HTTPClient: performs the same GET with the
// system's curl, so the weather scene shows live data in the simulator
// too.

#include <Arduino.h>
#include <WiFiClientSecure.h>

constexpr int HTTP_CODE_OK = 200;

class HTTPClient {
public:
  void setConnectTimeout(int32_t) {}
  void setTimeout(uint16_t) {}
  bool begin(WiFiClientSecure &, const String &url) {
    url_ = url;
    return true;
  }
  int GET();
  String getString() const { return body_; }
  void end() {}

private:
  String url_;
  String body_;
};
