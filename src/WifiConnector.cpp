#include "WifiConnector.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <math.h>

namespace {
// Name of the access point the device broadcasts when it needs setup.
// Join it from a phone or computer; WiFiManager serves a captive portal
// (usually pops up automatically, otherwise browse to 192.168.4.1) to
// submit your real network's credentials.
constexpr char kApName[] = "LED-Setup";
constexpr unsigned long kPortalTimeoutSec = 180;  // give up after 3 minutes unconfigured

// WiFiManager's AP callback is a plain function pointer, so it can't
// capture `this`; stash the active instance here instead.
WifiConnector *g_activeConnector = nullptr;

void handleApCallback(WiFiManager *) {
  Serial.printf("[WifiConnector] no saved network - opened setup portal \"%s\"\n", kApName);
  if (g_activeConnector != nullptr) {
    g_activeConnector->drawSetupPortal();
  }
}
}  // namespace

WifiConnector::WifiConnector(MatrixPanel_I2S_DMA *matrix) : matrix_(matrix) {}

bool WifiConnector::begin() {
  drawConnecting();

  g_activeConnector = this;
  WiFiManager wm;
  wm.setConfigPortalTimeout(kPortalTimeoutSec);
  wm.setAPCallback(handleApCallback);

  const bool connected = wm.autoConnect(kApName);
  g_activeConnector = nullptr;

  if (!connected) {
    Serial.println("[WifiConnector] setup portal timed out without a connection");
    drawFailed();
    return false;
  }

  Serial.printf("[WifiConnector] connected, IP %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void WifiConnector::drawConnecting() const {
  const uint16_t lit = matrix_->color565(60, 200, 255);
  constexpr int cx = 32;
  constexpr int cy = 15;
  constexpr int radii[] = {4, 7, 10};
  constexpr int thickness = 2;

  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  for (const int r : radii) {
    drawArc(cx, cy, r, thickness, lit);
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

void WifiConnector::drawSetupPortal() const {
  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->setTextWrap(false);
  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(255, 190, 60));
  matrix_->setCursor(2, 4);
  matrix_->print("WiFi Setup");
  matrix_->setTextColor(matrix_->color565(200, 200, 210));
  matrix_->setCursor(2, 14);
  matrix_->print("Join AP:");
  matrix_->setTextColor(matrix_->color565(120, 220, 255));
  matrix_->setCursor(2, 23);
  matrix_->print(kApName);
}

void WifiConnector::drawFailed() const {
  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->setTextWrap(false);
  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(220, 50, 50));
  matrix_->setCursor(8, 12);
  matrix_->print("No WiFi");
}
