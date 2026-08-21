#include "WifiConnector.h"

#include <WiFi.h>
#include <math.h>

#include "config/wifi_credentials.h"

namespace {
constexpr unsigned long kConnectTimeoutMs = 15000;
constexpr unsigned long kAnimFrameMs = 250;
}  // namespace

WifiConnector::WifiConnector(MatrixPanel_I2S_DMA *matrix) : matrix_(matrix) {}

bool WifiConnector::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  int frame = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < kConnectTimeoutMs) {
    drawConnecting(frame % 4);
    ++frame;
    delay(kAnimFrameMs);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WifiConnector] failed to connect within %lums\n", kConnectTimeoutMs);
    drawFailed();
    return false;
  }

  Serial.printf("[WifiConnector] connected, IP %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void WifiConnector::drawConnecting(int activeArcs) const {
  const uint16_t lit = matrix_->color565(60, 200, 255);
  const uint16_t dim = matrix_->color565(20, 60, 75);
  constexpr int cx = 32;
  constexpr int cy = 15;
  constexpr int kArcCount = 3;
  constexpr int radii[kArcCount] = {4, 7, 10};
  constexpr int thickness = 2;

  matrix_->fillScreen(matrix_->color565(0, 0, 0));

  for (int i = 0; i < kArcCount; ++i) {
    const uint16_t color = i < activeArcs ? lit : dim;
    drawArc(cx, cy, radii[i], thickness, color);
  }

  matrix_->setTextWrap(false);
  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(150, 150, 160));
  matrix_->setCursor(3, 24);
  matrix_->print("Connecting");
}

// Draws the upper half of a ring (a downward-opening arc, like a WiFi
// signal bar) of the given radius and thickness, centered on (cx, cy).
void WifiConnector::drawArc(int cx, int cy, int radius, int thickness, uint16_t color) const {
  for (int y = -radius; y <= 0; ++y) {
    for (int x = -radius; x <= radius; ++x) {
      const float d = sqrtf(static_cast<float>(x * x + y * y));
      if (d <= radius && d >= radius - thickness) {
        const int px = cx + x;
        const int py = cy + y;
        if (px >= 0 && px < matrix_->width() && py >= 0 && py < matrix_->height()) {
          matrix_->drawPixel(px, py, color);
        }
      }
    }
  }
}

void WifiConnector::drawFailed() const {
  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->setTextWrap(false);
  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(220, 50, 50));
  matrix_->setCursor(8, 12);
  matrix_->print("No WiFi");
}
