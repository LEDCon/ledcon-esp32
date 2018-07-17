#include "include/WS2812Strand.h"

#include <esp_log.h>
#include <driver/rmt.h>
#include <driver/gpio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>
#include <iostream>

// divider to generate 100ns base period from 80MHz APB clock
#define WS2812_CLKDIV (100 * 80 /1000)

static intr_handle_t intr_handle;
static char TAG[] = "WS2812";

// bit H & L durations in multiples of 100ns
#define WS2812_DURATION_T0H 4
#define WS2812_DURATION_T0L 7
#define WS2812_DURATION_T1H 8
#define WS2812_DURATION_T1L 6

#define RESET_1X { 4, 0, 4, 0 }
#define RESET_2X     RESET_1X,  RESET_1X
#define RESET_4X     RESET_2X,  RESET_2X
#define RESET_8X     RESET_4X,  RESET_4X
#define RESET_16X    RESET_8X,  RESET_8X
#define RESET_32X    RESET_16X, RESET_16X
#define RESET_64X    RESET_32X, RESET_32X

static const rmt_item32_t reset_block[64] = { RESET_64X };

// 0 bit in rmt encoding
const rmt_item32_t rmt_bit0 = {
WS2812_DURATION_T0H, 1,
WS2812_DURATION_T0L, 0 };
// 1 bit in rmt encoding
const rmt_item32_t rmt_bit1 = {
WS2812_DURATION_T1H, 1,
WS2812_DURATION_T1L, 0 };


WS2812_Strand::WS2812_Strand(gpio_num_t dinPin, uint16_t pixelCount,
    uint8_t pixelWidth, rmt_channel_t rmtChannel) {
  this->pin = dinPin;
  this->pixelCount = pixelCount;
  assert(pixelWidth == 3 || pixelWidth == 4);
  this->pixelWidth = pixelWidth;
  this->rmtChannel = rmtChannel;

  DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
  DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

  RMT.apb_conf.fifo_mask = 1;  // Enable memory access, instead of FIFO mode
  RMT.apb_conf.mem_tx_wrap_en = 1;  // Wrap around when hitting end of buffer

  this->currentLen = bufferLen();
  this->buffer = new uint8_t[bufferLen()];
  this->currentPos = this->buffer;
  this->txOffset = 0;

  rmt_config_t rmt_tx;
  rmt_tx.mem_block_num = 1;
  rmt_tx.clk_div = WS2812_CLKDIV;
  rmt_tx.tx_config.loop_en = false;
  rmt_tx.tx_config.carrier_en = false;
  rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  rmt_tx.tx_config.idle_output_en = true;
  rmt_tx.rmt_mode = RMT_MODE_TX;
  rmt_tx.channel = rmtChannel;
  rmt_tx.gpio_num = dinPin;

  ESP_ERROR_CHECK(rmt_config(&rmt_tx));
  ESP_ERROR_CHECK(
      rmt_driver_install(rmtChannel, 0, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED));
  ESP_LOGD(TAG, "Setup strand OK: GPIO=%d, RMT_CHANNEL=%d", dinPin, rmtChannel);
  xSemaphoreGive(sem);
}

void WS2812_Strand::show() {
  xSemaphoreTake(sem, portMAX_DELAY);
  this->currentLen = bufferLen();
  this->currentPos = this->buffer;
  this->txOffset = 0;

  // hook-in our ISR
  ESP_ERROR_CHECK(
      esp_intr_alloc(ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED, isr, this, &intr_handle));

  // we just feed a single block for generating the reset to the rmt driver
  // the actual payload data is handled by our private ISR
  ESP_ERROR_CHECK(rmt_write_items( this->rmtChannel, (rmt_item32_t *)reset_block, 64, false ));

  ESP_ERROR_CHECK(rmt_wait_tx_done( this->rmtChannel, portMAX_DELAY));
  // un-hook our ISR
  ESP_ERROR_CHECK(esp_intr_free(intr_handle));
  ESP_LOGV(TAG, "Strand data written.");
  xSemaphoreGive(sem);
}

void IRAM_ATTR WS2812_Strand::fillMemoryEncoded(rmt_channel_t rmtChannel, const uint8_t *data, size_t len, size_t tx_offset) {
  for (size_t idx = 0; idx < len; idx++) {
    uint8_t byte = data[idx];

    for (uint8_t i = 0; i < 8; i++) {
      RMTMEM.chan[rmtChannel].data32[tx_offset + idx * 8 + i].val = byte & 0x80 ? rmt_bit1.val : rmt_bit0.val;
      byte <<= 1;
    }
  }
}

void IRAM_ATTR WS2812_Strand::isr(void* arg) {
  auto obj = static_cast<WS2812_Strand*>(arg);
  uint32_t intr_st = RMT.int_st.val;

  if (intr_st & (BIT(obj->rmtChannel + 24))) {
    RMT.int_clr.val = BIT(obj->rmtChannel + 24);
    uint32_t data_sub_len = RMT.tx_lim_ch[obj->rmtChannel].limit / 8;

    if (obj->currentLen > data_sub_len) {
      fillMemoryEncoded(obj->rmtChannel, obj->currentPos, data_sub_len,
          obj->txOffset);
      obj->currentPos += data_sub_len;
      obj->currentLen -= data_sub_len;
    } else if (obj->currentLen == 0) {
      RMTMEM.chan[obj->rmtChannel].data32[obj->txOffset].val = 0;
    } else {
      fillMemoryEncoded(obj->rmtChannel, obj->currentPos, obj->currentLen,
          obj->txOffset);
      RMTMEM.chan[obj->rmtChannel].data32[obj->txOffset + obj->currentLen * 8].val =
          0;
      obj->currentPos += obj->currentLen;
      obj->currentLen = 0;
    }

    if (obj->txOffset == 0) {
      obj->txOffset = data_sub_len * 8;
    } else {
      obj->txOffset = 0;
    }
  }
}

void WS2812_Strand::fill(const RGBColor& color) {
  if (pixelWidth == 3) {
    uint8_t grb[3];
    grb[0] = color.g;
    grb[1] = color.r;
    grb[2] = color.b;
    for (auto i = 0; i < pixelCount; i++) {
      memcpy(buffer + i * 3, &grb, 3);
    }
  } else if (pixelWidth == 4) {
    uint8_t grbw[4];
    auto rgbw = RGBWColor(color);
    grbw[0] = rgbw.g;
    grbw[1] = rgbw.r;
    grbw[2] = rgbw.b;
    grbw[3] = rgbw.w;
    for (auto i = 0; i < pixelCount; i++) {
      memcpy(buffer + i * 4, &grbw, 4);
    }
  }
}

void WS2812_Strand::setPixelColor(uint16_t index, const RGBColor& color) {
  assert(index < pixelCount);
  if (pixelWidth == 3) {
    uint8_t grb[3];
    grb[0] = color.g;
    grb[1] = color.r;
    grb[2] = color.b;
    memcpy(buffer + index * 3, &grb, 3);
  } else if (pixelWidth == 4) {
    uint8_t grbw[4];
    auto rgbw = RGBWColor(color);
    grbw[0] = rgbw.g;
    grbw[1] = rgbw.r;
    grbw[2] = rgbw.b;
    grbw[3] = rgbw.w;
    memcpy(buffer + index * 4, &grbw, 4);
  }
}

void WS2812_Strand::shiftBuffer(uint16_t by) {
  xSemaphoreTake(sem, portMAX_DELAY);
  memmove(buffer + by * pixelWidth, buffer, bufferLen() - by * pixelWidth);
  xSemaphoreGive(sem);
}

WS2812_Strand::~WS2812_Strand() {}

