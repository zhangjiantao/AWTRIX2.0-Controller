#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <FastLED_NeoMatrix.h>
#include <WiFiClient.h>

#include <cstdint>
#include <list>
#include <vector>

#include "../NTPClock.h"

#define DDNS_UPDATE_TIME 2

#define DUCKDNS_HOSTNAME "llvm.duckdns.org"
#define IP_API "http://ip.3322.net/"
#define DUCKDNS_API                                                            \
  "http://www.duckdns.org/"                                                    \
  "update?domains=llvm&token=54d696ca-7d9a-4633-97be-2fd4e9fb875f"

#define CLOUDNS_HOSTNAME "llvm.abrdns.com"
#define CLOUDNS_API                                                            \
  "http://ipv4.cloudns.net/api/dynamicURL/"                                    \
  "?q="                                                                        \
  "MTIxMjYyMTc6NzQ2NDIxMDk3OjZkMWI1MWFmNGY4Y2M1MjNiYTc1MGQxMzJkZjljYzdiMzdiMm" \
  "ZlZWFiNTI1ZmQwZjEyMGUyZDRhYzVlOWU5ZTY"

class HttpUtils {
  HTTPClient httpClient;
  WiFiClient wifiClient;

public:
  String httpRequest(const String &url, int &errCode, bool auth = false) {
    String res;

    if (httpClient.begin(wifiClient, url)) {
      int httpCode = httpClient.GET();
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

HttpUtils http;

class DDNS_TASK : public Task {
  static String resolve(const char *host) {
    IPAddress resolve_ip;
    if (!WiFi.hostByName(host, resolve_ip, 1000)) {
      LOG(Serial.println("resolve hostname failed"));
      return "";
    }
    return resolve_ip.toString();
  }

  static bool update_api(const char *api) {
    int errCode = 0;

    auto res = http.httpRequest(api, errCode);
    res.trim();
    if (errCode != 0 || (!res.startsWith("OK"))) {
      LOG(Serial.printf("failed, code %d, res %s\n", errCode, res.c_str()));
      return false;
    }
    LOG(Serial.println("done"));
    return true;
  }

  static bool update(const String &wan, const char *host, const char *api) {
    String rip = resolve(host);
    if (rip.isEmpty() || rip == "255.255.255.255")
      return false;

    if (wan != rip)
      return update_api(api);
    LOG(Serial.printf("no update needed\n"));
    return true;
  }

public:
  static unsigned long next_update_ts;
  static bool duckdns_ok, cloudns_ok;

  bool run() override {
    if (!WiFi.isConnected())
      return false;
    if (!ntp.isTimeSet())
      return false;

    auto ts = ntp.getEpochTime();
    if (next_update_ts == 0)
      next_update_ts = ts;
    if (ts == next_update_ts) {
      next_update_ts += DDNS_UPDATE_TIME * 32;
      LOG(Serial.printf("ts %lu\n", ts));
      LOG(Serial.printf("next_update_ts %lu\n", next_update_ts));

      int err = 0;

      String wan = http.httpRequest(IP_API, err);
      wan.trim();
      if (err) {
        LOG(Serial.println("can not get current ip"));
        return false;
      }

      duckdns_ok = update(wan, DUCKDNS_HOSTNAME, DUCKDNS_API);
      cloudns_ok = update(wan, CLOUDNS_HOSTNAME, CLOUDNS_API);
      return duckdns_ok && cloudns_ok;
    }
    return false;
  }
};

unsigned long DDNS_TASK::next_update_ts = 0;
bool DDNS_TASK::duckdns_ok = false;
bool DDNS_TASK::cloudns_ok = false;

//
// class DDNS : public Widget {
//   uint16_t c_succe = Color565(0, 255, 64);
//   uint16_t c_error = Color565(255, 0, 0);
//   uint16_t update_progress_fc = Color565(230, 230, 230);
//   uint16_t update_progress_bc = Color565(130, 130, 130);
//   uint8_t animation_progress = 0;
//   uint16_t animation_color = c_error;
//   uint8_t update_progress = 0;
//
//   unsigned frame_delay = 0;
//
// public:
//   void loop() override {
//     static unsigned _delay = 0;
//     _delay = ++_delay % (frame_delay + 1);
//     if (_delay == 0) {
//       auto ts = ntp.getEpochTime();
//       update_progress =
//           32 - ((DDNS_TASK::next_update_ts - ts) / DDNS_UPDATE_TIME);
//       if (update_progress == 0) {
//         animation_progress = 0;
//         animation_color = (DDNS_TASK::duckdns_ok && DDNS_TASK::cloudns_ok)
//                               ? c_succe
//                               : c_error;
//       } else {
//         animation_progress++;
//       }
//     }
//   }
//
//   bool event1() override {
//     frame_delay = ++frame_delay % 5;
//     return true;
//   }
//
//   void render(int x, int y) override {
//     auto black = Color565(0, 0, 0);
//     matrix->fillRect(x, y, 32, 8, black);
//     matrix->setTextColor(animation_color);
//     matrix->setCursor(x - animation_progress + 32, y + 6);
//
//     char buff[64];
//     int16_t x1, y1;
//     uint16_t w, h;
//     snprintf(buff, 64, "DUCKDNS %d CLOUDNS %d     ", DDNS_TASK::duckdns_ok,
//              DDNS_TASK::cloudns_ok);
//     matrix->getTextBounds(buff, 0, 0, &x1, &y1, &w, &h);
//     matrix->print(buff);
//
//     if (animation_progress > w)
//       animation_progress = 0;
//     matrix->drawLine(x, y + 7, x + 32, y + 7, update_progress_bc);
//     matrix->drawLine(x, y + 7, x + update_progress, y + 7,
//     update_progress_fc);
//   }
// };
//
// static Registry::RegisterFullscreenWidget<DDNS> X;
static Registry::RegisterTask<DDNS_TASK> Y;
