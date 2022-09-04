#include <Arduino.h>
#include <FastLED_NeoMatrix.h>

#include <cstdint>
#include <list>
#include <map>
#include <queue>

#include "../NTPClock.h"

template <uint8_t h, uint8_t w, bool fullscreen> class Snake : public Widget {
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
      animation_progress = 8;
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
    matrix->drawRect(pos_x + 1 - fullscreen, pos_y + 1 - fullscreen, w + 2,
                     h + 2, c);

    // draw target
    c = matrix->Color(target_r, target_g, target_b);
    matrix->drawPixel(y + pos_x + 2 - fullscreen, x + pos_y + 2 - fullscreen, c);

    // draw snake
    uint8_t r, g, b;
    if (animation_progress == 0 || animation_progress % 2) {
      r = snake_r;
      g = snake_g;
      b = snake_b;
    } else {
      r = target_r;
      g = target_g;
      b = target_b;
    }

    draw_snake(matrix, pos_x + 2 - fullscreen, pos_y + 2 - fullscreen, r, g, b);
  }

  bool event1(FastLED_NeoMatrix *matrix) override {
    frame_delay = ++frame_delay % 8;
    return false;
  }
};

static Registry::RegisterWidget<Snake<5, 10, false>> X;
static Registry::RegisterFullscreenWidget<Snake<6, 30, true>> Y;