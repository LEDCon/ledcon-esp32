/*
 * WS2812Effects.h
 *
 *  Created on: Jul 13, 2018
 *      Author: harry
 */

#ifndef COMPONENTS_WS2812_WS2812EFFECTS_H_
#define COMPONENTS_WS2812_WS2812EFFECTS_H_

#include "WS2812Strand.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <vector>
#include <string>
#include "esp_log.h"

#define FPS 40
#define DELAY_MS (1000/FPS)

#define MODE_CYCLE 0
#define MODE_RAINBOW 1

static char TAG[]="Effects";

class EffectMode;

class WS2812_Effects {
public:
  WS2812_Effects(WS2812_Strand strand) :
      strand(strand) {
    this->timer = xTimerCreate("WS2812_EFFECTS", pdMS_TO_TICKS(DELAY_MS), true,
        this, timerCallback);
    assert(this->timer != nullptr);
  }
  void start() {
    first_run = true;
    timerCallback(this->timer);
    first_run = false;
    xTimerStart(this->timer, portMAX_DELAY);
    ESP_LOGI(TAG, "Started");
  }
  void stop() {
    xTimerStop(timer, portMAX_DELAY);
    ESP_LOGI(TAG, "Stopped");
  }
  static void timerCallback(TimerHandle_t xTimer);
  inline uint32_t currentTime() {
    return esp_timer_get_time() / 1000;
  }
  inline uint32_t elapsedTime() {
    return currentTime() - last_run_time;
  }
  void setMode(EffectMode* mode);
  uint8_t nextColorIndex(uint8_t idx) {
    auto new_idx = idx + 1;
    if (new_idx >= colors.size()) {
      return 0;
    }
    return new_idx;
  }
  ~WS2812_Effects() {};
  std::vector<RGBColor> colors;
  WS2812_Strand strand;
  bool first_run = false;

private:
  TimerHandle_t timer;
  EffectMode* mode = nullptr;
  uint32_t last_run_time;
  uint16_t delay = 0;
};

class EffectMode {
public:
  virtual uint16_t run(WS2812_Effects&) = 0;
  virtual ~EffectMode() {};
};

class CycleMode: EffectMode {
public:
  CycleMode(uint32_t time_per_color, uint32_t transition_time): time_per_color(time_per_color), transition_time(transition_time) {}
  uint16_t run(WS2812_Effects& effects) {
    if (state == cycle_state::Static) {
      auto color = effects.colors[color_idx];
      effects.strand.fill(color);
      effects.strand.show();
      from = color;
      to = effects.colors[effects.nextColorIndex(color_idx)];
      state = cycle_state::TransitionStart;
      return time_per_color;
    }

    if (state == cycle_state::TransitionStart) {
      if (transition_time == 0) {
        color_idx = effects.nextColorIndex(color_idx);
        state = cycle_state::Static;
      } else {
        state = cycle_state::TransitionRunning;
        transition_start_time = effects.currentTime();
      }
    }

    if (state == cycle_state::TransitionRunning) {
      uint32_t current = effects.currentTime();
      float factor = (current - transition_start_time) / (float)transition_time;

      if (factor >= 1) {
        color_idx = effects.nextColorIndex(color_idx);
        state = cycle_state::Static;
      } else {
        auto color = RGBColor::linearBlend(from, to, factor);
        effects.strand.fill(color);
        effects.strand.show();
      }
    }

    return DELAY_MS;
  }
private:
  typedef enum {
    Static,
    TransitionStart,
    TransitionRunning
  } cycle_state;
  // Args
  uint32_t time_per_color;
  uint32_t transition_time;
  // !Args
  uint8_t color_idx = 0;
  RGBColor from;
  RGBColor to;
  cycle_state state = cycle_state::Static;
  uint32_t transition_start_time = 0;
};

class RainbowMode: public EffectMode {
public:
  RainbowMode(uint8_t color_gap_min, uint8_t color_gap_max, uint8_t color_len_min, uint8_t color_len_max, uint16_t speed) {
    this->color_gap_min = MIN(color_gap_min, color_gap_max);
    this->color_gap_max = MAX(color_gap_min, color_gap_max);
    this->color_len_min = CLAMP(MIN(color_len_min, color_len_max), 1, 255);
    this->color_len_max = CLAMP(MAX(color_len_min, color_len_max), 1, 255);
    this->speed = MIN(DELAY_MS, speed);
  }
  RainbowMode(const std::string& input) {
    uint8_t color_gap_min = input[0];
    uint8_t color_gap_max = input[1];
    uint8_t color_len_min = input[2];
    uint8_t color_len_max = input[3];
    uint16_t speed = (uint16_t)input[5] | (uint16_t)input[4] << 8;
    this->color_gap_min = MIN(color_gap_min, color_gap_max);
    this->color_gap_max = MAX(color_gap_min, color_gap_max);
    this->color_len_min = CLAMP(MIN(color_len_min, color_len_max), 1, 255);
    this->color_len_max = CLAMP(MAX(color_len_min, color_len_max), 1, 255);
    this->speed = MIN(DELAY_MS, speed);
  }
  uint16_t run(WS2812_Effects& effects) {
    if (effects.first_run) {
      for (auto i = 0; i < effects.strand.pixelCount; i++) {
        effects.strand.setPixelColor(i, nextPixel(effects));
      }
    } else {
      effects.strand.shiftBuffer();
      effects.strand.setPixelColor(0, nextPixel(effects));
    }
    effects.strand.show();
    return speed;
  }
  ~RainbowMode() {}
private:
  RGBColor nextPixel(WS2812_Effects& effects) {
    if (current_color_len == 0) {
      if (current_color_gap == 0) {
        // Time for a new color!
        current_color_idx = next_color_idx;
        next_color_idx = effects.nextColorIndex(current_color_idx);
        uint32_t rand_val = esp_random();
        uint8_t* rand_arr = (uint8_t*)&rand_val;

        if (color_gap_min == color_gap_max) {
          current_color_gap = color_gap_min;
        } else {
          uint8_t diff = color_gap_max - color_gap_min;
          current_color_gap = color_gap_min + ((diff * rand_arr[0]) >> 8);
          current_color_gap_full = current_color_gap;
        }

        if (color_len_min == color_len_max) {
          current_color_len = color_len_min;
        } else {
          uint8_t diff = color_len_max - color_len_min;
          current_color_len = color_len_min + ((diff * rand_arr[1]) >> 8);
        }
        ESP_LOGI(TAG, "Rainbow new color, gap=%d, len = %d", current_color_gap_full, current_color_len);

        current_color_len--;
        return effects.colors[current_color_idx];
      } else {
        float progress = (current_color_gap_full - current_color_gap) / (float)current_color_gap_full;
        current_color_gap--;
        return RGBColor::linearBlend(effects.colors[current_color_idx], effects.colors[next_color_idx], progress);
      }
    }

    --current_color_len;
    return effects.colors[current_color_idx];
  }
  // Args
  uint8_t color_gap_min;
  uint8_t color_gap_max;
  uint8_t color_len_min;
  uint8_t color_len_max;
  uint16_t speed; // ms per pixel
  // !Args
  uint8_t current_color_idx = 0;
  uint8_t next_color_idx = 0;
  uint8_t current_color_len = 0; // Static color first
  uint8_t current_color_gap = 0; // Gradient follows, when both value reaches 0, switch color index and start over.
  uint8_t current_color_gap_full = 0;
};

#endif /* COMPONENTS_WS2812_WS2812EFFECTS_H_ */
