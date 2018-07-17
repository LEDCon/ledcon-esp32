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

#define FPS 40
#define DELAY_MS (1000/FPS)

class WS2812_Effects {
  typedef uint16_t (WS2812_Effects::*mode_run_ptr)();
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
  }
  static void timerCallback(TimerHandle_t xTimer) {
    auto obj = (WS2812_Effects*)pvTimerGetTimerID(xTimer);
    auto should_run = obj->mode_run != nullptr && (*obj).elapsedTime() >= obj->delay;
    if (should_run || obj->first_run) {
      obj->delay = (obj->*obj->mode_run)();
      obj->last_run_time = obj->currentTime();
    }
  }
  inline uint32_t currentTime() {
    return esp_timer_get_time() / 1000;
  }
  inline uint32_t elapsedTime() {
    return currentTime() - last_run_time;
  }
  virtual ~WS2812_Effects();
  std::vector<RGBColor> colors;
private:
  WS2812_Strand strand;
  TimerHandle_t timer;
  mode_run_ptr mode_run = nullptr;
  void* mode_runtime = nullptr;
  uint32_t last_run_time;
  uint16_t delay = 0;
  bool first_run = false;
  uint8_t nextColorIndex(uint8_t idx) {
    auto new_idx = idx + 1;
    if (new_idx >= colors.size()) {
      return 0;
    }
    return new_idx;
  }
public:
  // === Cycle ===
  typedef enum {
    Static,
    TransitionStart,
    TransitionRunning
  } cycle_state;
  typedef struct {
    // Args
    uint32_t time_per_color;
    uint32_t transition_time;
    // !Args
    uint8_t color_idx;
    RGBColor from;
    RGBColor to;
    cycle_state state;
    uint32_t transition_start_time;
  } cycle_runtime;
  uint16_t mode_cycle_run();
  void mode_cycle_init(uint32_t time_per_color, uint32_t transition_time);

  // === Rainbow ===
  typedef struct {
    // Args
    uint8_t color_gap_min;
    uint8_t color_gap_max;
    uint8_t color_len_min;
    uint8_t color_len_max;
    uint16_t speed; // ms per pixel
    // !Args
    uint8_t current_color_idx;
    uint8_t next_color_idx;
    uint8_t current_color_len; // Static color first
    uint8_t current_color_gap; // Gradient follows, when both value reaches 0, switch color index and start over.
    uint8_t current_color_gap_full;
  } rainbow_runtime;
  uint16_t mode_rainbow_run();
  void mode_rainbow_init(uint8_t color_gap_min, uint8_t color_gap_max, uint8_t color_len_min, uint8_t color_len_max, uint16_t speed);
  RGBColor mode_rainbow_next_pixel(rainbow_runtime* runtime);
};

#endif /* COMPONENTS_WS2812_WS2812EFFECTS_H_ */
