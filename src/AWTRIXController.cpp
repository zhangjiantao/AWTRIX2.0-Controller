// AWTRIX Controller
// Copyright (C) 2020
// by Blueforcer & Mazze2000

// clang-format off
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <Fonts/TomThumb.h>
#include <Wire.h>
#include "SoftwareSerial.h"

#include <WiFiManager.h>
#include <DoubleResetDetect.h>
#include <Wire.h>
#include <BME280_t.h>
#include "Adafruit_HTU21DF.h"
#include <Adafruit_BMP280.h>

#include <DFMiniMp3.h>
// clang-format on

#include "NTPClock/NTPClock.h"
NTPClock ntpclock;

// instantiate temp sensor
BME280<> BMESensor;
Adafruit_BMP280 BMPSensor; // use I2C interface
Adafruit_HTU21DF htu = Adafruit_HTU21DF();

enum MsgType {
  MsgType_Wifi,
  MsgType_Host,
  MsgType_Temp,
  MsgType_Audio,
  MsgType_Gest,
  MsgType_LDR,
  MsgType_Other
};
enum TempSensor {
  TempSensor_None,
  TempSensor_BME280,
  TempSensor_HTU21D,
  TempSensor_BMP280
}; // None = 0

TempSensor tempState = TempSensor_None;

int ldrState = 0;           // 0 = None
bool USBConnection = false; // true = usb...
bool WIFIConnection = false;
bool notify = false;
int connectionTimout;
int matrixTempCorrection = 0;

const int matrixType = 0;

WiFiManager matrix_wifi_manager;

// update
ESP8266WebServer matrix_server(80);

// resetdetector
#define DRD_TIMEOUT 5.0
#define DRD_ADDRESS 0x00
DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

bool firstStart = true;
int myTime;  // need for loop
int myTime2; // need for loop
int myTime3; // need for loop3
int myCounter;
int myCounter2;
// boolean getLength = true;
// int prefix = -5;

bool ignoreServer = false;
int menuePointer;

// Taster_mid
int tasterPin[] = {D0, D4, D8};
int tasterCount = 3;
int timeoutTaster[] = {0, 0, 0, 0};
bool pushed[] = {false, false, false, false};
int blockTimeTaster[] = {0, 0, 0, 0};
bool blockTaster[] = {false, false, false, false};
bool blockTaster2[] = {false, false, false, false};
bool tasterState[3];
bool allowTasterSendToServer = true;
int pressedTaster = 0;

// Reset time (Touch Taster)
int resetTime = 6000; // in milliseconds

boolean awtrixFound = false;
int myPointer[14];
uint32_t messageLength = 0;
uint32_t SavemMessageLength = 0;

// USB Connection:
byte myBytes[1000];
int bufferpointer;

// Zum speichern...
int cfgStart = 0;

// flag for saving data
bool shouldSaveConfig = false;

/// LDR Config
#define LDR_PIN A0
int LDRvalue = 0;
int minBrightness = 5;
int maxBrightness = 100;
int newBri;
bool autoBrightness;

#define I2C_SDA D3
#define I2C_SCL D1

#ifndef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

bool updating = false;

// Audio

class Mp3Notify {};
SoftwareSerial mySoftwareSerial(D7, D5); // RX, TX
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
DfMp3 dfmp3(mySoftwareSerial);

// Matrix Settings
CRGB matrix_leds[256];
FastLED_NeoMatrix *matrix;

bool saveConfig() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();

  File configFile = LittleFS.open("/awtrix.json", "w");

  if (!configFile) {
    if (!USBConnection) {
      Serial.println("failed to open config file for writing");
    }

    return false;
  }

  json.printTo(configFile);
  configFile.close();
  return true;
}

int checkTaster(int nr) {
  tasterState[0] = !digitalRead(tasterPin[0]);
  tasterState[1] = digitalRead(tasterPin[1]);
  tasterState[2] = !digitalRead(tasterPin[2]);

  switch (nr) {
  case 0:
    if (tasterState[0] == LOW && !pushed[nr] && !blockTaster2[nr] &&
        tasterState[1] && tasterState[2]) {
      pushed[nr] = true;
      timeoutTaster[nr] = millis();
    }
    break;
  case 1:
    if (tasterState[1] == LOW && !pushed[nr] && !blockTaster2[nr] &&
        tasterState[0] && tasterState[2]) {
      pushed[nr] = true;
      timeoutTaster[nr] = millis();
    }
    break;
  case 2:
    if (tasterState[2] == LOW && !pushed[nr] && !blockTaster2[nr] &&
        tasterState[0] && tasterState[1]) {
      pushed[nr] = true;
      timeoutTaster[nr] = millis();
    }
    break;
  }

  if (pushed[nr] && (millis() - timeoutTaster[nr] < 2000) &&
      tasterState[nr] == HIGH) {
    if (!blockTaster2[nr]) {
      pushed[nr] = false;
      return 1;
    }
  }

  if (pushed[nr] && (millis() - timeoutTaster[nr] > 2000)) {
    if (!blockTaster2[nr]) {
      StaticJsonBuffer<400> jsonBuffer;
      JsonObject &root = jsonBuffer.createObject();
      root["type"] = "button";
      switch (nr) {
      case 0:
        root["left"] = "long";
        // Serial.println("LEFT: langer Tastendruck");
        break;
      case 1:
        root["middle"] = "long";
        // Serial.println("MID: langer Tastendruck");
        break;
      case 2:
        root["right"] = "long";
        // Serial.println("RIGHT: langer Tastendruck");
        break;
      case 3:
        if (allowTasterSendToServer) {
          allowTasterSendToServer = false;
          ignoreServer = true;
        } else {
          allowTasterSendToServer = true;
          ignoreServer = false;
          menuePointer = 0;
        }
        break;
      }

      blockTaster[nr] = true;
      blockTaster2[nr] = true;
      pushed[nr] = false;
      return 2;
    }
  }
  if (nr == 3) {
    if (blockTaster[nr] && tasterState[0] == HIGH && tasterState[2] == HIGH) {
      blockTaster[nr] = false;
      blockTimeTaster[nr] = millis();
    }
  } else {
    if (blockTaster[nr] && tasterState[nr] == HIGH) {
      blockTaster[nr] = false;
      blockTimeTaster[nr] = millis();
    }
  }

  if (!blockTaster[nr] && (millis() - blockTimeTaster[nr] > 500)) {
    blockTaster2[nr] = false;
  }
  return 0;
}

void wifiUncheck(int typ, int x, int y) {
  int wifiCheckTime = millis();
  int wifiCheckPoints = 0;
  while (millis() - wifiCheckTime < 2000) {
    while (wifiCheckPoints < 10) {
      matrix->clear();
      matrix->setCursor(7, 6);
      matrix->print("WiFi");

      switch (wifiCheckPoints) {
      case 9:
        matrix->drawPixel(x, y + 4, 0xF800);
      case 8:
        matrix->drawPixel(x - 1, y + 3, 0xF800);
      case 7:
        matrix->drawPixel(x - 2, y + 2, 0xF800);
      case 6:
        matrix->drawPixel(x - 3, y + 1, 0xF800);
      case 5:
        matrix->drawPixel(x - 4, y, 0xF800);
      case 4:
        matrix->drawPixel(x - 4, y + 4, 0xF800);
      case 3:
        matrix->drawPixel(x - 3, y + 3, 0xF800);
      case 2:
        matrix->drawPixel(x - 2, y + 2, 0xF800);
      case 1:
        matrix->drawPixel(x - 1, y + 1, 0xF800);
      case 0:
        matrix->drawPixel(x, y, 0xF800);
        break;
      }
      wifiCheckPoints++;
      matrix->show();
      delay(100);
    }
  }
}

void wifiCheck(int typ, int x, int y) {
  int wifiCheckTime = millis();
  int wifiCheckPoints = 0;
  while (millis() - wifiCheckTime < 2000) {
    while (wifiCheckPoints < 7) {
      matrix->clear();
      matrix->setCursor(7, 6);
      matrix->print("WiFi");

      switch (wifiCheckPoints) {
      case 6:
        matrix->drawPixel(x, y, 0x07E0);
      case 5:
        matrix->drawPixel(x - 1, y + 1, 0x07E0);
      case 4:
        matrix->drawPixel(x - 2, y + 2, 0x07E0);
      case 3:
        matrix->drawPixel(x - 3, y + 3, 0x07E0);
      case 2:
        matrix->drawPixel(x - 4, y + 4, 0x07E0);
      case 1:
        matrix->drawPixel(x - 5, y + 3, 0x07E0);
      case 0:
        matrix->drawPixel(x - 6, y + 2, 0x07E0);
        break;
      }
      wifiCheckPoints++;
      matrix->show();
      delay(100);
    }
  }
}

void wifiSearch(int typ, int x, int y) {
  for (int i = 0; i < 4; i++) {
    matrix->clear();
    matrix->setTextColor(0xFFFF);
    matrix->setCursor(7, 6);
    matrix->print("WiFi");
    switch (i) {
    case 3:
      matrix->drawPixel(x, y, 0x22ff);
      matrix->drawPixel(x + 1, y + 1, 0x22ff);
      matrix->drawPixel(x + 2, y + 2, 0x22ff);
      matrix->drawPixel(x + 3, y + 3, 0x22ff);
      matrix->drawPixel(x + 2, y + 4, 0x22ff);
      matrix->drawPixel(x + 1, y + 5, 0x22ff);
      matrix->drawPixel(x, y + 6, 0x22ff);
    case 2:
      matrix->drawPixel(x - 1, y + 2, 0x22ff);
      matrix->drawPixel(x, y + 3, 0x22ff);
      matrix->drawPixel(x - 1, y + 4, 0x22ff);
    case 1:
      matrix->drawPixel(x - 3, y + 3, 0x22ff);
    case 0:
      break;
    }
    matrix->show();
    delay(100);
  }
}

String GetChipID() { return String(ESP.getChipId()); }

uint32_t Wheel(byte WheelPos, int pos) {
  if (WheelPos < 85) {
    return matrix->Color((WheelPos * 3) - pos, (255 - WheelPos * 3) - pos, 0);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return matrix->Color((255 - WheelPos * 3) - pos, 0, (WheelPos * 3) - pos);
  } else {
    WheelPos -= 170;
    return matrix->Color(0, (WheelPos * 3) - pos, (255 - WheelPos * 3) - pos);
  }
}

void flashProgress(unsigned int progress, unsigned int total) {
  matrix->setBrightness(80);
  long num = 32 * 8 * progress / total;
  for (unsigned char y = 0; y < 8; y++) {
    for (unsigned char x = 0; x < 32; x++) {
      if (num-- > 0)
        matrix->drawPixel(x, 8 - y - 1, Wheel((num * 16) & 255, 0));
    }
  }
  matrix->setCursor(1, 6);
  matrix->setTextColor(matrix->Color(200, 200, 200));
  matrix->print("FLASHING");
  //
  // // // zjt
  // int16_t len = map(progress, 0, total, 0, 31);
  // matrix->drawLine(0, 7, 32, 7, Color565(200, 200, 200));
  // matrix->drawLine(0, 7, len, 7, Color565(0, 255, 64));
  // // //
  matrix->show();
}

void saveConfigCallback() {
  if (!USBConnection) {
    Serial.println("Should save config");
  }
  shouldSaveConfig = true;
}

void configModeCallback(WiFiManager *myWiFiManager) {
  if (!USBConnection) {
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    Serial.println(myWiFiManager->getConfigPortalSSID());
  }
  matrix->clear();
  matrix->setCursor(3, 6);
  matrix->setTextColor(matrix->Color(0, 255, 50));
  matrix->print("Hotspot");
  matrix->show();
}

void setup() {
  delay(2000);

  // https://bbs.hassbian.com/thread-7507-1-1.html
  WiFi.setPhyMode(WIFI_PHY_MODE_11B);

  for (int i = 0; i < tasterCount; i++) {
    pinMode(tasterPin[i], INPUT_PULLUP);
  }

  Serial.setRxBufferSize(1024);
  Serial.begin(115200);
  mySoftwareSerial.begin(9600);

  if (LittleFS.begin()) {
    // if file not exists
    if (!(LittleFS.exists("/awtrix.json"))) {
      LittleFS.open("/awtrix.json", "w+");
    }

    File configFile = LittleFS.open("/awtrix.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject &json = jsonBuffer.parseObject(buf.get());
      if (json.success()) {
        // todo
      }
      configFile.close();
    }
  } else {
    // error
  }

  matrix = new FastLED_NeoMatrix(matrix_leds, 32, 8,
                                 NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
                                     NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
  FastLED.addLeds<NEOPIXEL, D2>(matrix_leds, 256)
      .setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setTextWrap(false);
  matrix->setBrightness(30);
  matrix->setFont(&TomThumb);

  // Reset with Tasters...
  int zeit = millis();
  int zahl = 5;
  int zahlAlt = 6;
  matrix->clear();
  matrix->setTextColor(matrix->Color(255, 0, 255));
  matrix->setCursor(9, 6);
  matrix->print("BOOT");
  matrix->show();
  delay(1000);
  while (!digitalRead(D4)) {
    if (zahl != zahlAlt) {
      matrix->clear();
      matrix->setTextColor(matrix->Color(255, 0, 0));
      matrix->setCursor(6, 6);
      matrix->print("RESET ");
      matrix->print(zahl);
      matrix->show();
      zahlAlt = zahl;
    }
    zahl = 5 - ((millis() - zeit) / 1000);
    if (zahl == 0) {
      matrix->clear();
      matrix->setTextColor(matrix->Color(255, 0, 0));
      matrix->setCursor(6, 6);
      matrix->print("RESET!");
      matrix->show();
      delay(1000);
      if (LittleFS.begin()) {
        delay(1000);
        LittleFS.remove("/awtrix.json");

        LittleFS.end();
        delay(1000);
      }
      matrix_wifi_manager.resetSettings();
      ESP.reset();
    }
  }

  auto ip = IPAddress(172, 217, 28, 1);
  auto gw = IPAddress(172, 217, 28, 1);
  auto sn = IPAddress(255, 255, 255, 0);
  matrix_wifi_manager.setAPStaticIPConfig(ip, gw, sn);
  // WiFiManagerParameter custom_awtrix_server("server", "AWTRIX Host",
  //                                           awtrix_server, 16);
  // WiFiManagerParameter custom_port("Port", "Matrix Port", Port, 6);
  // WiFiManagerParameter custom_matrix_type("matrixType", "MatrixType", "0",
  // 1);
  // // Just a quick hint
  // WiFiManagerParameter host_hint(
  //     "<small>AWTRIX Host IP (without Port)<br></small><br><br>");
  // WiFiManagerParameter port_hint(
  //     "<small>Communication Port (default: 7001)<br></small><br><br>");
  // WiFiManagerParameter matrix_hint(
  //     "<small>0: Columns; 1: Tiles; 2: Rows <br></small><br><br>");
  // WiFiManagerParameter p_lineBreak_notext("<p></p>");
  //
  //
  // matrix_wifi_manager.addParameter(&p_lineBreak_notext);
  // matrix_wifi_manager.addParameter(&host_hint);
  // matrix_wifi_manager.addParameter(&custom_awtrix_server);
  // matrix_wifi_manager.addParameter(&port_hint);
  // matrix_wifi_manager.addParameter(&custom_port);
  // matrix_wifi_manager.addParameter(&matrix_hint);
  // matrix_wifi_manager.addParameter(&custom_matrix_type);
  // matrix_wifi_manager.addParameter(&p_lineBreak_notext);

  matrix_wifi_manager.setSaveConfigCallback(saveConfigCallback);
  matrix_wifi_manager.setAPCallback(configModeCallback);

  matrix_wifi_manager.setCustomHeadElement(
      "<style>html{ background-color:#607D8B;}</style>");

  wifiSearch(0, 24, 0);

  // skip hotspot mode
  if (matrix_wifi_manager.getWiFiIsSaved() &&
      matrix_wifi_manager.getLastConxResult() == WL_CONNECTED) {
    Serial.println("has saved ssid, skip hotspot mode");
    matrix_wifi_manager.setEnableConfigPortal(false);
  }

  if (!matrix_wifi_manager.autoConnect("AWTRIX Controller", "awtrixxx")) {
    // reset and try again, or maybe put it to deep sleep
    Serial.println("failed connect wifi, reset");
    wifiUncheck(0, 27, 1);
    // if (matrix_wifi_manager.getLastConxResult() ==
    // WiFiManager::WL_STATION_WRONG_PASSWORD)
    // matrix_wifi_manager.resetSettings();
    ESP.reset();
    delay(5000);
  }

  // is needed for only one hotpsot!
  WiFi.mode(WIFI_STA);

  ntpclock.setup();

  matrix_server.begin();

  if (shouldSaveConfig) {
    saveConfig();
    ESP.reset();
  }

  wifiCheck(0, 27, 2);

  // delay(1000); // is needed for the dfplayer to startup

  // Checking periphery
  Wire.begin(I2C_SDA, I2C_SCL);

  dfmp3.begin();
  dfmp3.playAdvertisement(0);
  dfmp3.setVolume(2);
  // Serial.println(dfmp3.getVolume());

  ArduinoOTA.onStart([&]() {
    updating = true;
    matrix->clear();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    flashProgress(progress, total);
  });

  ArduinoOTA.begin();

  system_update_cpu_freq(160);
}

void loop() {
  matrix_server.handleClient();
  ArduinoOTA.handle();

  ntpclock.loop(pushed, timeoutTaster);

  // not during the falsh process
  checkTaster(0);
  checkTaster(1);
  checkTaster(2);
  // checkTaster(3);

  // get data and ignore
  if (Serial.available() > 0) {
    Serial.read();
  }
}
