#include "WS2812Effects.h"
#include "esp_log.h"

static char TAG[]="Effects";
WS2812_Effects::~WS2812_Effects() {}

void WS2812_Effects::mode_cycle_init(uint32_t time_per_color, uint32_t transition_time) {
  auto runtime = (cycle_runtime*)malloc(sizeof(cycle_runtime));
  runtime->time_per_color = time_per_color;
  runtime->transition_time = transition_time;
  runtime->color_idx = 0;
  runtime->state = cycle_state::Static;
  mode_run = &WS2812_Effects::mode_cycle_run;
  mode_runtime = runtime;
}

uint16_t WS2812_Effects::mode_cycle_run() {
  auto runtime = (cycle_runtime*)mode_runtime;

  if (runtime->state == cycle_state::Static) {
    auto color = colors[runtime->color_idx];
    strand.fill(color);
    strand.show();
    runtime->from = color;
    runtime->to = colors[nextColorIndex(runtime->color_idx)];
    runtime->state = cycle_state::TransitionStart;
    return runtime->time_per_color;
  }

  if (runtime->state == cycle_state::TransitionStart) {
    if (runtime->transition_time == 0) {
      runtime->color_idx = nextColorIndex(runtime->color_idx);
      runtime->state = cycle_state::Static;
    } else {
      runtime->state = cycle_state::TransitionRunning;
      runtime->transition_start_time = currentTime();
    }
  }

  if (runtime->state == cycle_state::TransitionRunning) {
    uint32_t current = currentTime();
    float factor = (current - runtime->transition_start_time) / (float)runtime->transition_time;

    if (factor >= 1) {
      runtime->color_idx = nextColorIndex(runtime->color_idx);
      runtime->state = cycle_state::Static;
    } else {
      auto color = RGBColor::linearBlend(runtime->from, runtime->to, factor);
      strand.fill(color);
      strand.show();
    }
  }

  return DELAY_MS;
}

void WS2812_Effects::mode_rainbow_init(uint8_t color_gap_min, uint8_t color_gap_max, uint8_t color_len_min, uint8_t color_len_max, uint16_t speed) {
  if (colors.size() < 2) { // No enough color for a rainbow.
    ESP_LOGW(TAG, "colors.size() = %d, too few for a rainbow!", colors.size());
    mode_cycle_init(100000, 0);
    return;
  }
  auto runtime = (rainbow_runtime*)malloc(sizeof(rainbow_runtime));
  assert(color_gap_min <= color_gap_max);
  runtime->color_gap_min = CLAMP(color_gap_min, 1, 255);
  runtime->color_gap_max = CLAMP(color_gap_max, 1, 255);
  assert(color_len_min <= color_len_max);
  runtime->color_len_min = CLAMP(color_len_min, 1, 255);
  runtime->color_len_max = CLAMP(color_len_max, 1, 255);
  runtime->speed = speed > DELAY_MS ? speed : DELAY_MS;
  runtime->current_color_idx = 0;

  mode_run = &WS2812_Effects::mode_rainbow_run;
  mode_runtime = runtime;
}

RGBColor WS2812_Effects::mode_rainbow_next_pixel(rainbow_runtime* runtime) {
  if (runtime->current_color_len == 0) {
    if (runtime->current_color_gap == 0) {
      // Time for a new color!
      runtime->current_color_idx = runtime->next_color_idx;
      runtime->next_color_idx = nextColorIndex(runtime->current_color_idx);
      uint32_t rand_val = esp_random();
      uint8_t* rand_arr = (uint8_t*)&rand_val;

      if (runtime->color_gap_min == runtime->color_gap_max) {
        runtime->current_color_gap = runtime->color_gap_min;
      } else {
        uint8_t diff = runtime->color_gap_max - runtime->color_gap_min;
        runtime->current_color_gap = runtime->color_gap_min + ((diff * rand_arr[0]) >> 8);
        runtime->current_color_gap_full = runtime->current_color_gap;
      }

      if (runtime->color_len_min == runtime->color_len_max) {
        runtime->current_color_len = runtime->color_len_min;
      } else {
        uint8_t diff = runtime->color_len_max - runtime->color_len_min;
        runtime->current_color_len = runtime->color_len_min + ((diff * rand_arr[1]) >> 8);
      }
      ESP_LOGI(TAG, "Rainbow new color, gap=%d, len = %d", runtime->current_color_gap_full, runtime->current_color_len);

      runtime->current_color_len--;
      return colors[runtime->current_color_idx];
    } else {
      float progress = (runtime->current_color_gap_full - runtime->current_color_gap) / (float)runtime->current_color_gap_full;
      runtime->current_color_gap--;
      return RGBColor::linearBlend(colors[runtime->current_color_idx], colors[runtime->next_color_idx], progress);
    }
  }

  runtime->current_color_len--;
  return colors[runtime->current_color_idx];
}

uint16_t WS2812_Effects::mode_rainbow_run() {
  auto runtime = (rainbow_runtime*)mode_runtime;
  if (first_run) {
    for (auto i = 0; i < strand.pixelCount; i++) {
      strand.setPixelColor(i, mode_rainbow_next_pixel(runtime));
    }
  } else {
    strand.shiftBuffer(1);
    strand.setPixelColor(0, mode_rainbow_next_pixel(runtime));
  }
  strand.show();
  return runtime->speed;
}

