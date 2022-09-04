#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

template <bool fullscreen = false> class DigitalHMClock : public Widget {
  uint8_t last_hh = 0;
  uint8_t last_hl = 0;
  uint8_t last_mh = 0;
  uint8_t last_ml = 0;
  uint8_t ani_hh = 0;
  uint8_t ani_hl = 0;
  uint8_t ani_mh = 0;
  uint8_t ani_ml = 0;
  bool first_start = true;

  uint8_t last_h = 0;

  uint8_t animation_progress = 0;

  uint8_t r = 0;
  uint8_t g = 255;
  uint8_t b = 64;

public:
  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    uint8_t offset = fullscreen ? 7 : 0;

    auto h = ntp.getHours();
    auto m = ntp.getMinutes();

    auto hh = h / 10;
    auto hl = h % 10;
    auto mh = m / 10;
    auto ml = m % 10;

    auto black = matrix->Color(0, 0, 0);
    if (fullscreen) {
      matrix->fillRect(x, y, 32, 8, black);
    } else {
      matrix->fillRect(x, y, 19, 8, black);
    }

    matrix->setTextColor(matrix->Color(r, g, b));
    char buf[10];
    auto ntp_s = ntp.getSeconds();
    auto ntp_d = ntp.getDay();
    auto blink = ntp_s % 2 ? ":" : " ";
    sprintf(buf, "%02d%s%02d", last_hh * 10 + last_hl, blink, last_mh * 10 + last_ml);
    matrix->setCursor(x + offset + 1, y + 6);
    matrix->print(buf);

    if (ml != last_ml || mh != last_mh || hl != last_hl || hh != last_hh) {
      ani_ml = last_ml;
      ani_mh = last_mh;
      ani_hl = last_hl;
      ani_hh = last_hh;
      last_ml = ml;
      last_mh = mh;
      last_hl = hl;
      last_hh = hh;
      animation_progress = 18;
    }

    if (animation_progress || first_start) {
      if (ml != ani_ml || first_start) {
        matrix->fillRect(x + offset + 15, y + 0, 3, 8, black);
        matrix->setCursor(x + offset + 15,
                          y + (6 - animation_progress / 3) + 6);
        matrix->printf("%d", ani_ml);
        matrix->setCursor(x + offset + 15, y + (6 - animation_progress / 3));
        matrix->printf("%d", ml);
        matrix->drawRect(x + offset + 15, y + 0, 3, 1, black);
        matrix->drawRect(x + offset + 15, y + 6, 3, 2, black);
      }

      if (mh != ani_mh || first_start) {
        matrix->fillRect(x + offset + 11, y + 0, 3, 8, black);
        matrix->setCursor(x + offset + 11,
                          y + (6 - animation_progress / 3) + 6);
        matrix->printf("%d", ani_mh);
        matrix->setCursor(x + offset + 11, y + (6 - animation_progress / 3));
        matrix->printf("%d", mh);
        matrix->drawRect(x + offset + 11, y + 0, 3, 1, black);
        matrix->drawRect(x + offset + 11, y + 6, 3, 2, black);
      }

      if (hl != ani_hl || first_start) {
        matrix->fillRect(x + offset + 5, y + 0, 3, 8, black);
        matrix->setCursor(x + offset + 5, y + (6 - animation_progress / 3) + 6);
        matrix->printf("%d", ani_hl);
        matrix->setCursor(x + offset + 5, y + (6 - animation_progress / 3));
        matrix->printf("%d", hl);
        matrix->drawRect(x + offset + 5, y + 0, 3, 1, black);
        matrix->drawRect(x + offset + 5, y + 6, 3, 2, black);
      }

      if (hh != ani_hh || first_start) {
        matrix->fillRect(x + offset + 1, y + 0, 3, 8, black);
        matrix->setCursor(x + offset + 1, y + (6 - animation_progress / 3) + 6);
        matrix->printf("%d", ani_hh);
        matrix->setCursor(x + offset + 1, y + (6 - animation_progress / 3));
        matrix->printf("%d", hh);
        matrix->drawRect(x + offset + 1, y + 0, 3, 1, black);
        matrix->drawRect(x + offset + 1, y + 6, 3, 2, black);
      }

      animation_progress--;

      if (animation_progress == 0) {
        first_start = false;
        if (h != last_h) {
          RANDOM_RGB(r, g, b);
          dfmp3.playAdvertisement(0);
          last_h = h;
        }
      }
    }

    auto week_f = matrix->Color(230, 230, 230);
    auto week_b = matrix->Color(130, 130, 130);
    if (fullscreen) {
      ntp_d = ntp_d ? ntp_d : 7;
      for (int i = 1; i < 8; i++) {
        auto color = ntp_d == i ? week_f : week_b;
        auto x0 = x + 3 + (i - 1) * 4;
        matrix->drawLine(x0, y + 7, x0 + 2, y + 7, color);
      }
    } else {
      for (int i = 1; i < 7; i++) {
        auto color = ntp_d == i ? week_f : week_b;
        auto x0 = x + 1 + (i - 1) * 3;
        matrix->drawLine(x0, y + 7, x0 + 1, y + 7, color);
      }
    }
  }

  bool event1(FastLED_NeoMatrix *matrix) override {
    RANDOM_RGB(r, g, b);
    return true;
  }
};

Widget *Registry::GetMainClock() {
  static Widget *c = nullptr;
  if (!c)
    c = new DigitalHMClock<>();
  return c;
}

static Registry::RegisterFullscreenWidget<DigitalHMClock<true>> X;
