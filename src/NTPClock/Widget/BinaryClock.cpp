#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

class BinaryClock : public Widget {
public:
  void render(int x, int y) override {
    auto ntp_h = ntp.getHours();
    auto ntp_m = ntp.getMinutes();
    auto ntp_s = ntp.getSeconds();
    auto r = Color565(255, 0, 0);
    auto g = Color565(0, 255, 0);
    auto b = Color565(0, 0, 255);

    if ((ntp_h / 10) & 1)
      matrix->drawRect(x + 8 + 0, y + 6, 2, 2, r);
    if ((ntp_h / 10) & 2)
      matrix->drawRect(x + 8 + 0, y + 4, 2, 2, r);

    if ((ntp_h % 10) & 1)
      matrix->drawRect(x + 8 + 3, y + 6, 2, 2, r);
    if ((ntp_h % 10) & 2)
      matrix->drawRect(x + 8 + 3, y + 4, 2, 2, r);
    if ((ntp_h % 10) & 4)
      matrix->drawRect(x + 8 + 3, y + 2, 2, 2, r);
    if ((ntp_h % 10) & 8)
      matrix->drawRect(x + 8 + 3, y + 0, 2, 2, r);

    if ((ntp_m / 10) & 1)
      matrix->drawRect(x + 8 + 6, y + 6, 2, 2, g);
    if ((ntp_m / 10) & 2)
      matrix->drawRect(x + 8 + 6, y + 4, 2, 2, g);
    if ((ntp_m / 10) & 4)
      matrix->drawRect(x + 8 + 6, y + 2, 2, 2, g);

    if ((ntp_m % 10) & 1)
      matrix->drawRect(x + 8 + 9, y + 6, 2, 2, g);
    if ((ntp_m % 10) & 2)
      matrix->drawRect(x + 8 + 9, y + 4, 2, 2, g);
    if ((ntp_m % 10) & 4)
      matrix->drawRect(x + 8 + 9, y + 2, 2, 2, g);
    if ((ntp_m % 10) & 8)
      matrix->drawRect(x + 8 + 9, y + 0, 2, 2, g);

    if ((ntp_s / 10) & 1)
      matrix->drawRect(x + 8 + 12, y + 6, 2, 2, b);
    if ((ntp_s / 10) & 2)
      matrix->drawRect(x + 8 + 12, y + 4, 2, 2, b);
    if ((ntp_s / 10) & 4)
      matrix->drawRect(x + 8 + 12, y + 2, 2, 2, b);

    if ((ntp_s % 10) & 1)
      matrix->drawRect(x + 8 + 15, y + 6, 2, 2, b);
    if ((ntp_s % 10) & 2)
      matrix->drawRect(x + 8 + 15, y + 4, 2, 2, b);
    if ((ntp_s % 10) & 4)
      matrix->drawRect(x + 8 + 15, y + 2, 2, 2, b);
    if ((ntp_s % 10) & 8)
      matrix->drawRect(x + 8 + 15, y + 0, 2, 2, b);
  }
};

static Registry::RegisterFullscreenWidget<BinaryClock> X;
