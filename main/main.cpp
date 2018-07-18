#include <esp_log.h>
#include <string>
#include "sdkconfig.h"

#include <WS2812Strand.h>
#include <WS2812Effects.h>
#include "Server.h"

extern "C" {
  void app_main(void);
}

void app_main(void)
{
  ESP_LOGI("LEDCon_Main", "LEDCon Starting.");
  auto server = new Server(gpio_num_t(13), 18, 4);
}
