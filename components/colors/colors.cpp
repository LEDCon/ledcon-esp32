#include "colors.h"

RGBWColor::RGBWColor(const RGBColor& rgb) {
  unsigned int white = CLAMP_UINT8((255 - rgb.saturation()) / 255 * (rgb.r+ rgb.g + rgb.b) / 3);
  this->r = rgb.r - white;
  this->g = rgb.g - white;
  this->b = rgb.b - white;
  this->w = white;
}
