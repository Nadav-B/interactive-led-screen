#pragma once

#include <time.h>

#include "LocationService.h"
#include "SceneStyle.h"
#include "SkyEvents.h"

// Today's sunrise/sunset and how much daylight that makes, for the
// configured location, in a Kurzgesagt-ish flat-vector style: navy
// gradient sky, three parallax star layers that twinkle, and a glowing
// sun cut off by the left edge (halo rings, rotating ray wedges, a ring
// pulsing outward). Values sit in dark navy pills and rise into view
// through a clip.
//
// The scene is a 2-page virtual canvas: the sun page first (rise and set
// times), then an eased camera pan to the daylight page - a 24h dial with
// today's daylight swept in gold and a hand at the current time, beside
// the day length ("9h55") and how it changed since yesterday ("-3m", or
// a gold up-arrow once the days lengthen again after the December
// solstice). Times are computed on-device by lib/SkyEvents from the
// config coordinates and the NTP clock - no API.
class SunMoonDisplay : public style::Scene {
public:
  SunMoonDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService, int width,
                 int height);

private:
  static constexpr unsigned long kPanStartMs = 4600;
  static constexpr unsigned long kPanDurationMs = 1300;

  enum class Icon { RiseArrow, SetArrow, Sun };

  // Recomputes today's (and yesterday's) sun events if the local date
  // changed (or the clock hadn't synced last time), and notes the time of
  // day for the dial's hand.
  void onEnter() override;
  void render(float tSec) override;
  void drawDial(float cx, float cy, float tSec, float revealSec);
  void drawPill(int canvasX, int y, float camX, float revealSec, float tSec, Icon icon,
                const char *text, const uint8_t rgb[3]);

  LocationService *locationService_;

  // The local date the events below are for; -1 until the clock syncs.
  int eventsYear_ = -1;
  int eventsYday_ = -1;
  sky::RiseSet sun_;
  // Today's daylight in seconds and its change since yesterday; only
  // meaningful when hasDayLength_ (both days had a sunrise and a sunset).
  bool hasDayLength_ = false;
  long dayLengthSec_ = 0;
  long dayLengthDeltaSec_ = 0;
  // Fractions of the local day (0 = midnight) for the dial.
  float riseFrac_ = 0, setFrac_ = 0, nowFrac_ = 0;
};
