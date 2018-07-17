#include <esp_log.h>
#include <string>
#include "sdkconfig.h"

#include <WS2812Strand.h>
#include <WS2812Effects.h>

//#include <BLEDevice.h>
//#include <BLEUtils.h>
//#include <BLEServer.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

static char tag[]="LEDCon_Main";

extern "C" {
  void app_main(void);
}

void app_main(void)
{
  ESP_LOGI(tag, "LEDCon Starting.");
  auto strand = WS2812_Strand(gpio_num_t(5), 18, 4, RMT_CHANNEL_0);
  auto effects = new WS2812_Effects(strand);
//  effects->mode_cycle_init(1000, 1000);
//  effects->colors.push_back(RGBColor(255,0,0));
//  effects->colors.push_back(RGBColor(0,255,0));
//  effects->colors.push_back(RGBColor(0,0,255));
//  effects->colors.push_back(RGBColor(0,0,0));
  effects->colors.push_back(RGBColor(255, 255, 255));
  effects->colors.push_back(RGBColor(0,0,200));
  effects->mode_rainbow_init(1, 10, 1, 10, 500);
  effects->start();
}
