#pragma once

#include "SceneStyle.h"

// The night scene (main.cpp shows it from 23:00 to 07:00, at low
// brightness): a rat curled up asleep on a cushion, breathing slowly, with
// "z"s drifting up from it, and the time in dim blue - on a black panel,
// so it barely lights the room. It renders only a few frames a second,
// which is all its slow motion needs, so the board is nearly idle.
class SleepDisplay : public style::Scene {
public:
  SleepDisplay(MatrixPanel_I2S_DMA *matrix, int width, int height);

protected:
  unsigned long frameIntervalMs() const override { return 150; }  // ~7 fps

private:
  void render(float tSec) override;
  void drawCushion();
  void drawRat(float tSec);
  void drawZs(float tSec);
  void drawTime();
};
