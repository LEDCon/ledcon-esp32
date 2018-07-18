#ifndef MAIN_SERVER_H_
#define MAIN_SERVER_H_

#include <WS2812Strand.h>
#include <WS2812Effects.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include "esp_log.h"

#define SERVICE_UUID     "7b23d9f5-d182-48e3-8c53-de17ff40b479"
#define CHAR_MODE_UUID   "ceac93b8-8b06-444c-b359-4e93b2713d93"
#define CHAR_COLORS_UUID "262e6dfa-ade8-4772-859b-be607ba0f8aa"

static const char TAG_SERVER[] = "LEDCon_Server";
class Server {
public:
  Server(gpio_num_t dinPin, uint16_t pixelCount, uint8_t pixelWidth) {
    auto strand = WS2812_Strand(dinPin, pixelCount, pixelWidth, RMT_CHANNEL_0);
    effects = new WS2812_Effects(strand);

    BLEDevice::init("LEDCon-ESP32");
    BLEServer* pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService(SERVICE_UUID);

    BLECharacteristic* char_mode = pService->createCharacteristic(CHAR_MODE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    char_mode->setCallbacks(new ModeCallback(effects));

    BLECharacteristic* char_colors = pService->createCharacteristic(CHAR_COLORS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    char_colors->setCallbacks(new ColorsCallback(effects));
    pService->start();

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
  }
private:
  class ModeCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) {
      auto value = characteristic->getValue();
      auto mode_id = value[0];
      bool mode_found = false;
      value.erase(0, 1);
      if (mode_id == MODE_RAINBOW) {
        effects->setMode(new RainbowMode(value));
        mode_found = true;
      }
      if (mode_found) {
        ESP_LOGI(TAG_SERVER, "Mode set to %d, starting...", mode_id);
        effects->start();
      }
    }
  private:
    WS2812_Effects* effects;
  public:
    ModeCallback(WS2812_Effects* effects): effects(effects) {}
  };
  class ColorsCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) {
      auto value = characteristic->getValue();
      if (value.size() < 3 || value.size() % 3 != 0) {
        ESP_LOGE(TAG_SERVER, "Invalid color array size: %d", value.size());
        return;
      }
      effects->stop();
      effects->colors.clear();
      for(auto i = 0; i < value.size(); i += 3) {
        ESP_LOGI(TAG_SERVER, "Color added: %d, %d, %d", value[i], value[i+1], value[i+2]);
        effects->colors.push_back(RGBColor(value[i], value[i+1], value[i+2]));
      }
    }
  private:
    WS2812_Effects* effects;
  public:
    ColorsCallback(WS2812_Effects* effects): effects(effects) {}
  };
  WS2812_Effects* effects;
};

#endif /* MAIN_SERVER_H_ */
