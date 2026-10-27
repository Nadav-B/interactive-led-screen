#include "WifiConnector.h"

#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <math.h>

namespace {
// Name of the access point the device broadcasts when it needs setup.
// Join it from a phone or computer; WiFiManager serves a captive portal
// (usually pops up automatically, otherwise browse to 192.168.4.1) to
// submit your real network's credentials.
constexpr char kApName[] = "LED-Setup";

// While the portal is open: how often to retry the saved network (only
// when nobody is connected to the portal, since a retry can briefly move
// the access point's channel), and the setup screen's frame interval.
constexpr unsigned long kRetrySavedMs = 120000;
constexpr unsigned long kPortalFrameMs = 40;
}  // namespace

WifiConnector::WifiConnector(MatrixPanel_I2S_DMA *matrix) : matrix_(matrix) {}

bool WifiConnector::begin() {
  drawConnecting();

  WiFiManager wm;
  // Non-blocking: autoConnect() tries the saved network, and if that
  // fails starts the portal and returns, leaving process() to serve it from
  // the loop below - which keeps the setup screen animating meanwhile.
  wm.setConfigPortalBlocking(false);
  if (!wm.autoConnect(kApName)) {
    Serial.printf("[WifiConnector] no network - setup portal \"%s\" open\n", kApName);
    const unsigned long start = millis();
    unsigned long lastRetryMs = start;
    while (!wm.process() && WiFi.status() != WL_CONNECTED) {
      if (wm.getWiFiIsSaved() && WiFi.softAPgetStationNum() == 0 &&
          millis() - lastRetryMs >= kRetrySavedMs) {
        lastRetryMs = millis();
        Serial.println("[WifiConnector] retrying the saved network");
        WiFi.begin();  // in the background; the loop notices if it connects
      }
      drawSetupPortal((millis() - start) / 1000.0f);
      delay(kPortalFrameMs);
    }
    wm.stopConfigPortal();
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

void WifiConnector::drawSetupPortal(float tSec) const {
  // Drawn off-screen and pushed in one go, so the frame never flickers.
  GFXcanvas16 canvas(matrix_->width(), matrix_->height());
  canvas.fillScreen(0);
  canvas.setTextWrap(false);
  canvas.setTextSize(1);

  // The whole block drifts slowly, a pixel or two either way, on two
  // unrelated periods - so over time every lit pixel moves around.
  const int dx = static_cast<int>(lroundf(1.5f * sinf(tSec * 0.21f)));
  const int dy = static_cast<int>(lroundf(1.0f * sinf(tSec * 0.13f + 1.0f)));
  // The access point's name, the thing to look for, gently pulses.
  const float pulse = 0.7f + 0.3f * sinf(tSec * 2.5f);

  canvas.setTextColor(matrix_->color565(255, 190, 60));
  canvas.setCursor(2 + dx, 4 + dy);
  canvas.print("WiFi Setup");
  canvas.setTextColor(matrix_->color565(200, 200, 210));
  canvas.setCursor(2 + dx, 14 + dy);
  canvas.print("Join AP:");
  canvas.setTextColor(matrix_->color565(120 * pulse, 220 * pulse, 255 * pulse));
  canvas.setCursor(2 + dx, 23 + dy);
  canvas.print(kApName);

  matrix_->drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
}
