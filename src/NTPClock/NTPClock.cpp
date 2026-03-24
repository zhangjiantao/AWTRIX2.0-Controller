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

const char *controller_page1 =
    R"(<!DOCTYPE html>
<html lang='zxx'>
<head>
    <title>Awtrix Controller</title>

    <!-- Meta tags -->
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <meta http-equiv='X-UA-Compatible' content='ie=edge'>

    <style>
        body,
        html {
            margin: 0;
        }

        * {
            box-sizing: border-box;
            font-family: -apple-system, Menlo, Monaco, Consolas, serif;
        }

        .wrapper {
            width: 100%;
            padding-right: 15px;
            padding-left: 15px;
            margin-right: auto;
            margin-left: auto;
        }

        @media (min-width: 576px) {
            .wrapper {
                max-width: 540px;
            }
        }

        @media (min-width: 768px) {
            .wrapper {
                max-width: 720px;
            }
        }

        @media (min-width: 992px) {
            .wrapper {
                max-width: 960px;
            }
        }

        @media (min-width: 1200px) {
            .wrapper {
                max-width: 1140px;
            }
        }

        button,
        .btn,
        select {
            cursor: pointer;
        }

        .lgform #form-section {
            background-size: cover;
            -webkit-background-size: cover;
            position: relative;
            display: grid;
            align-items: center;
            min-height: 100vh;
            z-index: 0;
            padding: 15px 0;
        }

        .lgform #form-section:before {
            content: '';
            background: rgba(0, 0, 0, 0.65);
            position: absolute;
            top: 0;
            min-height: 100%;
            left: 0;
            right: 0;
            z-index: -1;
        }

        .lgform .logo {
            text-align: center;
            margin-bottom: 40px;
        }

        .lgform .logo a {
            font-size: 36px;
            color: #fff;
            line-height: 40px;
            font-weight: normal;
        }

        .lgform .login-form {
            background: #fff;
        }

        .lgform .login-form form input[type='text'],
        .lgform .login-form form input[type='password'] {
            -webkit-appearance: none;
            font-size: 20px;
            color: #777777;
            border: none;
            width: 100%;
            background-color: #fff;
            padding: 15px;
        }

        .lgform .login-form form input[type='text']:focus,
        .lgform .login-form form input[type='password']:focus {
            outline: none;
        }

        .lgform .login-form form button {
            -webkit-appearance: none;
            font-size: 20px;
            line-height: 25px;
            text-align: center;
            color: #ffffff;
            background: #2abda4;
            height: 55px;
            border: none;
            display: block;
            cursor: pointer;
            width: 100%;
            font-weight: bold;
            opacity: 0.8;
            transition: 0.3s ease-in-out;
        }

        .lgform .login-form form button:hover {
            background: #2abda4;
            transition: 0.2s ease-in-out;
            opacity: 1;
        }

        .lgform .login-form form button:focus {
            outline: none;
        }

        #screen {
            background-color: black;
            line-height: .6;
            text-align: center;
            letter-spacing: 2px;
            font-size: 2.5vw;
        }


    </style>
</head>

<body>

<section class='lgform'>
    <div id='form-section'>
        <div class='wrapper'>
            <div class='login-form'>
                <form id='lgn' style='display: none'>
                    <input id='pwd' type='password' name='tk' placeholder='Password'
                           autocomplete='new-password' required='required'/>
                    <button id='login'>Login</button>
                </form>
                <form id='ctl' style='display: none'>
                    <div style='display: flex; align-items: center; justify-content: center;'>
                        <button id='0'>L</button>
                        <button id='1'>M</button>
                        <button id='2'>R</button>
                    </div>
                    <div id='screen'></div>
                    <button id='update'>Update</button>
                    <button id='wakeup'>WakeUp</button>
                    <button id='reboot'>Reboot</button>
                    <button id='reset'>Reset!</button>
                    <button id='logout'>Logout</button>
                </form>
            </div>
        </div>
    </div>
</section>
<script>
    const lgn = document.querySelector('#lgn');
    const ctl = document.querySelector('#ctl');
    const pwd = document.querySelector('#pwd');
    const upd = document.querySelector('#update');
    const wk = document.querySelector('#wakeup');

    const lgn_status = )";

const char *controller_page2 =
    R"(;

    const spans = [];
    const scr = document.getElementById('screen');
    for (let i = 0; i < 8; i++) {
        for (let j = 0; j < 32; j++) {
            const span = document.createElement('span');
            span.innerText = '■';
            scr.appendChild(span);
            spans.push(span);
        }
        scr.append(document.createElement('br'));
    }

    const updatescr = async () => {
        let nf = new FormData();
        nf.append('id', 'screen');
        const resp = await fetch('', {
            method: 'POST',
            body: nf
        });
        const txt = await resp.text();
        const data = txt.split(',');
        if (data.length === 256) {
            for (let i = 0; i < 256; i++) {
                const c = parseInt(data[i]);
                const r = (c & 0xff0000) >> 16;
                const g = (c & 0xff00) >> 8;
                const b = c & 0xff;
                spans[i].style.color = 'rgb(' + r + ',' + g + ',' + b + ')';
            }
        }
    };
    scr.addEventListener("click", updatescr);

    const wakenuc = async (keep) => {
        let nf = new FormData();
        nf.append('id', 'wakeup');
        if (keep !== undefined) {
            nf.append('action', keep ? 'keep' : 'auto')
        }
        const resp = await fetch('', {
            method: 'POST',
            body: nf
        });
        const s = (await resp.text()).charAt(2);
        if (s === '0')
            wk.innerHTML = 'WakeUp: auto';
        else if (s === '1')
            wk.innerHTML = 'WakeUp: keep';
        else
            wk.innerHTML = 'WakeUp: sleeping';
    }

    if (lgn_status) {
        updatescr();
        wakenuc();
        ctl.style.display = 'block';
    } else {
        lgn.style.display = 'block';
    }


    function hash(str) {
        let hash = 0;
        for (let i = 0; i < str.length; i++) {
            const char = str.charCodeAt(i);
            hash = hash * 131;
            hash &= 0x7fffffff;
            hash += char;
            hash &= 0x7fffffff;
        }
        return hash.toString();
    };

    lgn.onsubmit = async (e) => {
        e.preventDefault();
        let nf = new FormData();
        nf.append('id', 'login');
        const tk = hash(pwd.value + '-' + Math.floor(Date.now() / 1000));
        localStorage.setItem('tk', tk);
        nf.append('tk', tk);
        const resp = await fetch('', {
            method: 'POST',
            body: nf
        });
        const txt = await resp.text();
        if (txt === 'OK') {
            updatescr();
            wakenuc();
            lgn.style.display = 'none';
            ctl.style.display = 'block';
        } else {
            localStorage.removeItem('tk');
            alert(txt);
        }
    };

    ctl.onsubmit = async (e) => {
        e.preventDefault();
        let nf = new FormData();
        nf.append('id', e.submitter.id);
        nf.append('tk', localStorage.getItem('tk'));

        if (e.submitter.id === 'logout') {
            localStorage.removeItem('tk');
            document.cookie = 'awtrix_token=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;';
            window.location.reload();
            return;
        }

        if (e.submitter.id === 'reset') {
            if (!confirm('reset?')) {
                return;
            }
        }

        if (e.submitter.id === 'update') {
            const ele = document.createElement('input');
            ele.type = 'file';
            ele.style.display = 'none';
            ele.addEventListener('change', e => {
                const file = e.target.files[0];
                nf.append('size', file.size);
                nf.append('update', file);
                upd.innerHTML = 'FLASHING...';
                scr.style.display = 'none';
                document.querySelectorAll('button').forEach(btn => btn.style.display = 'none');
                upd.style.display = 'block';
                upd.style.background = 'gray';
                upd.disabled = true;
                fetch('update', {
                    method: 'POST',
                    body: nf
                }).then(r => {
                    if (r.ok) {
                        r.text().then(txt => {
                            if (!txt.startsWith('OK')) {
                                upd.style.background = 'red';
                                upd.innerHTML = 'Err: ' + txt;
                            } else {
                                upd.style.background = 'green';
                                upd.innerHTML = 'Done.';
                            }
                        });
                    } else {
                        upd.innerHTML = 'Err: ' + r.status;
                    }
                });
            });
            ele.click();
        }

        if (e.submitter.id === 'wakeup') {
            curr = wk.innerHTML;
            wakenuc(curr.includes('sleeping') || curr.includes('auto'))
        }
    };

</script>
</body>

</html>
)";

class WakeUpTool {
  HTTPClient httpClient;
  WiFiClient wifiClient;

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

public:
  int check() {
    int ret = -2;
    if (httpClient.begin(wifiClient, "192.168.31.222", 8080, "/wake")) {
      int errCode = httpClient.GET();
      String res = httpClient.getString();
      if (errCode == HTTP_CODE_OK)
        ret = res.charAt(2) == '1';
      else
        ret = -1;
      httpClient.end();
    }
    return ret;
  }

  int keep(bool k) {
    int ret = -2;
    if (httpClient.begin(wifiClient, "192.168.31.222", 8080,
                         "/wake?keep=" + String(k))) {
      int errCode = httpClient.GET();
      String res = httpClient.getString();
      if (errCode == HTTP_CODE_OK)
        ret = res.charAt(2) == '1';
      else
        ret = -1;
      httpClient.end();
    }
    return ret;
  }

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
  uint32_t lastip = 0;

  void split(std::string s, std::string delimiter,
             const std::function<void(const std::string &)> &fn) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
      token = s.substr(pos_start, pos_end - pos_start);
      pos_start = pos_end + delim_len;
      fn(token);
    }

    fn(s.substr(pos_start));
  }

public:
  int gettoken() { return lasttk; }

  static long hash(const String &msg) {
    long h = 0;
    for (int i = 0; i < msg.length(); i++) {
      h = h * 131 + msg.charAt(i);
      h &= 0x7fffffff;
    }
    return h;
  }

  void clear() {
    lasttk = -1;
    lastip = 0;
  }

  bool checkToken(int tk = 0) {
    auto ip = matrix_server.client().remoteIP().v4();
    if ((ip & 0xff) == 192 && ((ip >> 8) & 0xff) == 168)
      return true;

    if (tk == 0) {
      auto cookie = matrix_server.header("Cookie");
      cookie.trim();
      split(cookie.c_str(), ";", [&tk](const std::string &sub) {
        String cstr = String(sub.c_str());
        cstr.trim();
        if (cstr.startsWith("awtrix_token=")) {
          cstr.replace("awtrix_token=", "");
          tk = cstr.toInt();
        }
      });
    }
    if (tk == 0)
      return false;

    if (tk == lasttk && ip == lastip)
      return true;

    auto ts = ntp.getEpochTime() - 8 * 60 * 60;
    for (int i = -5; i < 6; i++) {
      auto s = WiFi.psk() + "-" + String(ts + i);
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

  matrix_server.on("/", []() {
    extern unsigned long startup_time;
    time_t timep = startup_time;
    struct tm *p;
    p = localtime(&timep);
    char timestr[80];
    sprintf(timestr, "%d/%d/%d %02d:%02d:%02d\n", 1900 + p->tm_year,
            1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
    matrix_server.sendHeader("Connection", "close");
    matrix_server.send(200, "text/html", timestr);
  });

  matrix_server.on("/control", HTTP_GET, []() {
    matrix_server.sendHeader("Connection", "close");
    String content = controller_page1;
    content += String((int)token_checker.checkToken());
    content += controller_page2;
    matrix_server.send(200, "text/html", content);
  });

  matrix_server.on("/control", HTTP_POST, [&]() {
    matrix_server.sendHeader("Connection", "close");

    auto arg_id = matrix_server.arg("id");
    if (arg_id == "login" &&
        token_checker.checkToken(matrix_server.arg("tk").toInt())) {
      String cookie("awtrix_token=");
      cookie += String(token_checker.gettoken());
      matrix_server.sendHeader("Set-Cookie", cookie);
      matrix_server.send(200, "text/plain", "OK");
      return;
    }

    if (!token_checker.checkToken()) {
      matrix_server.send(200, "text/plain", "ERROR: BADTOKEN");
      return;
    }

    if (arg_id == "wakeup") {
      auto q = wake_up_tool.check();
      auto arg_action = matrix_server.arg("action");
      if (arg_action.isEmpty()) {
        matrix_server.send(200, "text/plain", "OK" + String(q));
        return;
      }
      if (arg_action == "keep") {
        if (q >= 0) {
          q = wake_up_tool.keep(true);
          matrix_server.send(200, "text/plain", "OK" + String(q));
          return;
        } else {
          int errCode;
          auto res1 = wake_up_tool.sendCommand(true, errCode);
          delay(500);
          auto res2 = wake_up_tool.sendCommand(false, errCode);
          delay(500);
          q = wake_up_tool.keep(true);
          matrix_server.send(200, "text/plain",
                             "OK" + String(q) + "," + res1 + "," + res2);
          return;
        }
      }
      if (arg_action == "auto") {
        if (q >= 0) {
          wake_up_tool.keep(false);
          matrix_server.send(200, "text/plain", "OK" + String(q));
          return;
        } else {
          int errCode;
          auto res1 = wake_up_tool.sendCommand(true, errCode);
          delay(500);
          auto res2 = wake_up_tool.sendCommand(false, errCode);
          delay(500);
          q = wake_up_tool.keep(false);
          matrix_server.send(200, "text/plain",
                             "OK" + String(q) + "," + res1 + "," + res2);
          return;
        }
      }
      matrix_server.send(200, "text/plain", "ERROR: BADREQ");
      return;
    }

    if (arg_id == "screen") {
      String r;
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 32; j++) {
          auto p = matrix_leds[matrix->XY(j, i)];
          uint32_t c = p.r << 16;
          c += p.g << 8;
          c += p.b;
          r += String(c);
          if (i != 7 || j != 31)
            r += ",";
        }
      }
      matrix_server.send(200, "text/html", r);
      return;
    }

    if (arg_id == "reboot") {
      matrix_server.send(200, "text/html", "OK");
      delay(500);
      ESP.restart();
    }

    if (arg_id == "reset") {
      matrix_wifi_manager.resetSettings();
      ESP.reset();
      matrix_server.send(200, "text/html", "OK");
    }

    if (arg_id == "logout") {
      token_checker.clear();
      matrix_server.sendHeader("Set-Cookie", "awtrix-token=");
      matrix_server.send(200, "text/html", "OK");
      return;
    }

    int id = arg_id.toInt();
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
        matrix_server.sendHeader("Connection", "close");
        matrix_server.send(200, "text/plain",
                           (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        if (!token_checker.checkToken()) {
          matrix_server.send(200, "text/plain", "ERROR: BADTOKEN");
          matrix_server.sendHeader("Connection", "close");
          yield();
          return;
        }

        auto totalSize = matrix_server.arg("size").toInt();
        if (totalSize == 0) {
          matrix_server.send(200, "text/plain", "ERROR: BADREQ");
          matrix_server.sendHeader("Connection", "close");
          yield();
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
          static int progress = 0;
          progress += (int)upload.currentSize;
          flashProgress(progress > totalSize ? totalSize : progress, totalSize);
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

  const char *hks[] = {"Cookie"};
  size_t sz = sizeof(hks) / sizeof(char *);
  matrix_server.collectHeaders(hks, sz); // ask server to track these headers
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