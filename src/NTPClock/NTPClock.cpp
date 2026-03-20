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
    R"(<!DOCTYPE html> <html lang=zxx> <head> <title>Awtrix Controller</title>  <meta charset=UTF-8> <meta name=viewport content="width=device-width,initial-scale=1"> <meta http-equiv=X-UA-Compatible content="ie=edge"> <style>html{scroll-behavior:smooth}body,html{margin:0;padding:0;color:#585858}*{box-sizing:border-box;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Oxygen-Sans,Ubuntu,Cantarell,"Helvetica Neue",sans-serif}.wrapper{width:100%;padding-right:15px;padding-left:15px;margin-right:auto;margin-left:auto}@media (min-width:576px){.wrapper{max-width:540px}}@media (min-width:768px){.wrapper{max-width:720px}}@media (min-width:992px){.wrapper{max-width:960px}}@media (min-width:1200px){.wrapper{max-width:1140px}}button,input,select{-webkit-appearance:none;outline:0}button,.btn,select{cursor:pointer}a{text-decoration:none}h1,h2,h3,h4,h5,h6,p,ul,ol{margin:0;padding:0}.lgform #form-section{background-size:cover;-webkit-background-size:cover;-moz-background-size:cover;-o-background-size:cover;-ms-background-size:cover;position:relative;display:grid;align-items:center;min-height:100vh;z-index:0;padding:15px 0}.lgform #form-section:before{content:"";background:rgba(0,0,0,.65);position:absolute;top:0;min-height:100%;left:0;right:0;z-index:-1}.lgform .logo{text-align:center;margin-bottom:40px}.lgform .logo a{font-size:36px;color:#fff;line-height:40px;font-weight:400}.lgform .login-form{background:#fff;max-width:600px;margin:0 auto;box-shadow:0 9px 24px 5px rgba(0,0,0,.04)}.lgform .login-form form input[type=text],.lgform .login-form form input[type=password]{-webkit-appearance:none;font-style:normal;font-weight:400;font-size:16px;color:#777;border:none;width:100%;background-color:rgba(88,83,152,.03);padding:24px}.lgform .login-form form input[type=text]{border-right:1px solid #e8e8e8}.lgform .login-form form input[type=text]:focus,.lgform .login-form form input[type=password]:focus{outline:0;background-color:#fff;box-shadow:none}.lgform .login-form form button{-webkit-appearance:none;font-size:14px;line-height:25px;text-align:center;color:#fff;background:#2abda4;height:55px;border:none;display:block;cursor:pointer;width:100%;font-weight:700;opacity:.8;transition:.3s ease-in-out}.lgform .login-form form button:hover{background:#2abda4;transition:.3s ease-in-out;opacity:1}.lgform .login-form form button:focus{outline:0}</style><body> <section class=lgform> <div id=form-section> <div class=wrapper> <div class=login-form> <form id=lgn> <input id=pwd type=password name=tk placeholder=Password autocomplete=new-password required/> <button id=login>Login</button> </form> <form id=ctl style=display:none> <div style=display:flex;align-items:center;justify-content:center> <button id=0>L</button> <button id=1>M</button> <button id=2>R</button> </div> <button id=update>Update</button> <button id=wakeup>WakeUp</button> </form> </div> </div> </div> </section> <script>function hash(str){let hash=0;for(let i=0;i < str.length;i++){const char=str.charCodeAt(i);hash=hash * 131;hash &=0x7fffffff;hash +=char;hash &=0x7fffffff;}return hash.toString();};const lgn=document.querySelector("#lgn");const ctl=document.querySelector("#ctl");const pwd=document.querySelector("#pwd");const upd=document.querySelector("#update");lgn.onsubmit=async(e)=>{e.preventDefault();let nf=new FormData();nf.append("id","login");const tk=hash(pwd.value + "-" + Math.floor(Date.now()/ 1000));localStorage.setItem("tk",tk);nf.append("tk",tk);const resp=await fetch('',{method:'POST',body:nf});const txt=await resp.text();if(txt==="OK"){lgn.style.display="none";ctl.style.display="block";}else{localStorage.removeItem("tk");alert(txt);}};ctl.onsubmit=async(e)=>{e.preventDefault();let nf=new FormData();nf.append("id",e.submitter.id);nf.append("tk",localStorage.getItem("tk"));if(e.submitter.id==="update"){const ele=document.createElement("input");ele.type="file";ele.style.display="none";ele.addEventListener("change",e=>{const file=e.target.files[0];nf.append('update',file);upd.disabled=true;upd.innerHTML="FLASHING...";fetch('update',{method:'POST',body:nf}).then(r=>{if(r.ok){r.text().then(txt=>{if(!txt.startsWith("OK")){upd.innerHTML="Err:" + txt;}else{upd.innerHTML="Done.";upd.disabled=false;}});}else{upd.innerHTML="Err:" + r.status;}});});ele.click();}else{const resp=await fetch('',{method:'POST',body:nf});const txt=await resp.text();if(!txt.startsWith("OK")){localStorage.removeItem("tk");alert(txt);}}};window.onload=()=>{if(!localStorage.getItem("tk")){return;}let nf=new FormData();nf.append("id","login");nf.append("tk",localStorage.getItem("tk"));fetch('',{method:'POST',body:nf}).then((r)=>{console.log(r);if(r.ok){r.text().then((txt)=>{if(txt==="OK"){lgn.style.display="none";ctl.style.display="block";}else{localStorage.removeItem("tk");}});}});}</script>)";

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

class WakeUpTool {
  HTTPClient httpClient;
  WiFiClient wifiClient;

public:
  String sendCommand(bool open, int &errCode) {
    String res;

    if (httpClient.begin(wifiClient, "192.168.31.238", 8081,
                         "/zeroconf/switch")) {
      errCode = httpClient.POST(open ? method_open : method_close);
      res = httpClient.getString();
      if (errCode > 0) {
        if (errCode == HTTP_CODE_OK || errCode == HTTP_CODE_MOVED_PERMANENTLY) {
          errCode = 0;
        }
      }
      httpClient.end();
    }
    return res;
  }
};

WakeUpTool wake_up_tool;

class TokenChecker {
  int lasttk = -1;
  String lastip = "";
  const char *pwd = "qq279375561-";

public:
  static long hash(const String &msg) {
    long h = 0;
    for (int i = 0; i < msg.length(); i++) {
      h = h * 131 + msg.charAt(i);
      h &= 0x7fffffff;
    }
    return h;
  }

  bool checkToken() {
    auto tk = matrix_server.arg("tk").toInt();
    if (tk == 0)
      return false;

    auto ip = matrix_server.client().remoteIP().toString();
    if (tk == lasttk && ip == lastip)
      return true;

    auto ts = ntp.getEpochTime() - 8 * 60 * 60;
    for (int i = -5; i < 6; i++) {
      auto s = String(pwd) + String(ts + i);

      if (hash(s) == tk) {
        lasttk = tk;
        lastip = ip;
        return true;
      }
    }
    return false;
  }
};

TokenChecker token_checker;

extern void flashProgress(unsigned int progress, unsigned int total);
void NTPClock::setup() {
  static bool start = false;
  if (start)
    return;

  matrix_server.on("/control", HTTP_GET, []() {
    matrix_server.sendHeader("Connection", "close");
    matrix_server.send(200, "text/html", controller_page);
  });

  matrix_server.on("/control", HTTP_POST, [&]() {
    matrix_server.sendHeader("Connection", "close");

    if (!token_checker.checkToken()) {
      matrix_server.send(200, "text/plain", "ERROR: BADTOKEN");
      return;
    }

    if (matrix_server.arg("id") == "login") {
      matrix_server.send(200, "text/plain", "OK");
      return;
    }

    if (matrix_server.arg("id") == "wakeup") {
      int errCode;
      auto res = wake_up_tool.sendCommand(true, errCode);
      delay(500);
      auto res2 = wake_up_tool.sendCommand(false, errCode);
      matrix_server.send(200, "text/plain", "OK," + res + "," + res2);
      return;
    }

    int id = matrix_server.arg("id").toInt();
    if (id < 3 && id >= 0) {
      bool pushed[3]{false, false, false};
      int timeout[3]{0, 0, 0};
      pushed[id] = true;
      timeout[id] = millis();
      event(pushed, timeout);
      matrix_server.send(200, "text/plain", "OK");
      return;
    }

    matrix_server.send(200, "text/plain", "ERROR: BADREQ");
  });

  matrix_server.on(
      "/update", HTTP_POST,
      []() {
        if (!token_checker.checkToken()) {
          matrix_server.send(200, "text/plain", "ERROR: BADTOKEN");
          return;
        }

        matrix_server.sendHeader("Connection", "close");
        matrix_server.send(200, "text/plain",
                           (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        if (!token_checker.checkToken()) {
          matrix_server.send(200, "text/plain", "ERROR: BADTOKEN");
          return;
        }

        HTTPUpload &upload = matrix_server.upload();

        if (upload.status == UPLOAD_FILE_START) {
          Serial.setDebugOutput(true);

          uint32_t maxSketchSpace =
              (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace)) { // start with max available size
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          matrix->clear();
          flashProgress((int)upload.currentSize, (int)upload.buf);
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) { // true to set the size to the current
                                  // progress
            matrix_server.send(200, "text/plain",
                               (Update.hasError()) ? "FAIL" : "OK");
          } else {
            Update.printError(Serial);
          }
          Serial.setDebugOutput(false);
        }
        yield();
      });

  // matrix_server.on("/", HTTP_GET, []() {
  //   matrix_server.sendHeader("Connection", "close");
  //   matrix_server.send(200, "text/html", "OK");
  // });

  matrix_server.on("/reset", HTTP_GET, []() {
    if (!token_checker.checkToken()) {
      matrix_server.send(200, "text/plain", "ERROR: BADTOKEN");
      return;
    }
    matrix_wifi_manager.resetSettings();
    ESP.reset();
    matrix_server.send(200, "text/html", "OK");
  });

  matrix_server.begin();

  start = true;
}

void NTPClock::loop(bool *pushed, int *timeout) {
  event(pushed, timeout);
  setup();
  matrix->clear();
  m->render();
  m->loop();
  matrix->show();
  delay(GLOBAL_DELAY);
}