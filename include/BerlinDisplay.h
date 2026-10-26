#pragma once

#include "LocationService.h"
#include "SceneStyle.h"

// A little animated Berlin skyline in the same Kurzgesagt-ish style as the
// other scenes (see SceneStyle.h): the Fernsehturm at Alexanderplatz -
// silver sphere with its window band, red-and-white antenna and blinking
// aviation light - beside the Berliner Dom's green dome and the Rotes
// Rathaus tower, over a city of rooftops, with a red-and-ochre S-Bahn
// rolling across a brick viaduct along the bottom.
//
// The sky follows the real time of day at the configured location (from
// lib/SkyEvents' sun altitude): blue by day, coral at golden hour, the
// starry navy sky at night with the moon in its real phase - and the
// windows of the city, the tower's band and the train light up after
// dark. The camera drifts slowly so the layers move in parallax, and the
// tower rises into place when the scene appears.
class BerlinDisplay : public style::Scene {
public:
  BerlinDisplay(MatrixPanel_I2S_DMA *matrix, LocationService *locationService, int width,
                int height);

private:
  // Reads the sun's altitude and the moon's phase for this visit.
  void onEnter() override;
  void render(float tSec) override;
  void drawSky(float tSec);
  void drawRooftops(float camX);
  void drawLandmarks(float camX);
  void drawTower(float tSec, float camX);
  void drawTrain(float tSec);
  void drawViaduct();

  LocationService *locationService_;

  // 0 = full day, 1 = full night, from the sun's real altitude; refreshed
  // on each visit.
  float night_ = 1;
  float golden_ = 0;
  float moonAge_ = 0.5f;
  float moonIllumination_ = 1;
};
