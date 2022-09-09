#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

template <uint8_t h, uint8_t w> class Spectrumer : public Widget {
  int8_t value[w]{0};
  uint8_t r[h]{0};
  uint8_t g[h]{0};

  unsigned frame_delay = 4;
  unsigned _delay = 0;

public:
  Spectrumer() {
    for (uint8_t i = 0; i < h; i++) {
      auto c = map(i, 0, h - 1, 0, 256 * 2 - 2);
      if (c < 256) {
        g[i] = 255;
        r[i] = c;
      } else {
        g[i] = 256 * 2 - c;
        r[i] = 255;
      }
    }
  }

  void loop(FastLED_NeoMatrix *matrix) override {
    _delay = ++_delay % (frame_delay + 1);
    if (_delay == 0) {
      for (uint8_t i = 0; i < w; i++) {
        int8_t r8 = (int8_t)random8(3) - 1;
        value[i] += r8;
        if (value[i] > h || value[i] < 0)
          value[i] -= r8;
      }
    }
  }

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    // draw border
    auto c = matrix->Color(130, 130, 130);
    matrix->drawRect(x + 1, y + 1, w + 2, h + 2, c);

    // draw spectrumer
    for (int i = 0; i < w; i++) {
      for (int j = 0; j < h; j++)
        if (j < value[i])
          matrix->drawPixel(x + 2 + i, y + 6 - j, matrix->Color(r[j], g[j], 0));
    }
  }

  bool event1(FastLED_NeoMatrix *matrix) override {
    frame_delay = ++frame_delay % 8;
    return false;
  }
};

// static Registry::RegisterWidget<Spectrumer<5, 10>> X;
// static Registry::RegisterFullscreenWidget<Spectrumer<6, 30>> Y;