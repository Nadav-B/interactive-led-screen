#include "IpDisplay.h"

#include <WiFi.h>

namespace {
// Splits a dotted IPv4 string into two roughly-even lines, breaking at
// whichever '.' falls closest to the middle - keeps octets intact instead
// of cutting through digits, and every IPv4 address is short enough that
// each half comfortably fits the panel's 64px width at text size 1.
void splitAtMiddleDot(const String &ip, String *line1, String *line2) {
  const int middle = ip.length() / 2;
  int bestDot = -1;
  int bestDistance = ip.length();
  for (int i = 0; i < static_cast<int>(ip.length()); ++i) {
    if (ip[i] == '.') {
      const int distance = abs(i - middle);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestDot = i;
      }
    }
  }

  if (bestDot < 0) {
    *line1 = ip.substring(0, middle);
    *line2 = ip.substring(middle);
    return;
  }
  *line1 = ip.substring(0, bestDot + 1);
  *line2 = ip.substring(bestDot + 1);
}
}  // namespace

IpDisplay::IpDisplay(MatrixPanel_I2S_DMA *matrix) : matrix_(matrix) {}

void IpDisplay::show() const {
  const String ip = WiFi.localIP().toString();
  Serial.printf("[IpDisplay] showing IP %s\n", ip.c_str());

  String line1, line2;
  splitAtMiddleDot(ip, &line1, &line2);

  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->setTextWrap(false);

  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(120, 190, 255));
  matrix_->setCursor(2, 0);
  matrix_->print("Settings:");

  matrix_->setTextColor(matrix_->color565(230, 230, 230));
  matrix_->setCursor(2, 12);
  matrix_->print(line1);
  matrix_->setCursor(2, 21);
  matrix_->print(line2);
}
