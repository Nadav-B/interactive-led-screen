#pragma once

#include "SceneStyle.h"

// Bonar Bridge, in the Scottish Highlands, in the same Kurzgesagt-ish
// style as the other scenes (see SceneStyle.h): the village's steel arch
// bridge over the Kyle of Sutherland, heather-purple hills and pine-dotted
// slopes behind, white cottages and a waving Saltire on the bank, a car
// crossing now and then and a salmon leaping out of the water - a nod to
// the Falls of Shin just upriver.
//
// Like the Berlin scene, the sky follows the real time of day - but at
// Bonar Bridge itself (kLatDeg/kLonDeg in the .cpp), not the configured
// location, so it shows the Highlands' own longer summer evenings and
// earlier winter dark. Blue by day, coral at golden hour, the starry navy
// sky at night with the moon in its real phase, when the cottages, the
// bridge lamps and the car's headlights come on. The camera drifts slowly
// so the hills move in parallax, and the arch grows out from both banks
// when the scene appears.
class BonarBridgeDisplay : public style::Scene {
public:
  BonarBridgeDisplay(MatrixPanel_I2S_DMA *matrix, int width, int height);

private:
  // Reads Bonar Bridge's sun altitude and the moon's phase for this visit.
  void onEnter() override;
  void render(float tSec) override;
  void drawSky(float tSec, float camX);
  void drawClouds(float tSec);
  void drawHills(float camX);
  void drawVillage(float tSec, float camX);
  void drawWater(float tSec);
  void drawBridge(float tSec);
  void drawCar(float tSec);
  void drawSalmon(float tSec);

  // 0 = full day, 1 = full night, from the sun's real altitude at Bonar
  // Bridge; refreshed on each visit.
  float night_ = 1;
  float golden_ = 0;
  float moonAge_ = 0.5f;
  float moonIllumination_ = 1;
};
