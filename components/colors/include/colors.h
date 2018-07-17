#ifndef COMPONENTS_COLORS_H_
#define COMPONENTS_COLORS_H_

#include <stdint.h>

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(num, min, max) (MIN((max), MAX((num), (min))))
#define CLAMP_UINT8(num) CLAMP(num, 0, 255)

class RGBColor {
public:
  RGBColor(uint8_t r, uint8_t g, uint8_t b): r(r), g(g), b(b) {};
  RGBColor(uint8_t brightness): r(brightness), g(brightness), b(brightness) {};
  RGBColor operator*(float factor) {
    return RGBColor(CLAMP_UINT8(r * factor), CLAMP_UINT8(g * factor), CLAMP_UINT8(b * factor));
  }
  RGBColor operator+(uint8_t delta) {
    return RGBColor(CLAMP_UINT8(r + delta), CLAMP_UINT8(g + delta), CLAMP_UINT8(b + delta));
  }
  RGBColor operator-(uint8_t delta) {
    return RGBColor(CLAMP_UINT8(r - delta), CLAMP_UINT8(g - delta), CLAMP_UINT8(b - delta));
  }
  uint8_t r;
  uint8_t g;
  uint8_t b;
  static RGBColor linearBlend(const RGBColor& left, const RGBColor& right, float progress) {
    return RGBColor(
        left.r + ((right.r - left.r) * progress),
        left.g + ((right.g - left.g) * progress),
        left.b + ((right.b - left.b) * progress));
  }
  uint8_t saturation() const {
    float low = MIN(r, MIN(g, b));
    float high = MAX(r, MAX(g, b));
    return 100 * ((high - low) / high);
  }
};


class RGBWColor {
public:
  RGBWColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w): r(r), g(g), b(b), w(w) {};
  RGBWColor(const RGBColor& rgb);
  RGBWColor(uint8_t brightness): r(0), g(0), b(0), w(brightness) {};
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};

#endif /* COMPONENTS_COLORS_H_ */
