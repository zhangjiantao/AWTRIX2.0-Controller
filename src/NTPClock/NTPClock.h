#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>

#include <ctime>
#include <list>
#include <vector>

extern WiFiManager matrix_wifi_manager;
extern ESP8266WebServer matrix_server;
extern ESP8266WiFiClass WiFi;
extern CRGB matrix_leds[256];
extern FastLED_NeoMatrix *matrix;

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
    (r) = random8(255);                                                        \
    (g) = random8(255);                                                        \
    (b) = random8(255);                                                        \
  } while (((r) + (g) + (b) < 256))

#define Color565(r, g, b)                                                      \
  (((uint16_t)((r)&0xF8) << 8) | ((uint16_t)((g)&0xFC) << 3) | ((b) >> 3))

class Matrix {
public:
  virtual void render() = 0;
  virtual void loop() {}
  virtual void event0() {}
  virtual void event1() {}
  virtual void event2() {}
};

class Canvas {
public:
  virtual void render(int x, int y) = 0;
  virtual void loop() {}
  virtual void event0() {}
  virtual void event1() {}
};

class Widget {
public:
  virtual void render(int x, int y) = 0;
  virtual void loop() {}
  virtual bool event1() { return false; }
};

class Task {
public:
  virtual bool run() = 0;
};

namespace Registry {
std::list<Widget *> *GetWidgetList();
std::list<Widget *> *GetFullscreenWidgetList();
std::list<Task *> *GetTaskList();

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

  static bool shoud_wait_reconnect(const char *server);

  void handle();

  void event(const bool *pushed, const int *timeout);

  void loop(bool *pushed, int *timeout);
};