#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Adafruit_DS3502.h>
#include <FastLED.h>

const char* ssid = "Radio Noise AX";
const char* password = "Radio Noise AX";
WebServer server(80);

// ETHERNET PHY DIGITAL SWITCH
// WACCA "0" - PURPLE RED ZIP TIE (Sensor) WHITE (SELECT)
// CHUNI "1" - PURPLE BLUE ZIP TIE (Sensor) WHITE (SELECT)
const int numOfethPorts = 2;
const int ethSensors[2] = { 35, 34 };
const int ethButtons[2] = { 18, 19 };
// CONTROL PORT (KEYCHIP REPLACEMENT)
// PWR_OK / SSD 5V Select
const int powerToggleRelay0 = 17; // WHITE RED
const int gameSelectRelay0 = 5; // WHITE GREEN
// Power Controls
// 0 - PSU and Monitor Enable
// 1 - Nu Power Enable
// 2 - Marquee Enable
// 3 - LED Select
// 4 - NC
const int numberRelays = 5;
const int controlRelays[5] = { 12, 14, 27, 26, 25 };
// Fan Controller
const int fanPWM = 13;
const int fanAddr = 0x01;
// Aux
const int coinBlocker = 16;
// Chunithm LED Driver
#define LED_PIN_1 32  // Define the data pin for the first LED strip
#define LED_PIN_2 33  // Define the data pin for the second LED strip
#define NUM_LEDS_1 53  // Number of LEDs in each strip
#define NUM_LEDS_2 63  // Number of LEDs in each strip
CRGB leds1[NUM_LEDS_1];  // Define the LED array for the first strip
CRGB leds2[NUM_LEDS_2];  // Define the LED array for the second strip
CRGB leds1_source[NUM_LEDS_1];  // Define the LED array for the first strip
CRGB leds2_source[NUM_LEDS_2];  // Define the LED array for the second strip
CRGB leds1_target[NUM_LEDS_1];  // Define the LED array for the first strip
CRGB leds2_target[NUM_LEDS_2];  // Define the LED array for the second strip
uint8_t sourceBrightness = 0;
uint8_t targetBrightness = 0;

unsigned long previousMillis = 0; 
uint8_t numSteps = 33; // Number of steps in the transition
uint8_t currentStep = 0;
bool pending_release_leds = false;
bool transition_leds = false;
float transition_interval = 0;

int currentVolume = 0;
int currentGameSelected0 = 1;
int currentPowerState0 = -1;
int currentLEDState = 0;
int currentMarqueeState = 1;
int currentFanSpeed = 128;
int displayedSec = 0;
int refresh_time = 0;
int displayState = -1;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
Adafruit_DS3502 ds3502 = Adafruit_DS3502();
TaskHandle_t Task1;
TaskHandle_t Task2;

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Attempting to reconnect...");
    WiFi.hostname("CabinetManager");
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (tryCount > 30 && currentPowerState0 != 1) {
        ESP.restart();
      }
      delay(1000);
      Serial.print(".");
      tryCount++;
    }
    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  bootScreen("HappyCAB MCU");
  delay(5000);
  bootScreen("HARDWARE");
  pinMode(powerToggleRelay0, OUTPUT);
  pinMode(fanPWM, OUTPUT);
  pinMode(gameSelectRelay0, OUTPUT);
  pinMode(coinBlocker, INPUT);
  digitalWrite(gameSelectRelay0, HIGH);
  digitalWrite(powerToggleRelay0, HIGH);
  if (!ds3502.begin()) {
    Serial.println("Couldn't find DS3502 chip");
    bootScreen("I2C VOL FAILURE");
    while (1);
  }
  FastLED.addLeds<WS2812B, LED_PIN_1, RGB>(leds1, NUM_LEDS_1).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812B, LED_PIN_2, RGB>(leds2, NUM_LEDS_2).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);
  //EEPROM.begin((16 * numOfRelays));
  for (int i=0; i < numberRelays; i++) {
    pinMode(controlRelays[i], OUTPUT);
    digitalWrite(controlRelays[i], LOW);
  }
  for (int i=0; i < numOfethPorts; i++) {
    pinMode(ethButtons[i], OUTPUT);
    digitalWrite(ethButtons[i], HIGH);
    pinMode(ethSensors[i], INPUT);

    if (digitalRead(ethSensors[i]) == HIGH) {
      digitalWrite(ethButtons[i], LOW);
      delay(150);
      digitalWrite(ethButtons[i], HIGH);
    }
  }

  bootScreen("NETWORK");
  checkWiFiConnection();

  bootScreen("DEFAULTS");
  currentVolume = 30;
  ds3502.setWiper(30);

  bootScreen("REMOTE_ACCESS");
  server.on("/setVolume", [=]() {
    String response = "UNCHANGED";
    if (server.hasArg("wiper")) {
      int _volVal = server.arg("wiper").toInt();
      if (_volVal > 0 && _volVal <= 127) {
        Serial.println("Set Volume to: " + String(_volVal));
        currentVolume = _volVal;
        response = "SET TO VALUE ";
        response += currentVolume;
      }
    } else if (server.hasArg("percent")) {
      int _volVal = server.arg("percent").toInt();
      if (_volVal > 0 && _volVal <= 100) {
        Serial.println("Set Volume to: " + String(_volVal) + "%");
        currentVolume = map(_volVal, 0, 100, 0, 127);
        response = "SET TO ";
        response += _volVal;
        response += "%";
      }
    }
    ds3502.setWiper(currentVolume);
    server.send(200, "text/plain", response);
  });
  server.on("/getVolume", [=]() {
    String response = "";
    response += currentVolume;
    server.send(200, "text/plain", response);
  });
  server.on("/getVolumePercent", [=]() {
    String response = "";
    response += map(currentVolume, 0, 127, 0, 100);
    server.send(200, "text/plain", response);
  });

  server.on("/setFan", [=]() {
    String response = "UNCHANGED";
    int fanSpeed = -1;
    if (server.hasArg("percent")) {
      int _fanVal = server.arg("percent").toInt();
      if (_fanVal > 0 && _fanVal <= 100) {
        Serial.println("Set Fan to: " + String(_fanVal) + "%");
        fanSpeed = _fanVal;
        response = "SET TO ";
        response += _fanVal;
        response += "%";
      }
    }
    if (fanSpeed > -1) {
      setChassisFanSpeed(fanSpeed);
    }
    server.send(200, "text/plain", response);
  });
  server.on("/getFan", [=]() {
    String response = "";
    response += map(currentFanSpeed, 0, 255, 0, 100);
    server.send(200, "text/plain", response);
  });
  
  server.on("/setEthSelects", [=]() {
    for (int i=0; i < server.args(); i++) {
      if (server.argName(i) == "ethNum") {
        const String ethVal = server.arg(i);
        for (int o=0; o < numOfethPorts; o++) {
          if (digitalRead(ethSensors[o]) == (ethVal[o] == '1' ? LOW : HIGH)) {
            if (o == 1 && currentPowerState0 == 1) {
              setSysBoardPower(false);
              delay(800);
            }
            pushEthSwitch(o);
            if (o == 1 && currentPowerState0 == 1) {
              setSysBoardPower(true);
            }
          }
        }
      }
    }
    server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
  });
  server.on("/setEthSelect", [=]() {
    String assembledOutput = "";
    int ethNum = 0;
    String ethVal = "";
    for (int i=0; i < server.args(); i++) {
      if (server.argName(i) == "cabNum")
        ethNum = server.arg(i).toInt();
      else if (server.argName(i) == "ethNum")
        ethVal = server.arg(i).toInt();
    }
    if (digitalRead(ethSensors[ethNum]) == ((ethVal == "1") ? LOW : HIGH)) {
      if (ethNum == 1 && currentPowerState0 == 1) {
        setSysBoardPower(false);
        delay(800);
      }
      pushEthSwitch(ethNum);
      if (ethNum == 1 && currentPowerState0 == 1) {
        setSysBoardPower(true);
      }
      server.send(200, "text/plain", (ethVal == "1" && currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/getEthSelect", [=]() {
    String assembledOutput = "";
    int ethNum = 0;
    for (int i=0; i < server.args(); i++) {
      if (server.argName(i) == "cabNum") {
        ethNum = server.arg(i).toInt();
        String const val = getEthSwitchVal(ethNum);
        assembledOutput += val;
      }
    }
    server.send(200, "text/plain", assembledOutput);
  });

  server.on("/enableOmnimix", [=]() {
    if (currentGameSelected0 == 0) {
      if (currentPowerState0 == 1) {
        setSysBoardPower(false);
        delay(800);
      }
      setOmnimixState(true, true);
      if (currentPowerState0 == 1) {
        setSysBoardPower(true);
      }
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/disableOmnimix", [=]() {
    if (currentGameSelected0 == 1) {
      if (currentPowerState0 == 1) {
        setSysBoardPower(false);
        delay(800);
      }
      setOmnimixState(false, true);
      if (currentPowerState0 == 1) {
        setSysBoardPower(true);
      }
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/getOmnimix", [=]() {
    String const val = getGameSelect();
    server.send(200, "text/plain", val);
  });
  
  server.on("/setMasterOff", [=]() {
    server.send(200, "text/plain", (currentPowerState0 == -1) ? "UNCHNAGED" : "OK");
    setMasterPowerOff();
    currentPowerState0 = -1;
  });
  server.on("/setMasterOn", [=]() {
    if (currentPowerState0 == -1) {
      setMasterPowerOn();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/setGameOff", [=]() {
    server.send(200, "text/plain", (currentPowerState0 != 1) ? "UNCHNAGED" : "OK");
    setGameOff();
    server.send(200, "text/plain", "OK");
  });
  server.on("/setGameWarning", [=]() {
    if (currentPowerState0 == 1) {
      setLEDControl(true);
      delay(100);
    }
    shuttingDownLEDState();
    server.send(200, "text/plain", "OK");
  });
  server.on("/setGameOn", [=]() {
    server.send(200, "text/plain", (currentPowerState0 == 1) ? "UNCHNAGED" : "OK");
    setGameOn();
  });
  server.on("/getPowerState", [=]() {
    String const val = getPowerAuth();
    server.send(200, "text/plain", val);
  });
  server.on("/getSystemState", [=]() {
    String const val = getPowerAuth();
    server.send(200, "text/plain", (currentPowerState0 != -1) ? "Enabled" : "Disabled");
  });
  server.on("/getPowerAuth", [=]() {
    String const val = (currentPowerState0 == 1) ? "Enabled" : "Disabled";
    server.send(200, "text/plain", val);
  });
  
  server.on("/setMarqueeOn", [=]() {
    if (currentMarqueeState == 0) {
      setMarqueeState(true, true);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/setMarqueeOff", [=]() {
    if (currentMarqueeState == 1) {
      setMarqueeState(false, true);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/getMarquee", [=]() {
    server.send(200, "text/plain", ((currentMarqueeState == 0) ? (currentPowerState0 == 1) ? "Enabled" : "Disabled" : "Enabled"));
  });

  server.on("/setLED", [=]() {
    String ledValues = server.arg("ledValues");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Data Sent");
    int brightness = 64;
    int bankSelect = 0;
    if (server.hasArg("ledBrightness")) {
      brightness = server.arg("ledBrightness").toInt();
      if (brightness > 0 && brightness <= 255) {
        if (server.hasArg("transition_time")) {
          targetBrightness = brightness;
        } else {
          FastLED.setBrightness(brightness);
        }
        Serial.println("Set brightness to: " + String(brightness));
      }
    }
    if (server.hasArg("takeOwnership")) {
      setLEDControl((server.arg("ledValues").c_str() == "true"));
    }
    if (server.hasArg("transition_time")) {
      float _transition_time = server.arg("transition_time").toFloat();
      numSteps = _transition_time * 16.6;
      transition_interval = (unsigned long)(1000.0 * _transition_time / (float)numSteps);
    }
    if (server.hasArg("bankSelect")) {
      bankSelect = server.arg("bankSelect").toInt(); 
    }

    handleSetLeds(ledValues, bankSelect, (server.hasArg("transition_time")));
  });
  server.on("/setLEDColor", [=]() {
    String ledValue = server.arg("ledColor");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Data Sent");
    int brightness = 64;
    int bankSelect = 0;
    brightness = server.arg("ledBrightness").toInt();
    if (server.hasArg("transition_time")) {
      float _transition_time = server.arg("transition_time").toFloat();
      numSteps = _transition_time * 16.6;
      transition_interval = (unsigned long)(1000.0 * _transition_time / (float)numSteps);
    }
    if (server.hasArg("ledBrightness")) {
      if (brightness > 0 && brightness <= 255) {
        if (server.hasArg("transition_time")) {
          targetBrightness = brightness;
        } else {
          FastLED.setBrightness(brightness);
        }
        Serial.println("Set brightness to: " + String(brightness));
      }
    }
    if (server.hasArg("bankSelect")) {
      bankSelect = server.arg("bankSelect").toInt(); 
    }

    handleSetLedColor(ledValue, bankSelect, (server.hasArg("transition_time")));
  });
  server.on("/getLEDState", [=]() {
    server.send(200, "text/plain", (currentLEDState == 0)  ? "MCU" : "GAME");
  });
  server.on("/returnLED", [=]() {
    String ledValues = server.arg("ledValues");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Returned to owner");
    setLEDControl((currentPowerState0 != 1));
    if (currentLEDState == 0) {
      delay(800);
      FastLED.show();
      delay(16.6);
      FastLED.show();
    }
  });
  server.on("/resetLED", [=]() {
    String ledValues = server.arg("ledValues");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Reset to standby");
    standbyLEDState();
  });

  server.begin();

  displayedSec = esp_timer_get_time() / 1000000;
  refresh_time = esp_timer_get_time() / 1000000;

  delay(250);
  bootScreen("SYS_PWR_ON");
  setMasterPowerOn();
  delay(500);
}

void loop() {
  int time_in_sec = esp_timer_get_time() / 1000000;
  unsigned long currentMillis = millis();
  int current_time = (time_in_sec - displayedSec) / 3;

  if (transition_leds == true) {
    if (currentStep == 0) {
      sourceBrightness = FastLED.getBrightness();
      for (int i = 0; i < NUM_LEDS_1; i++) {
        leds1_source[i] = leds1[i];
      }
      for (int i = 0; i < NUM_LEDS_2; i++) {
        leds2_source[i] = leds2[i];
      }
    }
    if (currentMillis - previousMillis >= transition_interval) {
      // Save the last time we updated the LED colors
      previousMillis = currentMillis;
      if (currentStep <= numSteps) {
        FastLED.setBrightness(map(currentStep, 0, numSteps, sourceBrightness, targetBrightness));
        for (int i = 0; i < NUM_LEDS_1; i++) {
          leds1[i].r = map(currentStep, 0, numSteps, leds1_source[i].r, leds1_target[i].r);
          leds1[i].g = map(currentStep, 0, numSteps, leds1_source[i].g, leds1_target[i].g);
          leds1[i].b = map(currentStep, 0, numSteps, leds1_source[i].b, leds1_target[i].b);
        }
        for (int i = 0; i < NUM_LEDS_2; i++) {
          leds2[i].r = map(currentStep, 0, numSteps, leds2_source[i].r, leds2_target[i].r);
          leds2[i].g = map(currentStep, 0, numSteps, leds2_source[i].g, leds2_target[i].g);
          leds2[i].b = map(currentStep, 0, numSteps, leds2_source[i].b, leds2_target[i].b);
        }
        FastLED.show();
        currentStep++;
      } else {
        transition_leds = false;
        currentStep = 0;
        for (int i = 0; i < NUM_LEDS_1; i++) {
          leds1_source[i] = leds1[i];
        }
        for (int i = 0; i < NUM_LEDS_2; i++) {
          leds2_source[i] = leds2[i];
        }
        sourceBrightness = FastLED.getBrightness();
        FastLED.show();
          delay(16.6);
        FastLED.show();
      }
    }
  } else {
    checkWiFiConnection();
    server.handleClient();
    bool coinEnable = (digitalRead(coinBlocker) == LOW);
    if (pending_release_leds == true && coinEnable == true) {
      setLEDControl(false);
      pending_release_leds = false;
    }
    
    if (displayState != 0 && current_time < 1) {
      displayIconMessage(255, true, true, 250, "チュウニズム");
      displayState = 0;
    } else if (displayState != 1 && current_time >= 1 && current_time < 2) {
      const String power = getPowerAuth();
      displayIconDualMessage(1, (power == "Active"), false, (power == "Active") ? 491 : 490, "System Board", power);
      displayState = 1;
    } else if (displayState != 2 && current_time >= 2 && current_time < 3) {
      displayIconDualMessage(1, (coinEnable == true), false, 71, "Card Reader", (coinEnable == true) ? "Enabled" : "Disabled");
      displayState = 2;
    } else  if (displayState != 3 && current_time >= 3 && current_time < 4) {
      String volume = "";
      volume += map(currentVolume, 0, 127, 0, 100);
      volume += "%";
      displayIconDualMessage(1, (currentVolume >= 50), false, 484, "Speakers", volume.c_str());
      displayState = 3;
    } else if (displayState != 4 && current_time >= 4 && current_time < 5) {
      String fan = "";
      fan += map(currentFanSpeed, 0, 255, 0, 100);
      fan += "%";
      displayIconDualMessage(1, (map(currentFanSpeed, 0, 255, 0, 100) >= 75), false, 225, "Fans", fan.c_str());
      displayState = 4;
    } else if (displayState != 5 && current_time >= 5 && current_time < 6) {
      displayIconDualMessage(1, (currentMarqueeState == 1), false, 398, "Marquee", (currentMarqueeState == 1) ? "Enabled" : "Disabled");
      displayState = 5;
    } else if (displayState != 6 && current_time >= 6 && current_time < 7) {
      displayIconDualMessage(1, (currentLEDState == 1), false, 398, "LED Control", (currentLEDState == 0) ? "MCU" : "Game");
      displayState = 6;
    } else if (displayState != 7 && current_time >= 7 && current_time < 8) {
      displayIconDualMessage(1, false, false, 141, "Game Disk", getGameSelect());
      displayState = 7;
    } else if (displayState != 8 && current_time >= 8 && current_time < 9) {
      displayIconDualMessage(1, false, false, 510, "Net: Chunithm", getEthSwitchVal(1));
      displayState = 8;
    } else if (displayState != 9 && current_time >= 9 && current_time < 10) {
      displayIconDualMessage(1, false, false, 510, "Net: WACCA", getEthSwitchVal(0));
      displayState = 9;
    } else if (current_time >= 10) {
      displayedSec = time_in_sec;
    }
  }
}

void bootScreen(String input_message) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_HelvetiPixel_tr);  // Choose your font
  const char* string = input_message.c_str();
  int textWidth = u8g2.getStrWidth(string);
  int centerX = ((u8g2.getWidth() - textWidth) / 2);
  int centerGlX = ((u8g2.getWidth() - textWidth) / 2);
  int centerY = u8g2.getHeight() / 2 + u8g2.getAscent() / 2;
  u8g2.drawStr(centerX, centerY, string);
  u8g2.sendBuffer();
}

void displayIconMessage(int bright, bool invert, bool jpn, int icon, String input_message) {
  u8g2.setPowerSave(0);
  u8g2.setContrast(bright);
  u8g2.clearBuffer();
  int Xpos = 2;
  const char* string = input_message.c_str();
  if (invert == true) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, u8g2.getWidth(), u8g2.getHeight());
    u8g2.sendBuffer();
    u8g2.setDrawColor(0);
    u8g2.setColorIndex(0);
  }
  u8g2.setFont((jpn == true) ? u8g2_font_b12_t_japanese1 : u8g2_font_HelvetiPixel_tr);  // Choose your font
  int textWidth = u8g2.getUTF8Width(string);
  int centerX = Xpos + 28;
  int centerY = (u8g2.getHeight() / 2 + u8g2.getAscent() / 2);
  u8g2.drawUTF8(centerX, centerY, string);
  u8g2.setFont(u8g2_font_streamline_all_t);
  int centerGlX = Xpos;
  int centerGlY = (u8g2.getHeight() / 2 + u8g2.getAscent() / 2);
  u8g2.drawGlyph(centerGlX, centerGlY, icon);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}
void displayIconDualMessage(int bright, bool invert, bool jpn, int icon, String input_message, String input2_message) {
  u8g2.setPowerSave(0);
  u8g2.setContrast(bright);
  u8g2.clearBuffer();
  int Xpos = 2;
  const char* string = input_message.c_str();
  const char* string2 = input2_message.c_str();
  if (invert == true) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, u8g2.getWidth(), u8g2.getHeight());
    u8g2.sendBuffer();
    u8g2.setDrawColor(0);
    u8g2.setColorIndex(0);
  }
  u8g2.setFont((jpn == true) ? u8g2_font_b12_t_japanese1 : u8g2_font_HelvetiPixel_tr);  // Choose your font
  int textWidth = u8g2.getUTF8Width(string);
  int centerX = Xpos + 28;
  int centerY = ((u8g2.getHeight() / 2 + u8g2.getAscent() / 2) / 2) + 2;
  int centerY2 = (centerY * 2) + 3;
  u8g2.drawUTF8(centerX, centerY, string);
  u8g2.drawUTF8(centerX, centerY2, string2);
  u8g2.setFont(u8g2_font_streamline_all_t);
  int centerGlX = Xpos;
  int centerGlY = (u8g2.getHeight() / 2 + u8g2.getAscent() / 2);
  u8g2.drawGlyph(centerGlX, centerGlY, icon);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

String getEthSwitchVal(int cabNum) {
  return ((digitalRead(ethSensors[cabNum])) == LOW) ? "Official" : "Private";
}
String getGameSelect() {
  String assembledOutput = "";
  assembledOutput += (currentGameSelected0 == 1) ? "Omnimix" : "Base";
  return assembledOutput;
}
String getPowerAuth() {
  String assembledOutput = "";
  assembledOutput += ((currentPowerState0 == -1) ? "Power Off" : (currentPowerState0 == 0) ? "Standby" : "Active");
  return assembledOutput;
}

void defaultLEDState() {
  FastLED.setBrightness(0);
  for (int i = 0; i < NUM_LEDS_1; i++) {
    leds1[i] = CRGB::Black;
  }
  for (int i = 0; i < NUM_LEDS_2; i++) {
    leds2[i] = CRGB::Black;
  }
  FastLED.show();
  delay(100);
  FastLED.show();
}
void standbyLEDState() {
  FastLED.setBrightness(32);
  setUpperLEDColor(CRGB::Black, false);
  setSideLEDs(CRGB::Yellow, CRGB::Yellow, false);
  FastLED.show();
  delay(100);
  FastLED.show();
}
void startingLEDState() {
  setLEDControl(true);
  targetBrightness = 128;
  numSteps = 4.0 * 16.6;
  transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  setUpperLEDColor(CRGB::Black, true);
  setSideLEDs(CRGB::Green, CRGB::Green, true);
  pending_release_leds = true;
  transition_leds = true;
}
void shuttingDownLEDState() {
  FastLED.setBrightness(255);
  setUpperLEDColor(CRGB::Black, false);
  setSideLEDs(CRGB::Red, CRGB::Red, false);
  FastLED.show();
  delay(100);
  FastLED.show();
}

void setUpperLED(String ledValues, bool should_transition) {
  Serial.println("Received LED values: " + ledValues);
  int index = 0;
  while (ledValues.length() > 0 && index < (NUM_LEDS_1 + NUM_LEDS_2)) {
    String hexValue = ledValues.substring(0, ledValues.indexOf(' '));
    ledValues = ledValues.substring(ledValues.indexOf(' ') + 1);
    uint32_t color = (uint32_t)strtol(hexValue.c_str(), NULL, 16);
    if (index > (NUM_LEDS_1 - 1)) {
      leds1[index] = color;
    } else {
      leds2[(index - NUM_LEDS_1)] = color;
    }
    index++;
  }
}
void setUpperLEDColor(uint32_t color, bool should_transition) {
  for (int i = 0; i < 50; i++) {
    if (should_transition == true) {
      leds1_target[i] = color;
    } else {
      leds1[i] = color;
    }
  }
  for (int i = 0; i < 60; i++) {
    if (should_transition == true) {
      leds2_target[i] = color;
    } else {
      leds2[i] = color;
    }
  }
}
void setSideLEDs(uint32_t colorL, uint32_t colorR, bool should_transition) {
  for (int i = 0; i < 3; i++) {
    if (should_transition == true) {
      leds1_target[50 + i] = colorL;
    } else {
      leds1[50 + i] = colorL;
    }
  }
  for (int i = 0; i < 3; i++) {
    if (should_transition == true) {
      leds2_target[60 + i] = colorR;
    } else {
      leds2[60 + i] = colorR;
    }
  }
}

void handleSetLeds(String ledValues, int bankSelect, bool should_transition) {
  Serial.println("Received LED values: " + ledValues);
  int index = 0;
  int ledIndex = 0;
  while (ledValues.length() > 0 && index < (NUM_LEDS_1 + NUM_LEDS_2)) {
    String hexValue = ledValues.substring(0, ledValues.indexOf(' '));
    ledValues = ledValues.substring(ledValues.indexOf(' ') + 1);
    uint32_t color = (uint32_t)strtol(hexValue.c_str(), NULL, 16);

    if (index < (NUM_LEDS_1 + NUM_LEDS_2)) {
      if (bankSelect == 0) {
        if (index == ledIndex) {
          if (should_transition == true) {
            leds1_target[ledIndex] = color;
          } else {
            leds1[ledIndex] = color;
          }
          ledIndex++;
          if (ledIndex >= NUM_LEDS_1) {
            ledIndex = 0;
          }
        } else if (should_transition == true) {
          leds2_target[ledIndex] = color;
          ledIndex++;
        } else {
          leds2[ledIndex] = color;
          ledIndex++;
        }
      }
    } else {
      Serial.println("Received LED Value Index OUT OF RANGE OF CONTROLLER: " + index);
    }
    index++;
  }
  if (should_transition == true) {
    bool all_same = true;
    for (int i = 0; i < NUM_LEDS_1; i++) {
      if (leds1_source[i].r != leds1_target[i].r && leds1_source[i].g != leds1_target[i].g && leds1_source[i].b != leds1_target[i].b) {
        all_same = false;
      }
    }
    for (int i = 0; i < NUM_LEDS_2; i++) {
      if (leds2_source[i].r != leds2_target[i].r && leds2_source[i].g != leds2_target[i].g && leds2_source[i].b != leds2_target[i].b) {
        all_same = false;
      }
    }
    if (all_same == false) {
      transition_leds = true;
    } else {
      FastLED.show();
      delay(100);
      FastLED.show();
    }
  } else {
    FastLED.show();
    delay(100);
    FastLED.show();
  }
}
void handleSetLedColor(String ledValue, int bankSelect, bool should_transition) {
  Serial.println("Received LED value: " + ledValue);
  uint32_t color = (uint32_t)strtol(ledValue.c_str(), NULL, 16);
  int index = 0;
  if (bankSelect == 0) {
    if (should_transition == true) {
      for (int i = 0; i < NUM_LEDS_1; i++) {
        leds1_target[i] = color;
      }
      for (int i = 0; i < NUM_LEDS_2; i++) {
        leds2_target[i] = color;
      }
    } else {
      for (int i = 0; i < NUM_LEDS_1; i++) {
        leds1[i] = color;
      }
      for (int i = 0; i < NUM_LEDS_2; i++) {
        leds2[i] = color;
      }
    }
  } else if (bankSelect == 1) {
    setUpperLEDColor(color, should_transition);
  } else if (bankSelect == 2) {
    setSideLEDs(color, color, should_transition);
  }
  if (should_transition == true) {
    bool all_same = true;
    for (int i = 0; i < NUM_LEDS_1; i++) {
      if (leds1_source[i].r != leds1_target[i].r && leds1_source[i].g != leds1_target[i].g && leds1_source[i].b != leds1_target[i].b) {
        all_same = false;
      }
    }
    for (int i = 0; i < NUM_LEDS_2; i++) {
      if (leds2_source[i].r != leds2_target[i].r && leds2_source[i].g != leds2_target[i].g && leds2_source[i].b != leds2_target[i].b) {
        all_same = false;
      }
    }
    if (all_same == false) {
      transition_leds = true;
    } else {
      FastLED.show();
      delay(100);
      FastLED.show();
    }
  } else {
    FastLED.show();
    delay(100);
    FastLED.show();
  }
}

void setMasterPowerOn() {
  if (currentPowerState0 == -1) {
    setIOPower(true);
    delay(250);
    setLEDControl(true);
    resetMarqueeState();
    setChassisFanSpeed(50);
    delay(500);
    defaultLEDState();
    standbyLEDState();
    currentPowerState0 = 0;
  }
}
void setMasterPowerOff() {
  setChassisFanSpeed(0);
  setLEDControl(true);
  setMarqueeState(false, false);
  if (currentPowerState0 == 1) {
    delay(500);
    setSysBoardPower(false);
    delay(200);
  }
  delay(500);
  setIOPower(false);
  if (currentPowerState0 == 1) {
    delay(500);
    setOmnimixState(false, false);
  }
  currentPowerState0 = -1;
}
void setGameOn() {
  if (currentPowerState0 == -1) {
    setMasterPowerOn();
    delay(500);
  }
  if (currentPowerState0 != 1) {
    setChassisFanSpeed(100);
    startingLEDState();
    setMarqueeState(true, false);
    resetOmnimixState();
    delay(1000);
    setSysBoardPower(true);
  }
  currentPowerState0 = 1;
}
void setGameOff() {
  if (currentPowerState0 == 1) {
    setChassisFanSpeed(50);
    setLEDControl(true);
    delay(250);
    standbyLEDState();
    resetMarqueeState();
    setSysBoardPower(false);
    delay(800);
    setOmnimixState(false, false);
  }
  currentPowerState0 = 0;
}

void setSysBoardPower(bool state) {
  digitalWrite(controlRelays[1], (state == true) ? HIGH : LOW);
  delay((state == true) ? 200 : 500);
  digitalWrite(powerToggleRelay0, (state == true) ? LOW : HIGH);
}
void setLEDControl(bool state) {
  digitalWrite(controlRelays[3], (state == true) ? LOW : HIGH);
  currentLEDState = (state == true) ? 0 : 1;
}
void setIOPower(bool state) {
  digitalWrite(controlRelays[0], (state == true) ? HIGH : LOW);
}
void resetMarqueeState() {
  digitalWrite(controlRelays[2], (currentMarqueeState == 1) ? HIGH : LOW);
}
void setMarqueeState(bool state, bool save) {
  if (currentPowerState0 != -1) {
    digitalWrite(controlRelays[2], (state == true) ? HIGH : LOW);
  }
  if (save == true) {
    currentMarqueeState = (state == true) ? 1 : 0;
  }
}
void setChassisFanSpeed(int speed) {
  currentFanSpeed = map(speed, 0, 100, 0, 255);
  analogWrite(fanPWM, currentFanSpeed);
}
void setOmnimixState(bool state, bool save) {
  digitalWrite(gameSelectRelay0, (state == true) ? HIGH : LOW);
  if (save == true) {
    currentGameSelected0 = (state == true) ? 1 : 0;
  }
}
void resetOmnimixState() {
  digitalWrite(gameSelectRelay0, (currentGameSelected0 == 1) ? HIGH : LOW);
}

void pushEthSwitch(int ethNum) {
  digitalWrite(ethButtons[ethNum], HIGH);
  digitalWrite(ethButtons[ethNum], LOW);
  delay(150);
  digitalWrite(ethButtons[ethNum], HIGH);
}