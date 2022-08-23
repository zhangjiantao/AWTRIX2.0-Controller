#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <time.h>
#include <vector>
#include <list>

class Layer {
  bool _hidden = false;

  int8_t _pos_x = 0;
  int8_t _pos_y = 0;

public:
  virtual void loop(unsigned frame_delay) = 0;
  virtual void render(FastLED_NeoMatrix *matrix, int8_t x, int8_t y) = 0;

  Layer &render(FastLED_NeoMatrix *matrix) {
    render(matrix, _pos_x, _pos_y);
    return *this;
  }

  Layer &move(int8_t x, int8_t y) {
    _pos_x = x;
    _pos_y = y;
    return *this;
  }

  Layer &hidden() {
    _hidden = true;
    return *this;
  }
  Layer &show() {
    _hidden = false;
    return *this;
  }

  bool is_hidden() { return _hidden; }

  int8_t x() { return _pos_x; }
  int8_t y() { return _pos_y; }
};

class LocalClock {
  typedef enum {
    LAYOUT_L_DIGITAL = 0,
    LAYOUT_R_DIGITAL,
    LAYOUT_BINARY,
    LAYOUT_FULLSCREEN,

    LAYOUT_MAX
  } LayoutType;

  unsigned long wait_start = 0;

  unsigned frame_delay = 4;

  void draw_digital_clock(FastLED_NeoMatrix *matrix);
  void draw_binary_clock(FastLED_NeoMatrix *matrix);

  std::vector<Layer *> layers;
  std::list<Layer *> windows;

  int8_t animation_1 = 0;
  int8_t animation_2 = 0;
  LayoutType layout;

public:
  LocalClock();

  bool wait();

  void loop(FastLED_NeoMatrix *matrix, bool autoBrightness, int minBrightness,
            int maxBrightness, bool *pushed, int *timeout);
};