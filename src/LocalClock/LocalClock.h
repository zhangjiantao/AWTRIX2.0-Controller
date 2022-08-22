#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <time.h>
#include <vector>

class Layer {
public:
  virtual void loop(unsigned frame_delay) = 0;
  virtual void render(FastLED_NeoMatrix *matrix) = 0;
};

class LocalClock {
  typedef enum {
    SWDigitalLeft = 0,
    SWDigitalRight,
    SWBinaryLeft,
    SWBinaryRight,
    SWFullScreen,

    SW_MAX
  } ShowMode;

  ShowMode mode = SWDigitalLeft;
  unsigned long wait_start = 0;

  unsigned frame_delay = 4;

  void draw_digital_clock(FastLED_NeoMatrix *matrix);
  void draw_binary_clock(FastLED_NeoMatrix *matrix);

  std::vector<Layer *> layers;

public:
  LocalClock() { layers.reserve(4); }

  bool wait();

  void loop(FastLED_NeoMatrix *matrix, bool autoBrightness, int minBrightness,
            int maxBrightness, bool *pushed, int *timeout);
};