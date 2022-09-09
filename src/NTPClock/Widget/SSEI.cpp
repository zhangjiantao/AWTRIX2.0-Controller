#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <FastLED_NeoMatrix.h>
#include <WiFiClient.h>

#include <cstdint>
#include <list>
#include <vector>

#include "../NTPClock.h"

class HttpUtils {
  HTTPClient httpClient;
  WiFiClient wifiClient;

public:
  String httpRequest(const String &url, int &errCode) {
    String res;

    if (httpClient.begin(wifiClient, url)) { // HTTP
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

template <uint8_t h, uint8_t w, bool fullscreen = false>
class SSEI : public Widget {
  HttpUtils http;
  const String api = "http://img2.money.126.net/data/hs/time/today/";

  struct Index {
    const String code;
    unsigned min;
    unsigned max;
    unsigned yestclose;
    std::vector<unsigned> chart;
  };

  std::vector<Index> targets{
      {"0000001.json", 0, 0, 0, {}},
      {"1399001.json", 0, 0, 0, {}},
      {"1399006.json", 0, 0, 0, {}},
  };

  unsigned long last_update_time = 0;

  unsigned current = 0;

  bool update() {
    Index &target = targets[current];
    int errCode = 0;
    auto response = http.httpRequest(api + target.code, errCode);
    if (errCode)
      return false;

    auto get_float_value = [&](const char *prefix, unsigned offset,
                               float &result) {
      auto p = response.indexOf(prefix);
      if (p == -1)
        return false;
      p += offset;
      auto pstr = response.c_str() + p;
      result = strtof(pstr, nullptr);
      return true;
    };

    // parse yestclose
    float value;
    get_float_value("\"yestclose\":", 12, value);
    target.yestclose = std::round(value * 100);

    static unsigned data[243];
    unsigned data_size = 0;

    static char buff[16];
    for (unsigned i = 930; i < 1531; i++) {
      if (i % 100 > 59 || i / 100 == 12)
        continue;
      sprintf(buff, "[\"%04d\",", i);
      if (get_float_value(buff, 8, value))
        data[data_size++] = (unsigned)std::round(value * 100);
    }

    Serial.printf("date size = %d\n", data_size);
    if (data_size == 0)
      return false;

    target.chart.clear();
    target.chart.push_back(data[0]);
    auto step = (242.0 / (w - 1));
    Serial.printf("step size = %f\n", step);
    unsigned pos = 0;
    for (auto i = step; i < data_size; i += step) {
      pos = (unsigned)std::round(i);
      target.chart.push_back(data[pos - 1]);
    }

    if (pos < data_size)
      target.chart.push_back(data[data_size - 1]);

    Serial.printf("chart size = %d\n", target.chart.size());

    target.max = *std::max_element(target.chart.begin(), target.chart.end());
    target.min = *std::min_element(target.chart.begin(), target.chart.end());
    return true;
  }

public:
  SSEI() {
    for (auto &i : targets)
      i.chart.reserve(w);
  }

  void loop(FastLED_NeoMatrix *matrix) override {
    auto ts = ntp.getEpochTime();
    if (ts - last_update_time < 60)
      return;
    last_update_time = ts;
    auto hasdata = !targets[current].chart.empty();

    if (hasdata) {
      if (ntp.getDay() == 0 || ntp.getDay() == 6)
        return;
      if (ntp.getHours() < 9 || ntp.getHours() == 12 || ntp.getHours() > 14)
        return;
      if (ntp.getHours() == 9 && ntp.getMinutes() < 30)
        return;
      if (ntp.getHours() == 11 && ntp.getMinutes() > 30)
        return;
    }

    update();

    Serial.print(ntp.getFormattedTime());
    Serial.println(" update succe");
  }

  void render(FastLED_NeoMatrix *matrix, int x, int y) override {
    auto c = Color565(130, 130, 130);
    matrix->drawRect(x + 1 - fullscreen, y + 1 - fullscreen, w + 2, h + 2, c);

    auto r = Color565(255, 0, 0);
    auto g = Color565(0, 255, 0);
    auto &target = targets[current];
    for (int i = 0; i < target.chart.size(); i++) {
      c = target.chart[i] >= target.yestclose ? r : g;
      int v;
      if (target.min == target.max)
        v = 3;
      else
        v = map(target.chart[i], target.min, target.max, 0, h - 1);
      matrix->drawPixel(x + 2 + i - fullscreen, y + 6 - v, c);
    }
  }

  bool event1(FastLED_NeoMatrix *matrix) override {
    current = ++current % targets.size();
    if (targets[current].chart.empty())
      update();
    return false;
  }
};

static Registry::RegisterWidget<SSEI<5, 10>> X;
static Registry::RegisterFullscreenWidget<SSEI<6, 30, true>> Y;