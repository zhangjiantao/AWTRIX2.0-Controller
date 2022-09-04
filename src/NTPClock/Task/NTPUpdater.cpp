#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, "ntp2.aliyun.com", 8 * 60 * 60, 24 * 60 * 60 * 1000);

class NTPUpdater : public Task {
public:
  bool run(FastLED_NeoMatrix *matrix) override {
    static bool seed = false;
    if (!seed) {
      if (ntp.update()) {
        random16_set_seed(ntp.getEpochTime() % UINT16_MAX);
        seed = true;
        return true;
      }
    }
    return ntp.update();
  }
};

static Registry::RegisterTask<NTPUpdater> X;