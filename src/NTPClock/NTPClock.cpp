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

#include "NTPClock.h"

#include <algorithm>
#include <list>
#include <map>
#include <queue>
#include <type_traits>

namespace Registry {
std::list<Widget *> *GetWidgetList() {
  static std::list<Widget *> *NTPClockWidgets = nullptr;
  if (!NTPClockWidgets)
    NTPClockWidgets = new std::list<Widget *>;
  return NTPClockWidgets;
}

std::list<Widget *> *GetFullscreenWidgetList() {
  static std::list<Widget *> *NTPClockFullscreenWidgets = nullptr;
  if (!NTPClockFullscreenWidgets)
    NTPClockFullscreenWidgets = new std::list<Widget *>;
  return NTPClockFullscreenWidgets;
}

std::list<Task *> *GetTaskList() {
  static std::list<Task *> *NTPClockTasks = nullptr;
  if (!NTPClockTasks)
    NTPClockTasks = new std::list<Task *>;
  return NTPClockTasks;
}
} // namespace Registry

template <typename T, uint8_t offset, uint8_t width> class EffectPlayer {
  uint8_t animation = 0;
  int8_t n = 0;

public:
  bool playing() { return n > 0; }

  void begin(std::list<T *> *list) {
    if (playing()) {
      n = 0;
      return;
    }
    animation = (animation + 1) % 2;
    if (animation < 2)
      n = 8;
    else
      n = width;
    list->push_back(list->front());
    list->pop_front();
  }

  void render(std::list<T *> *list, FastLED_NeoMatrix *matrix, int x, int y) {
#define EFFECT_SYNC_MODE 0
#if EFFECT_SYNC_MODE
    while (n >= 0) {
      matrix->fillRect(offset, 0, width, 8, matrix->Color(0, 0, 0));
#endif
      switch (animation) {
      case 0:
        list->front()->render(matrix, x + offset, y + 8 - (8 - n));
        list->back()->render(matrix, x + offset, y - 8 + n);
        break;
      case 1:
        list->front()->render(matrix, x + offset, y - 8 + (8 - n));
        list->back()->render(matrix, x + offset, y + 8 - n);
        break;
      case 2:
        list->front()->render(matrix, x + offset + width - (width - n), y);
        list->back()->render(matrix, x + offset - width + n, y);
        break;
      case 3:
        list->front()->render(matrix, x + offset - width + (width - n), y);
        list->back()->render(matrix, x + offset + width - n, y);
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
class HMClockCanvas : public Canvas {
  Widget *clock;
  std::list<Widget *> *widgets;
  EffectPlayer<Widget, widget_offset, 14> player;

public:
  HMClockCanvas(Widget *c, std::list<Widget *> *w) : clock(c), widgets(w) {}

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    clock->render(matrix, x + clock_offset, y);
    if (!player.playing())
      widgets->front()->render(matrix, x + widget_offset, y);
    else
      player.render(widgets, matrix, x, y);
  }

  void loop(FastLED_NeoMatrix *matrix) override {
    clock->loop(matrix);
    widgets->front()->loop(matrix);
  }

  void event0(FastLED_NeoMatrix *matrix) override { player.begin(widgets); }

  void event1(FastLED_NeoMatrix *matrix) override {
    if (!widgets->front()->event1(matrix))
      clock->event1(matrix);
  }
};

class HMSClockCanvas : public Canvas {
  std::list<Widget *> *widgets;
  EffectPlayer<Widget, 0, 32> player;

public:
  HMSClockCanvas(std::list<Widget *> *ws) : widgets(ws) {}

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    if (!player.playing()) {
      widgets->front()->render(matrix, x, y);
    } else {
      player.render(widgets, matrix, x, y);
    }
  }

  void loop(FastLED_NeoMatrix *matrix) override {
    widgets->front()->loop(matrix);
  }

  void event0(FastLED_NeoMatrix *matrix) override { player.begin(widgets); }

  void event1(FastLED_NeoMatrix *matrix) override {
    widgets->front()->event1(matrix);
  }
};

class MatrixImpl : public Matrix {
  std::list<Canvas *> *canvases;
  std::list<Task *> *tasks;
  EffectPlayer<Canvas, 0, 32> player;

public:
  MatrixImpl() : tasks(Registry::GetTaskList()) {
    auto c = Registry::GetMainClock();
    auto LHM = new HMClockCanvas<0, 18>(c, Registry::GetWidgetList());
    auto RHM = new HMClockCanvas<13, 0>(c, Registry::GetWidgetList());
    auto HMS = new HMSClockCanvas(Registry::GetFullscreenWidgetList());
    canvases = new std::list<Canvas *>({LHM, HMS, RHM, HMS});
  }

  void render(FastLED_NeoMatrix *matrix) override {
    if (!player.playing())
      canvases->front()->render(matrix, 0, 0);
    else
      player.render(canvases, matrix, 0, 0);
  }

  void loop(FastLED_NeoMatrix *m) override {
    canvases->front()->loop(m);
    for_each(tasks->begin(), tasks->end(), [&](Task *t) { t->run(m); });
  }

  void event0(FastLED_NeoMatrix *matrix) override {
    canvases->front()->event0(matrix);
  }
  void event1(FastLED_NeoMatrix *matrix) override {
    canvases->front()->event1(matrix);
  }
  void event2(FastLED_NeoMatrix *matrix) override { player.begin(canvases); }
};

NTPClock::NTPClock() { m = new MatrixImpl; }

bool NTPClock::shoud_wait_reconnect(const char *server) {
  if (!strcmp(server, "0.0.0.0"))
    return false;
  static unsigned long wait_start = 0;
  wait_start = wait_start ? wait_start : millis();
  if (millis() - wait_start < 2000)
    return true;
  return false;
}

void NTPClock::event(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout) {
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
    int count = 3;
    auto br = FastLED.getBrightness();
    matrix->setBrightness(30);
    while (!digitalRead(D4)) {
      matrix->clear();
      matrix->setTextColor(matrix->Color(255, 0, 255));
      matrix->setCursor(1, 6);
      if (count) {
        matrix->clear();
        matrix->printf("REBOOT  %d", count);
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
    matrix->setBrightness(br);
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

void NTPClock::handle(FastLED_NeoMatrix *matrix) {
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

void NTPClock::loop(FastLED_NeoMatrix *matrix, bool *pushed, int *timeout) {
  event(matrix, pushed, timeout);
  handle(matrix);
  matrix->clear();
  m->render(matrix);
  m->loop(matrix);
  matrix->show();
  delay(GLOBAL_DELAY);
}