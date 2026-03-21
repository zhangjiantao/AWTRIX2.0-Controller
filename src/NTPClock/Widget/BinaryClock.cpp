#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

class BinaryClock : public Widget {
  int r = Color565(0, 255, 64);
  int e = Color565(0, 0, 0);

public:
  void render(int x, int y) override {
    auto ntp_h = ntp.getHours();
    auto ntp_m = ntp.getMinutes();
    auto ntp_s = ntp.getSeconds();

    matrix->drawRect(x + 5 + 0, y + 6, 2, 2, ((ntp_h / 10) & 1) ? r : e);
    matrix->drawRect(x + 5 + 0, y + 4, 2, 2, ((ntp_h / 10) & 2) ? r : e);
    matrix->drawRect(x + 5 + 0, y + 2, 2, 2, e);
    matrix->drawRect(x + 5 + 0, y + 0, 2, 2, e);

    matrix->drawRect(x + 5 + 3, y + 6, 2, 2, ((ntp_h % 10) & 1) ? r : e);
    matrix->drawRect(x + 5 + 3, y + 4, 2, 2, ((ntp_h % 10) & 2) ? r : e);
    matrix->drawRect(x + 5 + 3, y + 2, 2, 2, ((ntp_h % 10) & 4) ? r : e);
    matrix->drawRect(x + 5 + 3, y + 0, 2, 2, ((ntp_h % 10) & 8) ? r : e);

    matrix->drawRect(x + 8 + 6, y + 6, 2, 2, ((ntp_m / 10) & 1) ? r : e);
    matrix->drawRect(x + 8 + 6, y + 4, 2, 2, ((ntp_m / 10) & 2) ? r : e);
    matrix->drawRect(x + 8 + 6, y + 2, 2, 2, ((ntp_m / 10) & 4) ? r : e);
    matrix->drawRect(x + 8 + 6, y + 0, 2, 2, e);

    matrix->drawRect(x + 8 + 9, y + 6, 2, 2, ((ntp_m % 10) & 1) ? r : e);
    matrix->drawRect(x + 8 + 9, y + 4, 2, 2, ((ntp_m % 10) & 2) ? r : e);
    matrix->drawRect(x + 8 + 9, y + 2, 2, 2, ((ntp_m % 10) & 4) ? r : e);
    matrix->drawRect(x + 8 + 9, y + 0, 2, 2, ((ntp_m % 10) & 8) ? r : e);

    matrix->drawRect(x + 11 + 12, y + 6, 2, 2, ((ntp_s / 10) & 1) ? r : e);
    matrix->drawRect(x + 11 + 12, y + 4, 2, 2, ((ntp_s / 10) & 2) ? r : e);
    matrix->drawRect(x + 11 + 12, y + 2, 2, 2, ((ntp_s / 10) & 4) ? r : e);
    matrix->drawRect(x + 11 + 12, y + 0, 2, 2, e);

    matrix->drawRect(x + 11 + 15, y + 6, 2, 2, ((ntp_s % 10) & 1) ? r : e);
    matrix->drawRect(x + 11 + 15, y + 4, 2, 2, ((ntp_s % 10) & 2) ? r : e);
    matrix->drawRect(x + 11 + 15, y + 2, 2, 2, ((ntp_s % 10) & 4) ? r : e);
    matrix->drawRect(x + 11 + 15, y + 0, 2, 2, ((ntp_s % 10) & 8) ? r : e);
  }

  bool event1() override {
    int a, b, c;
    RANDOM_RGB(a, b, c);
    r = Color565(a, b, c);
    return false;
  }
};

static Registry::RegisterFullscreenWidget<BinaryClock> X;
