#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <list>
#include <time.h>
#include <vector>

#define GLOBAL_DELAY 20
#define GLOBAL_DELAY_PERSEC (1000 / (GLOBAL_DELAY + 50))
#define GLOBAL_DELAY_PERMIN ((1000 * 60) / (GLOBAL_DELAY + 50))

class Matrix {
public:
  virtual void render(FastLED_NeoMatrix *matrix) = 0;
  virtual void loop(FastLED_NeoMatrix *matrix) {}
  virtual void event0(FastLED_NeoMatrix *matrix) {}
  virtual void event1(FastLED_NeoMatrix *matrix) {}
  virtual void event2(FastLED_NeoMatrix *matrix) {}
};

class Canvas {
public:
  virtual void render(FastLED_NeoMatrix *matrix, int x, int y) = 0;
  virtual void loop(FastLED_NeoMatrix *matrix) {}
  virtual void event0(FastLED_NeoMatrix *matrix) {}
  virtual void event1(FastLED_NeoMatrix *matrix) {}
};

class Widget {
public:
  virtual void render(FastLED_NeoMatrix *matrix, int x, int y) = 0;
  virtual void loop(FastLED_NeoMatrix *matrix) {}
  virtual void event1(FastLED_NeoMatrix *matrix) {}
};

class Task {
public:
  virtual bool run(FastLED_NeoMatrix *matrix) = 0;
};

class LocalClock {
  Matrix *m;

public:
  LocalClock();

  bool wait(const char *server);

  void handle(FastLED_NeoMatrix *matrix);

  void event(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout);

  void loop(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout);
};