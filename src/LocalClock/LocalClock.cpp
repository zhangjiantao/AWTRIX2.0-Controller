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
#include <type_traits>

#define LDR_PIN A0

WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, "ntp2.aliyun.com", 8 * 60 * 60, 24 * 60 * 60 * 1000);

class Mp3Notify;
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
extern DfMp3 dfmp3;
extern WiFiManager wifiManager;
extern ESP8266WebServer server;

#define RANDOM_RGB(r, g, b)                                                    \
  do {                                                                         \
    r = random8(255);                                                          \
    g = random8(255);                                                          \
    b = random8(255);                                                          \
  } while ((r + g + b < 256))

template <uint8_t h, uint8_t w> class Snake : public Widget {
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

  int frame_delay = 4;
  unsigned _delay = 0;

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
      animation_progress = 7;
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

  void loop(FastLED_NeoMatrix *matrix) override {
    _delay = (_delay + 1) % (frame_delay + 1);
    if (_delay)
      return;

    if (animation_progress) {
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
      return;
    }

    auto nd = next_direction();
    auto n = next_position(nd);
    if (n.first == x && n.second == y) {
      s.push_front({x, y});
      place();
    } else {
      forward(nd);
    }
  }

  void render(FastLED_NeoMatrix *matrix, int pos_x, int pos_y) override {
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
    }
  }

  void event1(FastLED_NeoMatrix *matrix) override {
    frame_delay = ++frame_delay % 8;
  }
};

template <bool fullscreen = false> class DigitalClock2 : public Widget {
  uint8_t last_hh = 0;
  uint8_t last_hl = 0;
  uint8_t last_mh = 0;
  uint8_t last_ml = 0;
  uint8_t ani_hh = 0;
  uint8_t ani_hl = 0;
  uint8_t ani_mh = 0;
  uint8_t ani_ml = 0;
  bool first_start = true;

  uint8_t animation_progress = 0;

  uint8_t r = 0;
  uint8_t g = 255;
  uint8_t b = 64;

public:
  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    uint8_t offset = fullscreen ? 7 : 0;

    auto hh = ntp.getHours() / 10;
    auto hl = ntp.getHours() % 10;
    auto mh = ntp.getMinutes() / 10;
    auto ml = ntp.getMinutes() % 10;

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
    auto last_h = last_hh * 10 + last_hl;
    auto last_m = last_mh * 10 + last_ml;
    sprintf(buf, "%02d%s%02d", last_h, blink, last_m);
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
        if (hl != last_hl) {
          RANDOM_RGB(r, g, b);
          dfmp3.playAdvertisement(0);
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

  void event1(FastLED_NeoMatrix *matrix) override { RANDOM_RGB(r, g, b); }
};

class DigitalClock3 : public Widget {
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
  bool animation = false;
  uint8_t animation_progress = 0;

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
    if (animation) {
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
        animation_progress = 12;
      }

      if (animation_progress || first_start) {
        if (sl != ani_sl || first_start) {
          matrix->fillRect(x + 27, y + 0, 3, 8, black);
          matrix->setCursor(x + 27, y + (6 - animation_progress / 2) + 6);
          matrix->printf("%d", ani_sl);
          matrix->setCursor(x + 27, y + (6 - animation_progress / 2));
          matrix->printf("%d", sl);
          matrix->drawRect(x + 27, y + 0, 3, 1, black);
          matrix->drawRect(x + 27, y + 6, 3, 2, black);
        }

        if (sh != ani_sh || first_start) {
          matrix->fillRect(x + 23, y + 0, 3, 8, black);
          matrix->setCursor(x + 23, y + (6 - animation_progress / 2) + 6);
          matrix->printf("%d", ani_sh);
          matrix->setCursor(x + 23, y + (6 - animation_progress / 2));
          matrix->printf("%d", sh);
          matrix->drawRect(x + 23, y + 0, 3, 1, black);
          matrix->drawRect(x + 23, y + 6, 3, 2, black);
        }

        if (ml != ani_ml || first_start) {
          matrix->fillRect(x + 17, y + 0, 3, 8, black);
          matrix->setCursor(x + 17, y + (6 - animation_progress / 2) + 6);
          matrix->printf("%d", ani_ml);
          matrix->setCursor(x + 17, y + (6 - animation_progress / 2));
          matrix->printf("%d", ml);
          matrix->drawRect(x + 17, y + 0, 3, 1, black);
          matrix->drawRect(x + 17, y + 6, 3, 2, black);
        }

        if (mh != ani_mh || first_start) {
          matrix->fillRect(x + 13, y + 0, 3, 8, black);
          matrix->setCursor(x + 13, y + (6 - animation_progress / 2) + 6);
          matrix->printf("%d", ani_mh);
          matrix->setCursor(x + 13, y + (6 - animation_progress / 2));
          matrix->printf("%d", mh);
          matrix->drawRect(x + 13, y + 0, 3, 1, black);
          matrix->drawRect(x + 13, y + 6, 3, 2, black);
        }

        if (hl != ani_hl || first_start) {
          matrix->fillRect(x + 7, y + 0, 3, 8, black);
          matrix->setCursor(x + 7, y + (6 - animation_progress / 2) + 6);
          matrix->printf("%d", ani_hl);
          matrix->setCursor(x + 7, y + (6 - animation_progress / 2));
          matrix->printf("%d", hl);
          matrix->drawRect(x + 7, y + 0, 3, 1, black);
          matrix->drawRect(x + 7, y + 6, 3, 2, black);
        }

        if (hh != ani_hh || first_start) {
          matrix->fillRect(x + 3, y + 0, 3, 8, black);
          matrix->setCursor(x + 3, y + (6 - animation_progress / 2) + 6);
          matrix->printf("%d", ani_hh);
          matrix->setCursor(x + 3, y + (6 - animation_progress / 2));
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

  void event1(FastLED_NeoMatrix *matrix) override {
    animation = !animation;
    RANDOM_RGB(r, g, b);
  }
};

class BinaryClock : public Widget {
public:
  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
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
        int8_t r = (int8_t)random8(3) - 1;
        value[i] += r;
        if (value[i] > h || value[i] < 0)
          value[i] -= r;
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

  void event1(FastLED_NeoMatrix *matrix) override {
    frame_delay = ++frame_delay % 8;
  }
};

class HttpUtils {
  HTTPClient httpClient;
  WiFiClient wifiClient;
  BearSSL::WiFiClientSecure httpsClient;

public:
  String httpRequest(const String &url, int &errCode) {
    String res;

    if (httpClient.begin(wifiClient, url)) { // HTTP
      int httpCode = httpClient.GET();
      errCode = httpCode;
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          errCode = 0;
          res = httpClient.getString();
        }
      }
      httpClient.end();
    }
    return res;
  }
};

template <uint8_t h, uint8_t w> class SSEC : public Widget {
  HttpUtils http;
  const String api = "http://img2.money.126.net/data/hs/time/today/";

  struct Index {
    const String code;
    unsigned min, max;
    unsigned yestclose;
    std::vector<unsigned> chart;
  };

  std::vector<Index> targets{{"0000001.json", 0, 0, 0, {}},
                             {"1399001.json", 0, 0, 0, {}},
                             {"1399006.json", 0, 0, 0, {}}};

  unsigned long last_update_time = 0;

  unsigned current = 0;

  bool update() {
    Index &target = targets[current];
    int errCode = 0;
    auto response = http.httpRequest(api + target.code, errCode);
    if (errCode)
      return false;

    auto get_float_value = [&](const char *prefix, unsigned offset,
                               float &result) {
      auto p = response.indexOf(prefix);
      if (p == -1)
        return false;
      p += offset;
      auto pstr = response.c_str() + p;
      result = strtof(pstr, nullptr);
      return true;
    };

    // parse yestclose
    float value;
    get_float_value("\"yestclose\":", 12, value);
    target.yestclose = std::round(value);

    static unsigned data[243];
    unsigned data_size = 0;

    static char buff[16];
    for (unsigned i = 930; i < 1531; i++) {
      if (i % 100 > 59 || i / 100 == 12)
        continue;
      sprintf(buff, "[\"%04d\",", i);
      if (get_float_value(buff, 8, value))
        data[data_size++] = std::round(value);
    }

    Serial.printf("date size = %d\n", data_size);
    if (data_size == 0)
      return false;

    target.chart.clear();
    target.chart.push_back(data[0]);
    for (unsigned i = 27; i <= data_size; i += 27)
      target.chart.push_back(data[i - 1]);

    if (data_size % 27)
      target.chart.push_back(data[data_size - 1]);

    Serial.printf("chart size = %d\n", target.chart.size());

    target.max = *std::max_element(target.chart.begin(), target.chart.end());
    target.min = *std::min_element(target.chart.begin(), target.chart.end());
    return true;
  }

public:
  SSEC() {
    for (auto &i : targets)
      i.chart.reserve(10);
  }

  void loop(FastLED_NeoMatrix *matrix) override {
    auto ts = ntp.getEpochTime();
    if (ts - last_update_time < 60)
      return;
    last_update_time = ts;
    auto hasdata = !targets[current].chart.empty();

    if (hasdata) {
      if (ntp.getDay() == 0 || ntp.getDay() == 6)
        return;

      if (ntp.getHours() < 9 || ntp.getHours() == 12 || ntp.getHours() > 14)
        return;

      if (ntp.getHours() == 9 && ntp.getMinutes() < 30)
        return;

      if (ntp.getHours() == 11 && ntp.getMinutes() > 30)
        return;
    }

    update();

    Serial.print(ntp.getFormattedTime());
    Serial.println(" update succe");
  }

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    auto c = matrix->Color(130, 130, 130);
    matrix->drawRect(x + 1, y + 1, w + 2, h + 2, c);

    auto r = matrix->Color(255, 0, 0);
    auto g = matrix->Color(0, 255, 0);
    auto &target = targets[current];
    for (int i = 0; i < target.chart.size(); i++) {
      auto c = target.chart[i] >= target.yestclose ? r : g;
      int v;
      if (target.min == target.max)
        v = 3;
      else
        v = map(target.chart[i], target.min, target.max, 0, 4);
      matrix->drawPixel(x + 2 + i, y + 6 - v, c);
    }
  }

  void event1(FastLED_NeoMatrix *matrix) override {
    current = ++current % targets.size();
    if (targets[current].chart.empty())
      update();
  }
};

class BrightnessUpdater : public Task {
  unsigned long last_update = 0;

public:
  bool run(FastLED_NeoMatrix *matrix) override {
    auto ms = millis();
    if (last_update != 0 && ms - last_update < 3000)
      return false;

    last_update = ms;
    auto ldr = analogRead(LDR_PIN);
    auto update = map(ldr > 256 ? 256 : ldr, 0, 256, 4, 70);
    matrix->setBrightness(update);
    return true;
  }
};

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

Snake<5, 10> snake; // 10, 0
Spectrumer<5, 10> spectrumer;
SSEC<5, 10> Ssec;
DigitalClock2<> dclock2; // 13
DigitalClock3 dclock3;
BinaryClock bclock;
Snake<6, 30> fullscreen_snake;

template <typename T, uint8_t offset, uint8_t width> class EffectPlayer {
  uint8_t animation = 0;
  int8_t n = 0;

public:
  bool playing() { return n > 0; }

  void begin(std::list<T *> &list) {
    if (playing()) {
      n = 0;
      return;
    }
    animation = (animation + 1) % 2;
    if (animation < 2)
      n = 8;
    else
      n = width;
    list.push_back(list.front());
    list.pop_front();
  }

  void render(std::list<T *> &list, FastLED_NeoMatrix *matrix, int x, int y) {
#define EFFECT_SYNC_MODE 0
#if EFFECT_SYNC_MODE
    while (n >= 0) {
      matrix->fillRect(offset, 0, width, 8, matrix->Color(0, 0, 0));
#endif
      switch (animation) {
      case 0:
        list.front()->render(matrix, x + offset, y + 8 - (8 - n));
        list.back()->render(matrix, x + offset, y - 8 + n);
        break;
      case 1:
        list.front()->render(matrix, x + offset, y - 8 + (8 - n));
        list.back()->render(matrix, x + offset, y + 8 - n);
        break;
      case 2:
        list.front()->render(matrix, x + offset + width - (width - n), y);
        list.back()->render(matrix, x + offset - width + n, y);
        break;
      case 3:
        list.front()->render(matrix, x + offset - width + (width - n), y);
        list.back()->render(matrix, x + offset + width - n, y);
        break;
      }

      n -= 1;
#if EFFECT_SYNC_MODE
      matrix->show();
      delay(GLOBAL_DELAY);
    }
    n = 0;
#endif
  }
};

template <uint8_t clock_offset, uint8_t widget_offset>
class ClockCanvas2 : public Canvas {
  std::list<Widget *> widgets;
  EffectPlayer<Widget, widget_offset, 14> player;

public:
  ClockCanvas2() : widgets({&snake, &spectrumer, &Ssec}) {}

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    dclock2.render(matrix, x + clock_offset, y);
    if (!player.playing())
      widgets.front()->render(matrix, x + widget_offset, y);
    else
      player.render(widgets, matrix, x, y);
  }

  void loop(FastLED_NeoMatrix *matrix) override {
    dclock2.loop(matrix);
    widgets.front()->loop(matrix);
  }

  void event0(FastLED_NeoMatrix *matrix) override { player.begin(widgets); }

  void event1(FastLED_NeoMatrix *matrix) override {
    dclock2.event1(matrix);
    widgets.front()->event1(matrix);
  }
};

class ClockCanvas1 : public Canvas {
  std::list<Widget *> widgets;
  EffectPlayer<Widget, 0, 32> player;

public:
  ClockCanvas1() : widgets({&dclock3, new DigitalClock2<true>, &bclock}) {}

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    if (!player.playing()) {
      widgets.front()->render(matrix, x, y);
    } else {
      player.render(widgets, matrix, x, y);
    }
  }

  void loop(FastLED_NeoMatrix *matrix) override {
    widgets.front()->loop(matrix);
  }

  void event0(FastLED_NeoMatrix *matrix) override { player.begin(widgets); }

  void event1(FastLED_NeoMatrix *matrix) override {
    widgets.front()->event1(matrix);
  }
};

class MatrixImpl : public Matrix {
  std::list<Canvas *> canvases;
  std::vector<Task *> tasks{new BrightnessUpdater, new NTPUpdater};

  EffectPlayer<Canvas, 0, 32> player;

public:
  MatrixImpl() {
    auto clock1 = new ClockCanvas1;
    canvases.push_back(new ClockCanvas2<0, 18>());
    canvases.push_back(clock1);
    canvases.push_back(new ClockCanvas2<13, 0>());
    canvases.push_back(clock1);
  }

  void render(FastLED_NeoMatrix *matrix) override {
    if (!player.playing())
      canvases.front()->render(matrix, 0, 0);
    else
      player.render(canvases, matrix, 0, 0);
  }

  void loop(FastLED_NeoMatrix *m) override {
    canvases.front()->loop(m);
    for (auto t : tasks)
      t->run(m);
  }

  void event0(FastLED_NeoMatrix *matrix) override {
    canvases.front()->event0(matrix);
  }
  void event1(FastLED_NeoMatrix *matrix) override {
    canvases.front()->event1(matrix);
  }
  void event2(FastLED_NeoMatrix *matrix) override { player.begin(canvases); }
};

LocalClock::LocalClock() { m = new MatrixImpl; }

bool LocalClock::shoud_wait_reconnect(const char *server) {
  if (!strcmp(server, "0.0.0.0"))
    return false;
  static unsigned long wait_start = 0;
  wait_start = wait_start ? wait_start : millis();
  if (millis() - wait_start < 2000)
    return true;
  return false;
}

void LocalClock::event(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout) {
  if (pushed[0] && millis() - timeout[0] < GLOBAL_DELAY) {
    dfmp3.playAdvertisement(9);
    m->event0(matrix);
  }
  if (pushed[1] && millis() - timeout[1] < GLOBAL_DELAY) {
    dfmp3.playAdvertisement(9);
    m->event1(matrix);
  }
  if (pushed[2] && millis() - timeout[2] < GLOBAL_DELAY) {
    dfmp3.playAdvertisement(9);
    m->event2(matrix);
  }

  if (pushed[1] && millis() - timeout[1] > 1900) {
    int count = 4;
    while (!digitalRead(D4)) {
      matrix->clear();
      matrix->setTextColor(matrix->Color(255, 0, 255));
      matrix->setCursor(1, 6);
      if (count) {
        matrix->clear();
        matrix->printf("REBOOT  %d", count - 1);
        matrix->show();
      } else {
        matrix->clear();
        matrix->print("REBOOT...");
        matrix->show();
        ESP.reset();
      }

      count--;
      delay(1000);
    }
  }
}

const char *controller_page =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/><title>Awtrix "
    "Controller</title><style "
    "type=\"text/"
    "css\">.button0{background-color:#4CAF50;border-radius:20%;color:white;"
    "padding:5%5%;text-align:center;text-decoration:none;display:inline-"
    "block;"
    "font-size:40px}</style></head><body><script>function btn_click(id){var "
    "t=document.createElement(\"form\");t.action=\"control\";t.method="
    "\"post\";"
    "t.style.display=\"none\";t.target=\"iframe\";var "
    "opt=document.createElement(\"textarea\");opt.name=\"id\";opt.value=id;t."
    "appendChild(opt);document.body.appendChild(t);t.submit()}</"
    "script><iframe "
    "id=\"iframe\"name=\"iframe\"style=\"display:none;\"></iframe><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#f44336;\"onclick=\"btn_click(0)\">左按键</button><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#008CBA;\"onclick=\"btn_click(1)\">中按键</button><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#f44336;\"onclick=\"btn_click(2)\">右按键</button></body></html>";

void LocalClock::handle(FastLED_NeoMatrix *matrix) {
  static bool start = false;
  if (start)
    return;

  server.on("/control", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", controller_page);
  });

  server.on("/control", HTTP_POST, [&]() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "ok");
    bool pushed[3]{false, false, false};
    int timeout[3]{0, 0, 0};
    for (int i = 0; i < server.args(); i++) {
      if (server.argName(i) == "id") {
        int id = server.arg(i).toInt();
        if (id < 3 && id >= 0) {
          pushed[id] = true;
          timeout[id] = millis();
          event(matrix, pushed, timeout);
        }
      }
    }
  });

  start = true;
}

void LocalClock::loop(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout) {
  event(matrix, pushed, timeout);

  handle(matrix);

  matrix->clear();

  m->render(matrix);
  m->loop(matrix);

  matrix->show();
  delay(GLOBAL_DELAY);
}