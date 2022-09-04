#include <ESP8266WebServer.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>

#include <ctime>
#include <list>
#include <vector>

extern WiFiManager wifiManager;
extern ESP8266WebServer server;

#include <DFMiniMp3.h>
class Mp3Notify;
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
extern DfMp3 dfmp3;

#include <NTPClient.h>
extern NTPClient ntp;

#define GLOBAL_FPS 60
#define GLOBAL_DELAY (1000 / GLOBAL_FPS)

#define RANDOM_RGB(r, g, b)                                                    \
  do {                                                                         \
    r = random8(255);                                                          \
    g = random8(255);                                                          \
    b = random8(255);                                                          \
  } while ((r + g + b < 256))

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
  virtual bool event1(FastLED_NeoMatrix *matrix) { return false; }
};

class Task {
public:
  virtual bool run(FastLED_NeoMatrix *matrix) = 0;
};

namespace Registry {
std::list<Widget *> *GetWidgetList();
std::list<Widget *> *GetFullscreenWidgetList();
std::list<Task *> *GetTaskList();

template <typename T> struct RegisterClock {
  RegisterClock() { SetMainClock(new T); }
};

template <typename T> struct RegisterWidget {
  RegisterWidget() { GetWidgetList()->push_back(new T); }
};

template <typename T> struct RegisterFullscreenWidget {
  RegisterFullscreenWidget() { GetFullscreenWidgetList()->push_back(new T); }
};

template <typename T> struct RegisterTask {
  RegisterTask() { GetTaskList()->push_back(new T); }
};

Widget *GetMainClock();
} // namespace Registry

class NTPClock {
  Matrix *m;

public:
  NTPClock();

  bool shoud_wait_reconnect(const char *server);

  void handle(FastLED_NeoMatrix *matrix);

  void event(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout);

  void loop(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout);
};