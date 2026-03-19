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
// #include <WiFi.h> // for WiFi shield
// #include <WiFi101.h> // for WiFi 101 shield or MKR1000

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

template <typename T, uint8_t offset, uint8_t width, uint8_t effect_type>
class EffectPlayer {
  int8_t n = 0;

public:
  bool playing() { return n > 0; }

  void begin(std::list<T *> *list) {
    if (playing()) {
      n = 0;
      return;
    }
    if (effect_type < 2)
      n = 8;
    else
      n = width;
    list->push_back(list->front());
    list->pop_front();
  }

  void render(std::list<T *> *list, int x, int y) {
#define EFFECT_SYNC_MODE 0
#if EFFECT_SYNC_MODE
    while (n >= 0) {
      matrix->fillRect(offset, 0, width, 8, COLOR565(0, 0, 0));
#endif
      switch (effect_type) {
      case 0:
        list->front()->render(x + offset, y + 8 - (8 - n));
        list->back()->render(x + offset, y - 8 + n);
        break;
      case 1:
        list->front()->render(x + offset, y - 8 + (8 - n));
        list->back()->render(x + offset, y + 8 - n);
        break;
      case 2:
        list->front()->render(x + offset + width - (width - n), y);
        list->back()->render(x + offset - width + n, y);
        break;
      case 3:
        list->front()->render(x + offset - width + (width - n), y);
        list->back()->render(x + offset + width - n, y);
        break;
      default:
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
  EffectPlayer<Widget, widget_offset, 14, 3> player;

public:
  HMClockCanvas(Widget *c, std::list<Widget *> *w) : clock(c), widgets(w) {}

  void render(int x, int y) override {
    if (!player.playing())
      widgets->front()->render(x + widget_offset, y);
    else
      player.render(widgets, x, y);
    clock->render(x + clock_offset, y);
  }

  void loop() override {
    clock->loop();
    widgets->front()->loop();
  }

  void event0() override { player.begin(widgets); }

  void event1() override {
    if (!widgets->front()->event1())
      clock->event1();
  }
};

class HMSClockCanvas : public Canvas {
  std::list<Widget *> *widgets;
  EffectPlayer<Widget, 0, 32, 1> player;

public:
  explicit HMSClockCanvas(std::list<Widget *> *ws) : widgets(ws) {}

  void render(int x, int y) override {
    if (!player.playing()) {
      widgets->front()->render(x, y);
    } else {
      player.render(widgets, x, y);
    }
  }

  void loop() override { widgets->front()->loop(); }

  void event0() override { player.begin(widgets); }

  void event1() override { widgets->front()->event1(); }
};

class MatrixImpl : public Matrix {
  std::list<Canvas *> *canvases;
  std::list<Task *> *tasks;
  EffectPlayer<Canvas, 0, 32, 1> player;

public:
  MatrixImpl() : tasks(Registry::GetTaskList()) {
    auto c = Registry::GetMainClock();
    auto LHM = new HMClockCanvas<0, 18>(c, Registry::GetWidgetList());
    auto RHM = new HMClockCanvas<13, 0>(c, Registry::GetWidgetList());
    auto HMS = new HMSClockCanvas(Registry::GetFullscreenWidgetList());
    canvases = new std::list<Canvas *>({HMS, LHM, HMS, RHM});
  }

  void render() override {
    if (!player.playing())
      canvases->front()->render(0, 0);
    else
      player.render(canvases, 0, 0);
  }

  void loop() override {
    canvases->front()->loop();
    for_each(tasks->begin(), tasks->end(), [&](Task *t) { t->run(); });
  }

  void event0() override { canvases->front()->event0(); }

  void event1() override { canvases->front()->event1(); }

  void event2() override { player.begin(canvases); }
};

NTPClock::NTPClock() { m = new MatrixImpl; }

bool NTPClock::should_wait_reconnect(const char *sv) {
  if (!strcmp(sv, "0.0.0.0"))
    return false;
  static unsigned long wait_start = 0;
  wait_start = wait_start ? wait_start : millis();
  if (millis() - wait_start < 2000)
    return true;
  return false;
}

void NTPClock::event(const bool *pushed, const int *timeout) {
  if (pushed[0] && millis() - timeout[0] < GLOBAL_DELAY) {
    dfmp3.playAdvertisement(9);
    m->event0();
  }
  if (pushed[1] && millis() - timeout[1] < GLOBAL_DELAY) {
    dfmp3.playAdvertisement(9);
    m->event1();
  }
  if (pushed[2] && millis() - timeout[2] < GLOBAL_DELAY) {
    dfmp3.playAdvertisement(9);
    m->event2();
  }

  if (pushed[1] && millis() - timeout[1] > 1900) {
    int count = 3;
    auto br = FastLED.getBrightness();
    matrix->setBrightness(30);
    while (!digitalRead(D4)) {
      matrix->clear();
      matrix->setTextColor(Color565(255, 0, 255));
      matrix->setCursor(1, 6);
      if (count) {
        matrix->printf("REBOOT  %d", count);
        matrix->show();
      } else {
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
    "padding:5%5%;text-align:center;text-decoration:none;display:inline-block;"
    "font-size:40px}</style></head><body><script>function btn_click(id){var "
    "t=document.createElement(\"form\");t.action=\"control\";t.method=\"post\";"
    "t.style.display=\"none\";t.target=\"iframe\";var "
    "opt=document.createElement(\"textarea\");opt.name=\"id\";opt.value=id;t."
    "appendChild(opt);document.body.appendChild(t);t.submit()}</script><iframe "
    "id=\"iframe\"name=\"iframe\"style=\"display:none;\"></iframe><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#f44336;\"onclick=\"btn_click(0)\">左按键</button><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#008CBA;\"onclick=\"btn_click(1)\">中按键</button><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#f44336;\"onclick=\"btn_click(2)\">右按键</button><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#f44336;\"onclick=\"btn_click(3)\">打开开关</button><button "
    "type=\"button\"class=\"button0\"style=\"background-color: "
    "#f44336;\"onclick=\"btn_click(4)\">关闭开关</button></body></html>";

const char *method_open =
    "{\"sequence\":\"0000000000000\",\"deviceid\":\"1001202f79\","
    "\"selfApikey\":\"00000000-0000-0000-0000-000000000000\",\"iv\":"
    "\"MDAwMDAwMDAwMDAwMDAwMA==\",\"encrypt\":true,\"data\":"
    "\"LxTLInQuyqzSHqgPbldQTA==\"}";
const char *method_close =
    "{\"sequence\":\"0000000000000\",\"deviceid\":\"1001202f79\","
    "\"selfApikey\":\"00000000-0000-0000-0000-000000000000\",\"iv\":"
    "\"MDAwMDAwMDAwMDAwMDAwMA==\",\"encrypt\":true,\"data\":"
    "\"IyuddWiyKYw54CrngLXdw+29BvxYDeWdwSAwmYqdMCQ=\"}";

class HttpUtils {
  HTTPClient httpClient;
  WiFiClient wifiClient;

public:
  String sendCommand(bool open, int &errCode) {
    String res;
    int httpCode;

    if (httpClient.begin(wifiClient, "192.168.31.238", 8081, "/zeroconf/switch")) {
      if (open) {
        httpCode = httpClient.POST(method_open);
      } else {
        httpCode = httpClient.POST(method_close);
      }

      errCode = httpCode;
      res = httpClient.getString();
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          errCode = 0;
        }
      }
      httpClient.end();
    }
    return res;
  }
};

HttpUtils httpx;

void NTPClock::handle() {
  static bool start = false;
  if (start)
    return;

  matrix_server.on("/control", HTTP_GET, []() {
    matrix_server.sendHeader("Connection", "close");
    matrix_server.send(200, "text/html", controller_page);
  });

  matrix_server.on("/control", HTTP_POST, [&]() {
    matrix_server.sendHeader("Connection", "close");

    bool pushed[3]{false, false, false};
    int timeout[3]{0, 0, 0};
    for (int i = 0; i < matrix_server.args(); i++) {
      if (matrix_server.argName(i) == "id") {
        int id = matrix_server.arg(i).toInt();
        if (id < 3 && id >= 0) {
          pushed[id] = true;
          timeout[id] = millis();
          event(pushed, timeout);
          matrix_server.send(200, "text/plain", "ok");
        }
        if (id == 3) {
          int errCode = 0;
          auto res = httpx.sendCommand(true, errCode);
          res.trim();
          matrix_server.send(200, "text/plain", String(errCode) + res);
        }
        if (id == 4) {
          int errCode = 0;
          auto res = httpx.sendCommand(false, errCode);
          res.trim();
          matrix_server.send(200, "text/plain", String(errCode) + res);
        }
      }
    }
  });

  start = true;
}

void NTPClock::loop(bool *pushed, int *timeout) {
  event(pushed, timeout);
  handle();
  matrix->clear();
  m->render();
  m->loop();
  matrix->show();
  delay(GLOBAL_DELAY);
}