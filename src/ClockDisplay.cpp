#include "ClockDisplay.h"

namespace {
// POSIX TZ string for Europe/Berlin (CET/CEST, DST handled automatically).
constexpr char kTimezone[] = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.nist.gov";
}  // namespace

ClockDisplay::ClockDisplay(MatrixPanel_I2S_DMA *matrix) : matrix_(matrix) {}

void ClockDisplay::begin() { configTzTime(kTimezone, kNtpServer1, kNtpServer2); }

void ClockDisplay::update() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return;  // Not synced yet; keep whatever is currently on screen.
  }
  if (timeinfo.tm_sec == lastSecond_) {
    return;
  }
  lastSecond_ = timeinfo.tm_sec;
  render(timeinfo);
}

void ClockDisplay::show() {
  lastSecond_ = -1;
  update();
}

void ClockDisplay::render(const struct tm &timeinfo) {
  char text[9];
  snprintf(text, sizeof(text), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min,
            timeinfo.tm_sec);

  matrix_->fillScreen(matrix_->color565(0, 0, 0));
  matrix_->setTextWrap(false);
  matrix_->setTextSize(1);
  matrix_->setTextColor(matrix_->color565(0, 220, 255));
  matrix_->setCursor(8, 12);
  matrix_->print(text);
}
