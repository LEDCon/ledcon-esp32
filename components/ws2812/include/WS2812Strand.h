#ifndef COMPONENTS_WS2812_WS2812STRAND_H_
#define COMPONENTS_WS2812_WS2812STRAND_H_
#include <stdint.h>
#include <driver/rmt.h>
#include <driver/gpio.h>
#include "colors.h"


class WS2812_Strand {
public:
  WS2812_Strand(gpio_num_t dinPin, uint16_t pixelCount, uint8_t pixelWidth, rmt_channel_t rmtChannel);
  void show();
  void fill(const RGBColor& color);
  void setPixelColor(uint16_t index, const RGBColor& color);
  void shiftBuffer(uint16_t by);
  virtual ~WS2812_Strand();
  uint16_t pixelCount;
  uint8_t pixelWidth;
protected:
  static void isr(void* arg);
  static void fillMemoryEncoded(rmt_channel_t rmtChannel, const uint8_t *data, size_t len, size_t tx_offset);
  inline size_t bufferLen() {
    return pixelCount * pixelWidth;
  }
  gpio_num_t pin;
  rmt_channel_t rmtChannel;
  uint8_t* buffer;
  uint8_t* currentPos;
  size_t currentLen;
  size_t txOffset;
  SemaphoreHandle_t sem = xSemaphoreCreateBinary();
};

#endif /* COMPONENTS_WS2812_WS2812STRAND_H_ */
