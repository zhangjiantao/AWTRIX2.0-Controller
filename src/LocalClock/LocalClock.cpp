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
#include <queue>

#define LDR_PIN A0

WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, "ntp2.aliyun.com", 8 * 60 * 60, 5 * 60 * 1000);

template <uint8_t pos_x, uint8_t pos_y, uint8_t h, uint8_t w> class Snake {
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

  DirType dir;

  std::list<std::pair<uint8_t, uint8_t>> s;

  const uint8_t UNREACHABLE = 255;

  bool show_border;

public:
  Snake(bool show_border = true)
      : show_border(show_border), x(random8() % h), y(random8() % w),
        dir(Dir_right), s({std::make_pair<uint8_t, uint8_t>(0, 0)}) {}

  void set_show_border(bool b) { show_border = b; }

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

  void forward(FastLED_NeoMatrix *matrix, DirType d) {
    auto n = next_position(d);
    auto any =
        std::any_of(s.begin(), s.end(), [&](std::pair<uint8_t, uint8_t> &node) {
          return node == n;
        });
    if (any || n.first == UNREACHABLE || n.second == UNREACHABLE) {
      // game over
      for (int i = 0; i < 3; i++) {
        draw_snake(matrix, target_r, target_g, target_b);
        delay(100);
        matrix->show();
        draw_snake(matrix, snake_r, snake_g, snake_b);
        delay(100);
        matrix->show();
      }

      // border_r = random8(110, 150);
      // border_g = random8(110, 150);
      // border_b = random8(110, 150);

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
    } else {
      s.pop_back();
      s.push_front(n);
    }
  }

  void draw_snake(FastLED_NeoMatrix *matrix, uint8_t r, uint8_t g, uint8_t b) {
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

  DirType next_dir() {
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
    std::vector<DirType> ds;
    ds.reserve(5);
    if (m_tail == max)
      ds.push_back(d_tail);
    if (m_right == max)
      ds.push_back(Dir_right);
    if (m_down == max)
      ds.push_back(Dir_down);
    if (m_left == max)
      ds.push_back(Dir_left);
    if (m_up == max)
      ds.push_back(Dir_up);

    return ds[random8(ds.size())];
  }

  void loop(FastLED_NeoMatrix *matrix) {
    static bool seed = false;
    if (!seed) {
      random16_set_seed(ntp.getEpochTime() % UINT16_MAX);
      seed = true;
    }

    // draw border
    auto c = matrix->Color(border_r, border_g, border_b);
    if (show_border)
      matrix->drawRect(pos_x - 1, pos_y - 1, w + 2, h + 2, c);

    // draw target
    c = matrix->Color(target_r, target_g, target_b);
    matrix->drawPixel(y + pos_x, x + pos_y, c);

    static unsigned wait = 0;
    wait = ++wait % 4;
    if (wait == 0) {
      auto nd = next_dir();
      auto n = next_position(nd);
      if (n.first == x && n.second == y) {
        s.push_front({x, y});
        place();
      } else {
        forward(matrix, nd);
      }
    }

    draw_snake(matrix, snake_r, snake_g, snake_b);
  }
};

Snake<2, 2, 5, 10> l_snake;
Snake<20, 2, 5, 10> r_snake;
Snake<1, 1, 6, 30> f_snake;

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
  if (pushed[0] && millis() - timeout[0] < 100)
    mode = (ShowMode)((mode + 6 - 1) % 6);
  if (pushed[2] && millis() - timeout[2] < 100)
    mode = (ShowMode)((mode + 1) % 6);

  // static int freq = 0;
  // freq = ++freq % 4;
  // if (freq == 0) {
  //   if (system_get_cpu_freq() != 80 || 1) {
  //     REG_CLR_BIT(0x3ff00014, BIT(0));
  //     system_update_cpu_freq(80);
  //     os_update_cpu_frequency(80);
  //   }
  // }

  if (autoBr) {
    auto ldr = analogRead(LDR_PIN);
    static int br = 0;
    auto newbr = map(ldr, 0, 1023, minBr, maxBr);
    if (newbr != br) {
      matrix->setBrightness(newbr);
      br = newbr;
    }
  }

  ntp.update();
  char buf[10];
  auto ntp_sec = ntp.getSeconds();
  auto blink = ntp_sec % 2 ? ":" : " ";
  sprintf(buf, "%02d%s%02d", ntp.getHours(), blink, ntp.getMinutes());

  matrix->clear();

  if (mode == LeftMode || mode == LeftNoBorder) {
    // 时:分
    matrix->setTextColor(matrix->Color(0, 255, 64));
    matrix->setCursor(14, 6);
    matrix->print(buf);

    // 星期
    auto day = ntp.getDay();
    if (day) {
      for (int i = 1; i < 7; i++) {
        auto f = matrix->Color(230, 230, 230);
        auto b = matrix->Color(130, 130, 130);
        auto color = day == i ? f : b;
        matrix->drawLine(14 + (i - 1) * 3, 7, 15 + (i - 1) * 3, 7, color);
      }
    } else {
      for (int i = 1; i < 7; i++) {
        auto f = matrix->Color(230, 230, 230);
        matrix->drawLine(14 + (i - 1) * 3, 7, 15 + (i - 1) * 3, 7, f);
      }
    }
  } else if (mode == RightMode || mode == RightNoBorder) {
    // 时:分
    matrix->setTextColor(matrix->Color(0, 255, 64));
    matrix->setCursor(1, 6);
    matrix->print(buf);

    // 星期
    auto day = ntp.getDay();
    if (day) {
      for (int i = 1; i < 7; i++) {
        auto f = matrix->Color(230, 230, 230);
        auto b = matrix->Color(130, 130, 130);
        auto color = day == i ? f : b;
        matrix->drawLine(1 + (i - 1) * 3, 7, 2 + (i - 1) * 3, 7, color);
      }
    } else {
      for (int i = 1; i < 7; i++) {
        auto f = matrix->Color(230, 230, 230);
        matrix->drawLine(1 + (i - 1) * 3, 7, 2 + (i - 1) * 3, 7, f);
      }
    }
  }

  if (mode == LeftMode || mode == LeftNoBorder) {
    l_snake.set_show_border(mode == LeftMode);
    l_snake.loop(matrix);
  } else if (mode == RightMode || mode == RightNoBorder) {
    r_snake.set_show_border(mode == RightMode);
    r_snake.loop(matrix);
  } else {
    f_snake.set_show_border(mode == FullScreen);
    f_snake.loop(matrix);
  }

  matrix->show();
  delay(100);
}