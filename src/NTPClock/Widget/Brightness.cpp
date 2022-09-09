#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>

#include "../NTPClock.h"

#define LDR_PIN A0

static bool AutoBrightness = true;

static bool UpdateBrightness(FastLED_NeoMatrix *matrix, int b) {
  static long CurrBrightness = 0;
  if (std::abs(CurrBrightness - b) < 2)
    return false;
  CurrBrightness = b;
  matrix->setBrightness(CurrBrightness);
  return true;
}

class BrightnessUpdater : public Task {
public:
  bool run(FastLED_NeoMatrix *matrix) override {
    if (!AutoBrightness)
      return false;

    static unsigned long last_update_ms = 0;
    auto ms = millis();
    if (last_update_ms != 0 && ms - last_update_ms < 2000)
      return false;

    last_update_ms = ms;
    auto ldr = analogRead(LDR_PIN);
    auto update = map(ldr > 512 ? 512 : ldr, 0, 512, 4, 70);
    return UpdateBrightness(matrix, update);
  }
};

class Brightness : public Widget {
  uint8_t current = 4; // auto
  uint8_t animation_progress = 0;
  uint8_t br[4] = {5, 27, 49, 70};
  uint8_t frame[4][12] = {
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00},
      {0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x2a, 0x00, 0x08, 0x00, 0x00, 0x00},
      {0x00, 0x00, 0x00, 0x08, 0x22, 0x08, 0x5d, 0x08, 0x22, 0x08, 0x00, 0x00},
  };

public:
  void loop(FastLED_NeoMatrix *matrix) override {
    static unsigned frame_delay = 10;
    static unsigned _delay = 0;
    _delay = ++_delay % (frame_delay + 1);
    if (_delay == 0) {
      if (AutoBrightness) {
        static int step = 1;
        auto next = animation_progress + step;
        if (next < 0 || next > 3)
          step *= -1;
        animation_progress += step;
        return;
      }

      UpdateBrightness(matrix, br[current]);
      animation_progress = current;
    }
  }

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    bool reverse = x != 0;
    auto c = Color565(255, 255, 0);
    for (int col = 0; col < 12; col++) {
      auto val = frame[animation_progress][col];
      for (int ln = 0; ln < 8; ln++) {
        if ((val >> (7 - ln)) & 1) {
          if (reverse)
            matrix->drawPixel(x + (13 - col), y + ln, c);
          else
            matrix->drawPixel(x + col, y + ln, c);
        }
      }
    }
  }

  bool event1(FastLED_NeoMatrix *matrix) override {
    current = ++current % 5;
    AutoBrightness = current == 4;
    return true;
  }
};

static Registry::RegisterWidget<Brightness> X;
static Registry::RegisterTask<BrightnessUpdater> Y;