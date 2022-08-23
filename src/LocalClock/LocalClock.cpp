#include "SoftwareSerial.h"
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <Fonts/TomThumb.h>
#include <LightDependentResistor.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <SparkFun_APDS9960.h>
#include <WiFiClient.h>
#include <Wire.h>

#include "Adafruit_HTU21DF.h"
#include <Adafruit_BMP280.h>
#include <BME280_t.h>
#include <DoubleResetDetect.h>
#include <WiFiManager.h>
#include <Wire.h>

#include <HardwareSerial.h>

#include <NTPClient.h>
// change next line to use with another board/shield
#include <ESP8266WiFi.h>
//#include <WiFi.h> // for WiFi shield
//#include <WiFi101.h> // for WiFi 101 shield or MKR1000
#include <WiFiUdp.h>

#include <DFMiniMp3.h>

#include "MenueControl/MenueControl.h"

#include "LocalClock.h"

#include <list>
#include <map>
#include <queue>

#define LDR_PIN A0

WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, "ntp2.aliyun.com", 8 * 60 * 60, 5 * 60 * 1000);

template <uint8_t h, uint8_t w> class Snake : public Layer {
  typedef enum {
    Dir_right = 0,
    Dir_down,
    Dir_left,
    Dir_up,
    Dir_unknow
  } DirType;

  // target
  uint8_t x;
  uint8_t y;

  uint8_t border_r = 130;
  uint8_t border_g = 130;
  uint8_t border_b = 130;

  uint8_t target_r = 255;
  uint8_t target_g = 64;
  uint8_t target_b = 0;

  uint8_t snake_r = 64;
  uint8_t snake_g = 255;
  uint8_t snake_b = 0;

  std::list<std::pair<uint8_t, uint8_t>> s;

  const uint8_t UNREACHABLE = 255;

  uint8_t animation_progress = 0;

public:
  Snake()
      : x(random8() % h), y(random8() % w),
        s({std::make_pair<uint8_t, uint8_t>(0, 0)}) {}

  std::pair<uint8_t, uint8_t> next_position(DirType d) {
    auto head = s.front();
    switch (d) {
    case Dir_right:
      if (head.second + 1 >= w)
        return {UNREACHABLE, UNREACHABLE};
      return {head.first, head.second + 1};
    case Dir_down:
      if (head.first + 1 >= h)
        return {UNREACHABLE, UNREACHABLE};
      return {head.first + 1, head.second};
    case Dir_left:
      if (head.second == 0)
        return {UNREACHABLE, UNREACHABLE};
      return {head.first, (head.second - 1)};
    case Dir_up:
      if (head.first == 0)
        return {UNREACHABLE, UNREACHABLE};
      return {head.first - 1, head.second};
    case Dir_unknow:
    default:
      return {UNREACHABLE, UNREACHABLE};
    }
  }

  void place() {
    bool any = false;
    do {
      x = random8() % h;
      y = random8() % w;
      any = std::any_of(s.begin(), s.end(),
                        [&](std::pair<uint8_t, uint8_t> &node) {
                          return node.first == x && node.second == y;
                        });
    } while (any);
  }

  void forward(DirType d) {
    auto n = next_position(d);
    auto any =
        std::any_of(s.begin(), s.end(), [&](std::pair<uint8_t, uint8_t> &node) {
          return node == n;
        });
    if (any || n.first == UNREACHABLE || n.second == UNREACHABLE) {
      animation_progress = 6;
      // border_r = random8(110, 150);
      // border_g = random8(110, 150);
      // border_b = random8(110, 150);
    } else {
      s.pop_back();
      s.push_front(n);
    }
  }

  void draw_snake(FastLED_NeoMatrix *matrix, uint8_t pos_x, uint8_t pos_y,
                  uint8_t r, uint8_t g, uint8_t b) {
    // draw gradient snake
    for (auto n = s.rbegin(); n != s.rend(); n++) {
      auto dis = std::distance(s.rbegin(), n) + 1;
      auto nr = map(dis, 0, s.size(), r / 3, r);
      auto ng = map(dis, 0, s.size(), g / 3, g);
      auto nb = map(dis, 0, s.size(), b / 3, b);
      auto Color = matrix->Color(nr, ng, nb);
      matrix->drawPixel(n->second + pos_x, n->first + pos_y, Color);
    }
  }

  DirType find_path_to(uint8_t x, uint8_t y) {
    const uint8_t MAX_STEP = 254;
    uint8_t map[h][w];
    memset(map, MAX_STEP, h * w);

    for (auto &n : s)
      map[n.first][n.second] = UNREACHABLE;
    auto begin_x = s.begin()->first;
    auto begin_y = s.begin()->second;
    map[begin_x][begin_y] = MAX_STEP;

    std::queue<std::pair<uint8_t, uint8_t>> q;

    q.push({x, y});
    map[x][y] = 1;

    while (!q.empty()) {
      auto n = q.front();
      q.pop();
      auto curr_val = map[n.first][n.second];
      if (n.second < w - 1) {
        auto &next_val = map[n.first][n.second + 1];
        if (next_val != UNREACHABLE && curr_val + 1 < next_val) {
          next_val = curr_val + 1;
          q.push({n.first, n.second + 1});
        }
      }
      if (n.second > 0) {
        auto &next_val = map[n.first][n.second - 1];
        if (next_val != UNREACHABLE && curr_val + 1 < next_val) {
          next_val = curr_val + 1;
          q.push({n.first, n.second - 1});
        }
      }
      if (n.first < h - 1) {
        auto &next_val = map[n.first + 1][n.second];
        if (next_val != UNREACHABLE && curr_val + 1 < next_val) {
          next_val = curr_val + 1;
          q.push({n.first + 1, n.second});
        }
      }
      if (n.first > 0) {
        auto &next_val = map[n.first - 1][n.second];
        if (next_val != UNREACHABLE && curr_val + 1 < next_val) {
          next_val = curr_val + 1;
          q.push({n.first - 1, n.second});
        }
      }
    }

    if (map[begin_x][begin_y] != MAX_STEP) {
      if (begin_y < w - 1 &&
          map[begin_x][begin_y + 1] == map[begin_x][begin_y] - 1) {
        return Dir_right;
      }
      if (begin_y > 0 &&
          map[begin_x][begin_y - 1] == map[begin_x][begin_y] - 1) {
        return Dir_left;
      }
      if (begin_x < h - 1 &&
          map[begin_x + 1][begin_y] == map[begin_x][begin_y] - 1) {
        return Dir_down;
      }
      if (begin_x > 0 &&
          map[begin_x - 1][begin_y] == map[begin_x][begin_y] - 1) {
        return Dir_up;
      }
    }
    return Dir_unknow;
  }

  unsigned measure(DirType d) {
    auto n = next_position(d);
    if (n.first == UNREACHABLE || n.second == UNREACHABLE)
      return 0;

    uint8_t map[h][w]{0};
    for (auto &node : s)
      map[node.first][node.second] = UNREACHABLE;

    if (map[n.first][n.second] == UNREACHABLE)
      return 0;

    unsigned r = 0;
    std::queue<std::pair<uint8_t, uint8_t>> q;

    map[n.first][n.second] = UNREACHABLE;
    q.push(n);
    while (!q.empty()) {
      r++;
      n = q.front();
      q.pop();

      if (n.second < w - 1 && map[n.first][n.second + 1] != UNREACHABLE) {
        map[n.first][n.second + 1] = UNREACHABLE;
        q.push({n.first, n.second + 1});
      }
      if (n.second > 0 && map[n.first][n.second - 1] != UNREACHABLE) {
        map[n.first][n.second - 1] = UNREACHABLE;
        q.push({n.first, n.second - 1});
      }
      if (n.first < h - 1 && map[n.first + 1][n.second] != UNREACHABLE) {
        map[n.first + 1][n.second] = UNREACHABLE;
        q.push({n.first + 1, n.second});
      }
      if (n.first > 0 && map[n.first - 1][n.second] != UNREACHABLE) {
        map[n.first - 1][n.second] = UNREACHABLE;
        q.push({n.first - 1, n.second});
      }
    }
    return r;
  }

  unsigned edges(DirType d) {
    auto n = next_position(d);
    if (n.first == UNREACHABLE || n.second == UNREACHABLE)
      return 0;

    uint8_t map[h][w]{0};
    for (auto &node : s)
      map[node.first][node.second] = UNREACHABLE;

    if (map[n.first][n.second] == UNREACHABLE)
      return 0;

    unsigned r = 0;

    if (n.second == w - 1 || map[n.first][n.second + 1] == UNREACHABLE)
      r++;
    if (n.second == 0 || map[n.first][n.second - 1] == UNREACHABLE)
      r++;
    if (n.first == h - 1 || map[n.first + 1][n.second] == UNREACHABLE)
      r++;
    if (n.first == 0 || map[n.first - 1][n.second] == UNREACHABLE)
      r++;
    return r;
  }

  DirType next_direction() {
#if 0
    auto d_best = find_path_to(x, y);
    if (d_best != Dir_unknow)
      return d_best;
    auto d_tail = find_path_to(s.rbegin()->first, s.rbegin()->second);
    if (d_tail != Dir_unknow)
      return d_tail;
    return (DirType)(random16() % Dir_unknow);
#else
    auto d_best = find_path_to(x, y);
    auto m_best = measure(d_best);
    if (d_best != Dir_unknow) {
      if (m_best > s.size())
        return d_best;
    }

    auto d_tail = find_path_to(s.rbegin()->first, s.rbegin()->second);
    auto m_tail = measure(d_tail);
    auto m_right = measure(Dir_right);
    auto m_down = measure(Dir_down);
    auto m_left = measure(Dir_left);
    auto m_up = measure(Dir_up);

    auto max = std::max({m_tail, m_right, m_down, m_left, m_up});
    std::map<DirType, unsigned> ds;
    if (m_tail == max)
      ds[d_tail] == edges(d_tail);
    if (m_right == max)
      ds[Dir_right] == edges(Dir_right);
    if (m_down == max)
      ds[Dir_down] == edges(Dir_down);
    if (m_left == max)
      ds[Dir_left] == edges(Dir_left);
    if (m_up == max)
      ds[Dir_up] == edges(Dir_up);

    max = 0;
    for (auto &d : ds)
      max = std::max(max, d.second);

    std::vector<DirType> r;
    r.reserve(4);
    for (auto &d : ds)
      if (d.second == max)
        r.push_back(d.first);

    return r[random16(r.size())];
#endif
  }

  void loop(unsigned frame_delay) override {
    if (animation_progress)
      return;

    static unsigned delay = 0;
    delay = (delay + 1) % (frame_delay + 1);
    if (delay == 0) {
      auto nd = next_direction();
      auto n = next_position(nd);
      if (n.first == x && n.second == y) {
        s.push_front({x, y});
        place();
      } else {
        forward(nd);
      }
    }
  }

  void render(FastLED_NeoMatrix *matrix, int8_t pos_x, int8_t pos_y) override {
    // draw border
    auto c = matrix->Color(border_r, border_g, border_b);
    matrix->drawRect(pos_x + 1, pos_y + 1, w + 2, h + 2, c);

    // draw target
    c = matrix->Color(target_r, target_g, target_b);
    matrix->drawPixel(y + pos_x + 2, x + pos_y + 2, c);

    // draw snake
    if (animation_progress == 0) {
      draw_snake(matrix, pos_x + 2, pos_y + 2, snake_r, snake_g, snake_b);
    } else {
      if (animation_progress % 2)
        draw_snake(matrix, pos_x + 2, pos_y + 2, target_r, target_g, target_b);
      else
        draw_snake(matrix, pos_x + 2, pos_y + 2, snake_r, snake_g, snake_b);

      animation_progress--;

      if (animation_progress == 0) {
        auto t = target_r;
        target_r = target_g;
        target_g = target_b;
        target_b = t;

        t = snake_r;
        snake_r = snake_g;
        snake_g = snake_b;
        snake_b = t;

        s.clear();
        s.push_back({0, 0});
        place();
      }
    }
  }
};

class DigitalClock : public Layer {
  uint8_t last_hh = 0;
  uint8_t last_hl = 0;
  uint8_t last_mh = 0;
  uint8_t last_ml = 0;
  bool first_start = true;

  uint8_t ani_hh = 0;
  uint8_t ani_hl = 0;
  uint8_t ani_mh = 0;
  uint8_t ani_ml = 0;
  uint8_t animation_progress = 0;

public:
  void loop(unsigned frame_delay) override {}

  void render(FastLED_NeoMatrix *matrix, int8_t x, int8_t y) override {
    ntp.update();
    auto hh = ntp.getHours() / 10;
    auto hl = ntp.getHours() % 10;
    auto mh = ntp.getMinutes() / 10;
    auto ml = ntp.getMinutes() % 10;

    // auto c = matrix->Color(0, 255, 64);
    matrix->setTextColor(matrix->Color(77, 147, 245));
    char buf[10];
    auto ntp_s = ntp.getSeconds();
    auto ntp_d = ntp.getDay();
    auto blink = ntp_s % 2 ? ":" : " ";
    auto last_h = last_hh * 10 + last_hl;
    auto last_m = last_mh * 10 + last_ml;
    sprintf(buf, "%02d%s%02d", last_h, blink, last_m);
    matrix->setCursor(x + 1, y + 6);
    matrix->print(buf);

    auto week_f = matrix->Color(230, 230, 230);
    auto week_b = matrix->Color(130, 130, 130);
    auto black = matrix->Color(0, 0, 0);

    if (ml != last_ml || mh != last_mh || hl != last_hl || hh != last_hh) {
      ani_ml = last_ml;
      ani_mh = last_mh;
      ani_hl = last_hl;
      ani_hh = last_hh;
      animation_progress = 7;
    }

    if (animation_progress || first_start) {
      if (ml != ani_ml || first_start) {
        matrix->fillRect(x + 15, y + 0, 3, 8, black);
        matrix->setCursor(x + 15, y + (7 - animation_progress) + 6);
        matrix->printf("%d", ani_ml);
        matrix->setCursor(x + 15, y + (7 - animation_progress));
        matrix->printf("%d", ml);
        matrix->drawRect(x + 15, y + 0, 3, 1, black);
        matrix->drawRect(x + 15, y + 6, 3, 2, black);
      }

      if (mh != ani_mh || first_start) {
        matrix->fillRect(x + 11, y + 0, 3, 8, black);
        matrix->setCursor(x + 11, y + (7 - animation_progress) + 6);
        matrix->printf("%d", ani_mh);
        matrix->setCursor(x + 11, y + (7 - animation_progress));
        matrix->printf("%d", mh);
        matrix->drawRect(x + 11, y + 0, 3, 1, black);
        matrix->drawRect(x + 11, y + 6, 3, 2, black);
      }

      if (hl != ani_hl || first_start) {
        matrix->fillRect(x + 5, y + 0, 3, 8, black);
        matrix->setCursor(x + 5, y + (7 - animation_progress) + 6);
        matrix->printf("%d", ani_hl);
        matrix->setCursor(x + 5, y + (7 - animation_progress));
        matrix->printf("%d", hl);
        matrix->drawRect(x + 5, y + 0, 3, 1, black);
        matrix->drawRect(x + 5, y + 6, 3, 2, black);
      }

      if (hh != ani_hh || first_start) {
        matrix->fillRect(x + 1, y + 0, 3, 8, black);
        matrix->setCursor(x + 1, y + (7 - animation_progress) + 6);
        matrix->printf("%d", ani_hh);
        matrix->setCursor(x + 1, y + (7 - animation_progress));
        matrix->printf("%d", hh);
        matrix->drawRect(x + 1, y + 0, 3, 1, black);
        matrix->drawRect(x + 1, y + 6, 3, 2, black);
      }

      animation_progress--;

      if (animation_progress == 0)
        first_start = false;
    }

    for (int i = 1; i < 7; i++) {
      auto color = ntp_d == i ? week_f : week_b;
      auto x0 = x + 1 + (i - 1) * 3;
      matrix->drawLine(x0, y + 7, x0 + 1, y + 7, color);
    }

    last_ml = ml;
    last_mh = mh;
    last_hl = hl;
    last_hh = hh;
  }
};

class BinaryClock : public Layer {
public:
  void loop(unsigned frame_delay) override {}

  void render(FastLED_NeoMatrix *matrix, int8_t x, int8_t y) override {
    ntp.update();
    auto ntp_h = ntp.getHours();
    auto ntp_m = ntp.getMinutes();
    auto ntp_s = ntp.getSeconds();
    auto r = matrix->Color(255, 0, 0);
    auto g = matrix->Color(0, 255, 0);
    auto b = matrix->Color(0, 0, 255);

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

template <uint8_t h, uint8_t w> class Spectrumer : public Layer {
  int8_t value[w]{0};
  uint8_t r[h]{0};
  uint8_t g[h]{0};

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

  void loop(unsigned frame_delay) override {
    static unsigned delay = 0;
    delay = (delay + 1) % (frame_delay + 1);
    if (delay == 0) {
      for (uint8_t i = 0; i < w; i++) {
        int8_t r = (int8_t)random8(3) - 1;
        value[i] += r;
        if (value[i] > h || value[i] < 0)
          value[i] -= r;
      }
    }
  }

  void render(FastLED_NeoMatrix *matrix, int8_t x, int8_t y) {
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
};

Snake<5, 10> snake; // 20, 2
Snake<6, 30> fullscreen_snake;

Spectrumer<5, 10> spectrumer;

DigitalClock dclock; // 13
BinaryClock bclock;

LocalClock::LocalClock() {
  layers.push_back(&dclock);
  layers.push_back(&bclock);
  layers.push_back(&fullscreen_snake);

  windows.push_back(&snake);
  windows.push_back(&spectrumer);
}

bool LocalClock::wait() {
  wait_start = wait_start ? wait_start : millis();
  if (millis() - wait_start < 2000) {
    delay(100);
    return true;
  }
  return false;
}

void LocalClock::loop(FastLED_NeoMatrix *matrix, bool autoBr, int minBr,
                      int maxBr, bool *pushed, int *timeout) {
  static bool seed = false;
  if (!seed) {
    random16_set_seed(ntp.getEpochTime() % UINT16_MAX);
    seed = true;
  }

  if (autoBr) {
    auto ldr = analogRead(LDR_PIN);
    static int br = 0;
    auto newbr = map(ldr, 0, 1023, minBr, maxBr);
    if (newbr != br) {
      matrix->setBrightness(newbr);
      br = newbr;
    }
  }

  if (pushed[0] && millis() - timeout[0] < 100) {
    if ((layout == LAYOUT_L_DIGITAL || layout == LAYOUT_R_DIGITAL) &&
        animation_1 == 0 && animation_2 == 0) {
      windows.push_back(windows.front());
      windows.pop_front();
      animation_1 = 7;
    }
  }
  if (pushed[1] && millis() - timeout[1] < 100) {
    frame_delay = (frame_delay + 1) % 6;
  }
  if (pushed[2] && millis() - timeout[2] < 100) {
    if (animation_1 == 0 && animation_2 == 0) {
      layout = (LayoutType)((layout + 1) % LAYOUT_MAX);
      animation_2 = 7;
    }
  }

  matrix->clear();

  //
  // window switching
  if (animation_1) {
    auto x = windows.back()->x();
    auto y = windows.back()->y() + (8 - animation_1);
    windows.back()->loop(frame_delay);
    windows.back()->render(matrix, x, y);
    windows.front()->loop(frame_delay);
    windows.front()->render(matrix, x, y - 8);
    dclock.loop(frame_delay);
    dclock.Layer::render(matrix);
    animation_1--;
  }

  //
  // layout switching
  else if (animation_2) {
    // prev layout
    auto prev_layout = (LayoutType)((layout + LAYOUT_MAX - 1) % LAYOUT_MAX);
    switch (prev_layout) {
    case LAYOUT_L_DIGITAL:
      windows.back()->render(matrix, 18, 8 - animation_2);
      dclock.render(matrix, 0, 8 - animation_2);
      break;
    case LAYOUT_R_DIGITAL:
      windows.back()->render(matrix, 0, 8 - animation_2);
      dclock.render(matrix, 13, 8 - animation_2);
      break;
    case LAYOUT_BINARY:
      bclock.render(matrix, 0, 8 - animation_2);
      break;
    case LAYOUT_FULLSCREEN:
      fullscreen_snake.render(matrix, -1, 8 - 1 - animation_2);
      break;
    default:
      break;
    }

    // current layout
    switch (layout) {
    case LAYOUT_L_DIGITAL:
      windows.front()->render(matrix, 18, -animation_2);
      dclock.render(matrix, 0, -animation_2);
      break;
    case LAYOUT_R_DIGITAL:
      windows.front()->render(matrix, 0, -animation_2);
      dclock.render(matrix, 13, -animation_2);
      break;
    case LAYOUT_BINARY:
      bclock.render(matrix, 0, -animation_2);
      break;
    case LAYOUT_FULLSCREEN:
      fullscreen_snake.render(matrix, -1, -1 - animation_2);
      break;
    default:
      break;
    }
    animation_2--;
  }

  //
  // layout switching
  else {
    windows.front()->hidden();
    for (auto layer : layers)
      layer->hidden();

    switch (layout) {
    case LAYOUT_L_DIGITAL:
      windows.front()->move(18, 0).show();
      dclock.move(0, 0).show();
      break;
    case LAYOUT_R_DIGITAL:
      windows.front()->move(0, 0).show();
      dclock.move(13, 0).show();
      break;
    case LAYOUT_BINARY:
      bclock.move(0, 0).show();
      break;
    case LAYOUT_FULLSCREEN:
      fullscreen_snake.move(-1, -1).show();
      break;
    default:
      break;
    }

    if (!windows.front()->is_hidden()) {
      windows.front()->loop(frame_delay);
      windows.front()->render(matrix);
    }
    for (auto layer : layers) {
      if (!layer->is_hidden()) {
        layer->loop(frame_delay);
        layer->render(matrix);
      }
    }
  }

  matrix->show();
  delay(100);
}