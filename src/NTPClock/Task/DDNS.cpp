#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <FastLED_NeoMatrix.h>
#include <WiFiClient.h>

#include <cstdint>
#include <list>
#include <vector>

#include "../NTPClock.h"

#define HOSTNAME "llvm.f3322.net"
#define AUTH_NAME "Authorization"
#define AUTH_VALUE "Basic cm9vdDp6aGFuZ2ppYW50YW8="
#define CURRENT_API "http://ip.3322.net/"
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

class DDNS : public Task {
  HttpUtils http;

  unsigned long last_update_time = 0;
  String last{"unknow"};

  bool update() {
    int errCode = 0;
    auto curr = http.httpRequest(CURRENT_API, errCode);
    curr.trim();
    if (errCode) {
      LOG(Serial.println("can not get current ip"));
      return false;
    }

    if (curr != last) {
      LOG(Serial.printf("update from %s to %s\n", last.c_str(), curr.c_str()));
      auto api = String(UPDATE_API) + curr;
      auto res = http.httpRequest(api, errCode, true);
      res.trim();
      if (errCode != 0 ||
          (!res.startsWith("good") && !res.startsWith("nochg"))) {
        LOG(Serial.printf("failed, code %d, res %s\n", errCode, res.c_str()));
        return false;
      }
      LOG(Serial.println("done"));
      last = curr;
      return true;
    }
    return true;
  }

public:
  bool run() override {
    auto ts = ntp.getEpochTime();
    if (ts - last_update_time < 5*60)
      return true;
    last_update_time = ts;
    return update();
  }
};

static Registry::RegisterTask<DDNS> X;