#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, "ntp2.aliyun.com", 8 * 60 * 60, 1 * 60 * 60 * 1000);

unsigned long startup_time = 0;

class NTPUpdater : public Task {
public:
  bool run() override {
    static bool seed = false;
    if (!seed) {
      if (ntp.update()) {
        startup_time = startup_time ? startup_time : ntp.getEpochTime();
        random16_set_seed(ntp.getEpochTime() % UINT16_MAX);
        seed = true;
        return true;
      }
    }
    return WiFi.status() == WL_CONNECTED && ntp.update();
  }
};

static Registry::RegisterTask<NTPUpdater> X;