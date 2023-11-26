#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <FastLED_NeoMatrix.h>
#include <WiFiClient.h>

#include <cstdint>
#include <list>
#include <vector>

#include "../NTPClock.h"

#define DDNS_UPDATE_TIME 2
#define HOSTNAME "llvm.x3322.net"
#define AUTH_NAME "Authorization"
#define AUTH_VALUE "Basic cm9vdDp6aGFuZ2ppYW50YW8="
#define IP_API "http://ip.3322.net/"
#define UPDATE_API                                                             \
  "http://members.3322.net/dyndns/update?hostname=" HOSTNAME "&myip="

class HttpUtils {
  HTTPClient httpClient;
  WiFiClient wifiClient;

public:
  String httpRequest(const String &url, int &errCode, bool auth = false) {
    String res;

    if (httpClient.begin(wifiClient, url)) {
      if (auth)
        httpClient.addHeader(AUTH_NAME, AUTH_VALUE);

      int httpCode = httpClient.GET();
      errCode = httpCode;
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          errCode = 0;
          res = httpClient.getString();
        }
      }
      httpClient.end();
    }
    return res;
  }
};

class DDNS_TASK : public Task {
  HttpUtils http;

  bool update() {
    if (!WiFi.isConnected())
      return false;

    int errCode = 0;
    l_ip = http.httpRequest(IP_API, errCode);
    l_ip.trim();
    if (errCode) {
      LOG(Serial.println("can not get current ip"));
      return false;
    }

    IPAddress resolve_ip;
    if (!WiFi.hostByName(HOSTNAME, resolve_ip, 1000)) {
      LOG(Serial.println("resolve hostname failed"));
      return false;
    }

    r_ip = resolve_ip.toString();
    LOG(Serial.printf("resolve %s: %s\n", HOSTNAME, r_ip.c_str()));
    if (r_ip == "255.255.255.255")
      return false;

    if (l_ip != r_ip) {
      LOG(Serial.printf("update from %s to %s\n", r_ip.c_str(), l_ip.c_str()));
      auto res = http.httpRequest(String(UPDATE_API) + l_ip, errCode, true);
      res.trim();
      if (errCode != 0 ||
          (!res.startsWith("good") && !res.startsWith("nochg"))) {
        LOG(Serial.printf("failed, code %d, res %s\n", errCode, res.c_str()));
        return false;
      }
      LOG(Serial.println("done"));
    }
    return true;
  }

public:
  static String r_ip, l_ip;
  static unsigned long next_update_ts;
  static bool last_update_succ;

  bool run() override {
    if (!ntp.isTimeSet())
      return false;
    auto ts = ntp.getEpochTime();
    if (next_update_ts == 0)
      next_update_ts = ts;
    if (ts == next_update_ts) {
      next_update_ts += DDNS_UPDATE_TIME * 32;
      LOG(Serial.printf("ts %lu\n", ts));
      LOG(Serial.printf("next_update_ts %lu\n", next_update_ts));
      last_update_succ = update();
      return last_update_succ;
    }
    return false;
  }
};

String DDNS_TASK::r_ip{"undefined"}, DDNS_TASK::l_ip{"undefined"};
unsigned long DDNS_TASK::next_update_ts = 0;
bool DDNS_TASK::last_update_succ = false;

class DDNS : public Widget {
  uint16_t c_succe = Color565(0, 255, 64);
  uint16_t c_error = Color565(255, 0, 0);
  uint16_t update_progress_fc = Color565(230, 230, 230);
  uint16_t update_progress_bc = Color565(130, 130, 130);
  uint8_t animation_progress = 0;
  uint16_t animation_color = c_error;
  uint8_t update_progress = 0;

  unsigned frame_delay = 0;

public:
  void loop() override {
    static unsigned _delay = 0;
    _delay = ++_delay % (frame_delay + 1);
    if (_delay == 0) {
      auto ts = ntp.getEpochTime();
      update_progress = 32 - ((DDNS_TASK::next_update_ts - ts) / DDNS_UPDATE_TIME);
      if (update_progress == 0) {
        animation_progress = 0;
        animation_color = DDNS_TASK::last_update_succ ? c_succe : c_error;
      } else {
        animation_progress++;
      }
    }
  }

  bool event1() override {
    frame_delay = ++frame_delay % 5;
    return true;
  }

  void render(int x, int y) override {
    auto black = Color565(0, 0, 0);
    matrix->fillRect(x, y, 32, 8, black);
    matrix->setTextColor(animation_color);
    matrix->setCursor(x - animation_progress + 32, y + 6);

    char buff[64];
    int16_t x1, y1;
    uint16_t w, h;
    snprintf(buff, 64, "%s -> %s", DDNS_TASK::r_ip.c_str(),
             DDNS_TASK::l_ip.c_str());
    matrix->getTextBounds(buff, 0, 0, &x1, &y1, &w, &h);
    matrix->printf("%s -> %s", DDNS_TASK::r_ip.c_str(),
                   DDNS_TASK::l_ip.c_str());

    if (animation_progress > w)
      animation_progress = 0;
    matrix->drawLine(x, y + 7, x + 32, y + 7, update_progress_bc);
    matrix->drawLine(x, y + 7, x + update_progress, y + 7, update_progress_fc);
  }
};

static Registry::RegisterFullscreenWidget<DDNS> X;
static Registry::RegisterTask<DDNS_TASK> Y;