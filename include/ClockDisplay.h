#pragma once

#include "SceneStyle.h"

// The local time (synced over NTP in the timezone LocationService::begin()
// applied), in the same Kurzgesagt-ish style as the other scenes: a 24h
// HH:MM in rounded seven-segment digits over the starry navy sky, with a
// gently pulsing gold colon, a thin bar beneath that fills over each
// minute (the seconds), and the date - "Mon 26 Oct" - in a navy pill
// along the bottom. A pixel-art Pikachu stands beside the time: he hops
// in when the scene appears, bobs, blinks, and his cheeks spark now and
// then. The time rises in through a clip.
class ClockDisplay : public style::Scene {
public:
  ClockDisplay(MatrixPanel_I2S_DMA *matrix, int width = 64, int height = 32);

private:
  void render(float tSec) override;

  // millis() when the wall-clock second last ticked over, for the smooth
  // sub-second motion of the colon and the seconds bar.
  int lastSecond_ = -1;
  unsigned long secondStartMs_ = 0;
};
