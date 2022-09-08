#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

class HMSDigitalClock : public Widget {
  uint8_t last_hh = 0;
  uint8_t last_hl = 0;
  uint8_t last_mh = 0;
  uint8_t last_ml = 0;
  uint8_t last_sh = 0;
  uint8_t last_sl = 0;
  uint8_t ani_hh = 0;
  uint8_t ani_hl = 0;
  uint8_t ani_mh = 0;
  uint8_t ani_ml = 0;
  uint8_t ani_sh = 0;
  uint8_t ani_sl = 0;
  bool first_start = true;
  uint8_t animation_progress = 0;
  uint8_t ani_speed = 6;

  uint8_t r = 0;
  uint8_t g = 255;
  uint8_t b = 64;
  uint16_t last_h = 0;

public:
  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    auto h = ntp.getHours();
    auto m = ntp.getMinutes();
    auto s = ntp.getSeconds();

    auto black = matrix->Color(0, 0, 0);
    matrix->fillRect(x, y, 32, 8, black);
    matrix->setTextColor(matrix->Color(r, g, b));

    char buf[10];
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    matrix->setCursor(x + 3, y + 6);
    matrix->print(buf);
    if (ani_speed) {
      auto hh = h / 10;
      auto hl = h % 10;
      auto mh = m / 10;
      auto ml = m % 10;
      auto sh = s / 10;
      auto sl = s % 10;

      if (sl != last_sl || sh != last_sh || ml != last_ml || mh != last_mh ||
          hl != last_hl || hh != last_hh) {
        ani_sl = last_sl;
        ani_sh = last_sh;
        ani_ml = last_ml;
        ani_mh = last_mh;
        ani_hl = last_hl;
        ani_hh = last_hh;
        last_sl = sl;
        last_sh = sh;
        last_ml = ml;
        last_mh = mh;
        last_hl = hl;
        last_hh = hh;
        animation_progress = ani_speed;
      }

      if (animation_progress || first_start) {
        if (sl != ani_sl || first_start) {
          matrix->fillRect(x + 27, y + 0, 3, 8, black);
          matrix->setCursor(x + 27,
                            y + (6 - animation_progress / (ani_speed / 6)) + 6);
          matrix->printf("%d", ani_sl);
          matrix->setCursor(x + 27,
                            y + (6 - animation_progress / (ani_speed / 6)));
          matrix->printf("%d", sl);
          matrix->drawRect(x + 27, y + 0, 3, 1, black);
          matrix->drawRect(x + 27, y + 6, 3, 2, black);
        }

        if (sh != ani_sh || first_start) {
          matrix->fillRect(x + 23, y + 0, 3, 8, black);
          matrix->setCursor(x + 23,
                            y + (6 - animation_progress / (ani_speed / 6)) + 6);
          matrix->printf("%d", ani_sh);
          matrix->setCursor(x + 23,
                            y + (6 - animation_progress / (ani_speed / 6)));
          matrix->printf("%d", sh);
          matrix->drawRect(x + 23, y + 0, 3, 1, black);
          matrix->drawRect(x + 23, y + 6, 3, 2, black);
        }

        if (ml != ani_ml || first_start) {
          matrix->fillRect(x + 17, y + 0, 3, 8, black);
          matrix->setCursor(x + 17,
                            y + (6 - animation_progress / (ani_speed / 6)) + 6);
          matrix->printf("%d", ani_ml);
          matrix->setCursor(x + 17,
                            y + (6 - animation_progress / (ani_speed / 6)));
          matrix->printf("%d", ml);
          matrix->drawRect(x + 17, y + 0, 3, 1, black);
          matrix->drawRect(x + 17, y + 6, 3, 2, black);
        }

        if (mh != ani_mh || first_start) {
          matrix->fillRect(x + 13, y + 0, 3, 8, black);
          matrix->setCursor(x + 13,
                            y + (6 - animation_progress / (ani_speed / 6)) + 6);
          matrix->printf("%d", ani_mh);
          matrix->setCursor(x + 13,
                            y + (6 - animation_progress / (ani_speed / 6)));
          matrix->printf("%d", mh);
          matrix->drawRect(x + 13, y + 0, 3, 1, black);
          matrix->drawRect(x + 13, y + 6, 3, 2, black);
        }

        if (hl != ani_hl || first_start) {
          matrix->fillRect(x + 7, y + 0, 3, 8, black);
          matrix->setCursor(x + 7,
                            y + (6 - animation_progress / (ani_speed / 6)) + 6);
          matrix->printf("%d", ani_hl);
          matrix->setCursor(x + 7,
                            y + (6 - animation_progress / (ani_speed / 6)));
          matrix->printf("%d", hl);
          matrix->drawRect(x + 7, y + 0, 3, 1, black);
          matrix->drawRect(x + 7, y + 6, 3, 2, black);
        }

        if (hh != ani_hh || first_start) {
          matrix->fillRect(x + 3, y + 0, 3, 8, black);
          matrix->setCursor(x + 3,
                            y + (6 - animation_progress / (ani_speed / 6)) + 6);
          matrix->printf("%d", ani_hh);
          matrix->setCursor(x + 3,
                            y + (6 - animation_progress / (ani_speed / 6)));
          matrix->printf("%d", hh);
          matrix->drawRect(x + 3, y + 0, 3, 1, black);
          matrix->drawRect(x + 3, y + 6, 3, 2, black);
        }

        animation_progress--;
        if (animation_progress == 0)
          first_start = false;
      }
    }

    if (last_h != h && m == 0 && s == 0) {
      RANDOM_RGB(r, g, b);
      dfmp3.playAdvertisement(0);
      last_h = h;
    }

    auto week_f = matrix->Color(230, 230, 230);
    auto week_b = matrix->Color(130, 130, 130);
    auto ntp_d = ntp.getDay() ? ntp.getDay() : 7;
    for (int i = 1; i < 8; i++) {
      auto color = ntp_d == i ? week_f : week_b;
      auto x0 = x + 3 + (i - 1) * 4;
      matrix->drawLine(x0, y + 7, x0 + 2, y + 7, color);
    }
  }

  bool event1(FastLED_NeoMatrix *matrix) override {
    ani_speed = ani_speed == 24 ? 0 : ani_speed + 6;
    RANDOM_RGB(r, g, b);
    return true;
  }
};

static Registry::RegisterFullscreenWidget<HMSDigitalClock> X;