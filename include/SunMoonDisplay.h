#pragma once

#include <time.h>

#include "LocationService.h"
#include "SceneStyle.h"
#include "SkyEvents.h"

// Today's sunrise/sunset and moonrise/moonset for the configured location,
// in a Kurzgesagt-ish flat-vector style: navy gradient sky, three
// parallax star layers that twinkle, a glowing sun cut off by the left
// edge (halo rings, rotating ray wedges, a ring pulsing outward) and a
// moon whose phase is shaded with a smoothstep terminator and a cyan rim.
// Times sit in dark navy pills and rise into view through a clip.
//
// The scene is a 2-page virtual canvas: the sun page first, then an
// eased camera pan to the moon page. Times are computed on-device by
// lib/SkyEvents from the config coordinates and the NTP clock - no API.
class SunMoonDisplay : public style::Scene {
public:
  SunMoonDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService, int width,
                 int height);

private:
  static constexpr unsigned long kPanStartMs = 4600;
  static constexpr unsigned long kPanDurationMs = 1300;

  // Recomputes today's events if the local date changed (or the clock
  // hadn't synced last time).
  void onEnter() override;
  void render(float tSec) override;
  void drawPill(int canvasX, int y, float camX, float revealSec, float tSec, bool up,
                time_t when, bool has, const uint8_t rgb[3]);

  LocationService *locationService_;

  // The local date the events below are for; -1 until the clock syncs.
  int eventsYear_ = -1;
  int eventsYday_ = -1;
  sky::RiseSet sun_;
  sky::RiseSet moon_;
  sky::MoonPhase phase_;
};
