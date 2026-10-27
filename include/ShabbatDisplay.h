#pragma once

#include "LocationService.h"
#include "SceneStyle.h"

// Shabbat Shalom, in the same Kurzgesagt-ish style as the other scenes
// (see SceneStyle.h): two Shabbat candles in silver candlesticks whose
// flames catch one after the other and then flicker, warming the evening
// sky around them, a braided challah on a white tablecloth, and
// "Shabbat Shalom" written in letter by letter in gold, then "Motek" in
// pink with a beating heart.
//
// Only in the rotation on Shabbat - from candle lighting on Friday to
// nightfall on Saturday at the configured location (see lib/Shabbat);
// isActive() tells main.cpp when.
class ShabbatDisplay : public style::Scene {
public:
  ShabbatDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService, int width,
                 int height);

  bool isActive() override;

private:
  void render(float tSec) override;
  void drawTablecloth();
  void drawCandle(float cx, float igniteSec, float phase, float tSec);
  void drawChallah(float tSec);
  void drawGreeting(float tSec);

  LocationService *locationService_;
};
