#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <time.h>

class LocalClock {
  typedef enum {
    LeftMode = 0,
    LeftNoBorder,
    RightMode,
    RightNoBorder,
    FullScreen,
    FullScreenNoBorder
  } ShowMode;

  ShowMode mode = LeftMode;
  unsigned long wait_start = 0;

public:
  bool wait();

  void loop(FastLED_NeoMatrix *matrix, bool autoBrightness, int minBrightness,
            int maxBrightness, bool *pushed, int *timeout);
};