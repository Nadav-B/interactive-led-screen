#pragma once
#include <Arduino.h>

#include "images_bitmap.h"
#include "public-domain-vectors-CDDJKcRvogA-unsplash_bitmap.h"

const uint16_t *const slides[] = {
  imagesBitmap,
  public_domain_vectors_CDDJKcRvogA_unsplashBitmap,
};
constexpr size_t SLIDE_COUNT = sizeof(slides) / sizeof(slides[0]);
