#include "WS2812Effects.h"

void WS2812_Effects::timerCallback(TimerHandle_t xTimer) {
    auto obj = (WS2812_Effects*)pvTimerGetTimerID(xTimer);
    auto should_run = obj->mode != nullptr && (*obj).elapsedTime() >= obj->delay;
    if (should_run || obj->first_run) {
      obj->delay = obj->mode->run(*obj);
      obj->last_run_time = obj->currentTime();
    }
}

void WS2812_Effects::setMode(EffectMode* mode) {
  stop();
  delete this->mode;
  this->mode = mode;
}
