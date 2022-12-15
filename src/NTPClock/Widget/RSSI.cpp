#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <deque>
#include <list>

#include "../NTPClock.h"

template <uint8_t h, uint8_t w, bool fullscreen = false>
class RSSI : public Widget {
  uint8_t r[h]{0};
  uint8_t g[h]{0};

  bool connected = false;
  int connecting = 0;
  int last_sec = 0;
  std::deque<int> rssi;

public:
  RSSI() {
    for (uint8_t i = 0; i < h; i++) {
      auto c = map(i, 0, h - 1, 0, 256 * 2 - 2);
      if (c < 256) {
        g[h - i - 1] = 255;
        r[h - i - 1] = c;
      } else {
        g[h - i - 1] = 256 * 2 - c;
        r[h - i - 1] = 255;
      }
    }
  }

  void loop() override {
    auto s = ntp.getSeconds();
    if (s == last_sec)
      return;

    last_sec = s;
    connected = WiFi.status() == WL_CONNECTED;
    if (!connected) {
      connecting = (connecting + 1) % 3;
      return;
    }

    if (rssi.size() == w)
      rssi.pop_front();

    auto i = wifi_station_get_rssi();
    auto k = i < -80 ? -80 : i;
    k = k > -40 ? -40 : k;
    k += 80;
    auto val = map(k, 0, 40, 1, h);
    rssi.push_back(val);
  }

  void render(int x, int y) override {
    // draw border
    auto c = Color565(130, 130, 130);
    matrix->drawRect(x + 1 - fullscreen, y + 1 - fullscreen, w + 2, h + 2, c);

    auto yellow = Color565(255, 255, 0);

    if (!connected) {
      auto pos_x = x + 1 + w / 2 - fullscreen;
      auto pos_y = y + 1 + h / 2 + 1 - fullscreen;
      matrix->drawPixel(pos_x, pos_y, yellow);
      matrix->drawPixel(pos_x + 1, pos_y, yellow);
      for (int i = 0; i < connecting; i++) {
        matrix->drawPixel(pos_x - (i + 1), pos_y, yellow);
        matrix->drawPixel(pos_x + 1 + (i + 1), pos_y, yellow);
      }
      return;
    }

    // draw rssi
    for (int i = 0; i < w; i++) {
      auto val = rssi[i];
      c = Color565(r[val - 1], g[val - 1], 0);
      for (int j = 0; j < h; j++) {
        if (j < val) {
          auto pos_x = x + 2 + i - fullscreen;
          auto pos_y = y + h + 1 - j - fullscreen;
          matrix->drawPixel(pos_x, pos_y, c);
        }
      }
    }
  }

  bool event1() override {
    last_sec = 61;
    return false;
  }
};

static Registry::RegisterWidget<RSSI<5, 10>> X;
static Registry::RegisterFullscreenWidget<RSSI<6, 30, true>> Y;