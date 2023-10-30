#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Adafruit_DS3502.h>
#include <Adafruit_NeoPixel.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include "melody.h"

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
// Aux
const int coinBlocker = 16;
bool coinEnable = false;
// Display Switch
// 23 - Select
// 15 - State
const int displayMainSelect = 23;
const int displayMainLDR = 15;
bool displayMainState = false;
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
CRGB leds1_backup[NUM_LEDS_1];  // Define the LED array for the first strip
CRGB leds2_backup[NUM_LEDS_2];  // Define the LED array for the second strip
Adafruit_NeoPixel NeoPixelL(NUM_LEDS_1, LED_PIN_1, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel NeoPixelR(NUM_LEDS_2, LED_PIN_2, NEO_RGB + NEO_KHZ800);
uint8_t sourceBrightness = 0;
uint8_t targetBrightness = 0;
unsigned long previousMillis = 0; 
uint8_t numSteps = 33; // Number of steps in the transition
uint8_t currentStep = 0;
bool pending_release_leds = false;
bool pending_release_display = false;
bool transition_leds = false;
float transition_interval = 0;
int animation_mode = -1;
int animation_state = -1;
// Beeper Tones
int buzzer_pin = 4;
unsigned long previousMelodyMillis = 0;
int currentNote = 0;
int melodyPlay = 2;
bool startMelody = true;
int loopMelody = -1;
int pauseBetweenNotes = 0;
int booting_tone[] = {
  NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C6, NOTE_E6
};
int booting_tone_dur[] = {
  8, 8, 8, 4, 2
};

int shutting_down_tone[] = {
  NOTE_D5, NOTE_A4, NOTE_D5, NOTE_D4
};
int shutting_down_dur[] = {
  8, 8, 4, 2
};
int warning_tone[] = {
  NOTE_A5, NOTE_G5, NOTE_F5
};
int warning_tone_dur[] = {
  8, 8, 8
};
int boot_tone[] = {
  NOTE_F5, NOTE_G5, NOTE_A5, NOTE_B5
};
int boot_tone_dur[] = {
  8, 8, 8, 4
};
// Kiosk API for PC
const char *hostURL = "http://192.168.100.16:6833/action/";
// Occupancy and Timer
const int inactivityMinTimeout = 45;
const int shutdownDelayMinTimeout = 5;
unsigned long previousOccupancyMillis = 0; 

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
    WiFi.hostname("CabinetManager");
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    tone(buzzer_pin, NOTE_CS5, 1000 / 8);
    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (tryCount > 60 && currentPowerState0 != 1) {
        ESP.restart();
      }
      tone(buzzer_pin, (tryCount % 2 == 0) ? NOTE_GS5 : NOTE_CS5, 1000 / 8);
      delay(500);
      tryCount++;
    }
    noTone(buzzer_pin);
  }
}

void setup() {
  pinMode(displayMainSelect, OUTPUT);
  digitalWrite(displayMainSelect, HIGH);
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
  pinMode(displayMainLDR, INPUT);
  digitalWrite(gameSelectRelay0, HIGH);
  digitalWrite(powerToggleRelay0, HIGH);
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
  if (!ds3502.begin()) {
    bootScreen("I2C VOL FAILURE");
    while (1);
  }
  NeoPixelL.begin();
  NeoPixelR.begin();
  NeoPixelL.show();
  NeoPixelR.show();
  NeoPixelL.setBrightness(255);
  NeoPixelR.setBrightness(255);

  bootScreen("NETWORK");
  checkWiFiConnection();

  bootScreen("DEFAULTS");
  currentVolume = 30;
  ds3502.setWiper(30);
  displayMainState = (digitalRead(displayMainLDR) == LOW);
  kioskModeRequest("StopAll");

  bootScreen("REMOTE_ACCESS");
  server.on("/setVolume", [=]() {
    String response = "UNCHANGED";
    if (server.hasArg("wiper")) {
      int _volVal = server.arg("wiper").toInt();
      if (_volVal > 0 && _volVal <= 127) {
        currentVolume = _volVal;
        response = "SET TO VALUE ";
        response += currentVolume;
      }
    } else if (server.hasArg("percent")) {
      int _volVal = server.arg("percent").toInt();
      if (_volVal > 0 && _volVal <= 100) {
        currentVolume = map(_volVal, 10, 100, 0, 127);
        response = "SET TO ";
        response += _volVal;
        response += "%";
      }
    } else if (server.hasArg("mute")) {
      int _muteVal = server.arg("mute").toInt();
      if (server.hasArg("invert")) {
        currentVolume = (_muteVal == 1) ? 10 : 30
      } else {
        currentVolume = (_muteVal == 0) ? 10 : 30
      }
      response = "SET TO ";
      response += _muteVal;
      response += "%";
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
    response += map(currentVolume, 10, 127, 0, 100);
    server.send(200, "text/plain", response);
  });
  server.on("/getVolumeFloat", [=]() {
    String response = "";
    if (server.hasArg("invert")) {
      currentVolume = (_muteVal == 1) ? 10 : 30
    } else {

    }
    
    server.send(200, "text/plain", response);
  });
  server.on("/getVolumeMute", [=]() {
    String response = "";
    response += (map(currentVolume, 0, 127, 0, 1) > 10) ? "1" : "0";
    server.send(200, "text/plain", response);
  });

  server.on("/setFan", [=]() {
    String response = "UNCHANGED";
    int fanSpeed = -1;
    if (server.hasArg("percent")) {
      int _fanVal = server.arg("percent").toInt();
      if (_fanVal > 0 && _fanVal <= 100) {
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

  server.on("/getDisplayState", [=]() {
    String assembledOutput = "";
    assembledOutput += ((displayMainSelect == true) ? "PC" : "GAME");
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/setDisplayPC", [=]() {
    setDisplayState(true);
    server.send(200, "text/plain", (displayMainSelect == true) ? "UNCHANGED" : "OK");
  });
  server.on("/setDisplayGame", [=]() {
    setDisplayState(false);
    server.send(200, "text/plain", (displayMainSelect == false) ? "UNCHANGED" : "OK");
  });
  server.on("/setDisplay", [=]() {
    String assembledOutput = "";
    pushDisplaySwitch();
    assembledOutput += ((displayMainSelect == false) ? "PC" : "AUX");
    server.send(200, "text/plain", assembledOutput);
  });
  
  server.on("/setEthSelects", [=]() {
    for (int i=0; i < server.args(); i++) {
      if (server.argName(i) == "ethNum") {
        const String ethVal = server.arg(i);
        for (int o=0; o < numOfethPorts; o++) {
          if (digitalRead(ethSensors[o]) == (ethVal[o] == '1' ? LOW : HIGH)) {
            if (o == 1 && currentPowerState0 == 1) {
              setSysBoardPower(false);
              if (pending_release_display == false) {
                setDisplayState(true);
                startLoadingScreen();
              }
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
        if (pending_release_display == false) {
          setDisplayState(true);
          startLoadingScreen();
        }
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
        if (pending_release_display == false) {
          setDisplayState(true);
          startLoadingScreen();
        }
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
        if (pending_release_display == false) {
          setDisplayState(true);
          startLoadingScreen();
        }
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
    server.send(200, "text/plain", "OK");
    shuttingDownLEDState();
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
    if (pending_release_leds == false) {
      int brightness = 64;
      int bankSelect = 0;
      if (server.hasArg("ledBrightness")) {
        brightness = server.arg("ledBrightness").toInt();
        if (brightness > 0 && brightness <= 255) {
          if (server.hasArg("transition_time")) {
            targetBrightness = brightness;
          } else {
            NeoPixelL.setBrightness(brightness);
            NeoPixelR.setBrightness(brightness);
          }
        }
      }
      if (server.hasArg("takeOwnership")) {
        setLEDControl((server.arg("ledValues").c_str() == "true"));
      }
      if (server.hasArg("transition_time")) {
        float _transition_time = server.arg("transition_time").toFloat();
        numSteps = _transition_time * 33.2;
        transition_interval = (unsigned long)(1000.0 * _transition_time / (float)numSteps);
      }
      if (server.hasArg("bankSelect")) {
        bankSelect = server.arg("bankSelect").toInt(); 
      }

      handleSetLeds(ledValues, bankSelect, (server.hasArg("transition_time")));
    }
  });
  server.on("/setLEDColor", [=]() {
    String ledValue = server.arg("ledColor");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Data Sent");
    if (pending_release_leds == false) {
      int brightness = 64;
      int bankSelect = 0;
      brightness = server.arg("ledBrightness").toInt();
      if (server.hasArg("transition_time")) {
        float _transition_time = server.arg("transition_time").toFloat();
        numSteps = _transition_time * 33.2;
        transition_interval = (unsigned long)(1000.0 * _transition_time / (float)numSteps);
      }
      if (server.hasArg("ledBrightness")) {
        if (brightness > 0 && brightness <= 255) {
          if (server.hasArg("transition_time")) {
            targetBrightness = brightness;
          } else {
            NeoPixelL.setBrightness(brightness);
            NeoPixelR.setBrightness(brightness);
          }
        }
      }
      if (server.hasArg("bankSelect")) {
        bankSelect = server.arg("bankSelect").toInt(); 
      }

      handleSetLedColor(ledValue, bankSelect, (server.hasArg("transition_time")));
    }
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
      copyLEDBuffer();
    }
    setDisplayState(false);
    melodyPlay = -1;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = false;
    pending_release_leds = false;
    pending_release_display = false;
    animation_state = -1;
    animation_mode = -1;
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
  bootScreen("BOOT_CPU2");
  xTaskCreatePinnedToCore(
                    cpu0Loop,   /* Task function. */
                    "driverTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  delay(500);
}

void loop() {
  checkWiFiConnection();
  server.handleClient();
  coinEnable = (digitalRead(coinBlocker) == LOW);
  displayMainState = (digitalRead(displayMainLDR) == LOW);
  if (pending_release_leds == true && coinEnable == true) {
    setLEDControl(false);
    pending_release_leds = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
  } else if (pending_release_leds == false && coinEnable == false && currentPowerState0 == 1) {
    startingLEDState();
  }
  if (pending_release_display == true && coinEnable == true) {
    pending_release_display = false;
    setDisplayState(false);
    kioskModeRequest("StopAll");
  }
  unsigned long currentMillis = millis();
  if (transition_leds == true) {
    if (currentStep == 0) {
      sourceBrightness = NeoPixelL.getBrightness();
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
        NeoPixelL.setBrightness(map(currentStep, 0, numSteps, sourceBrightness, targetBrightness));
        NeoPixelR.setBrightness(map(currentStep, 0, numSteps, sourceBrightness, targetBrightness));
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
        copyLEDBuffer();
        currentStep++;
      } else if (animation_mode == 1) {
        currentStep = 0;
        if (animation_state == -1) {
          for (int i = 0; i < NUM_LEDS_1; i++) {
            leds1_target[i] = CRGB::Black;
          }
          for (int i = 0; i < NUM_LEDS_2; i++) {
            leds2_target[i] = CRGB::Black;
          }
          numSteps = 3 * 33.2;
          transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
          animation_state = 1;
        } else {
          for (int i = 0; i < NUM_LEDS_1; i++) {
            leds1_target[i] = leds1_source[i];
          }
          for (int i = 0; i < NUM_LEDS_2; i++) {
            leds2_target[i] = leds2_source[i];
          }
          numSteps = 3 * 33.2;
          transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
          animation_state = -1;
        }
      } else {
        transition_leds = false;
        animation_state = -1;
        animation_mode = -1;
        currentStep = 0;
        for (int i = 0; i < NUM_LEDS_1; i++) {
          leds1_source[i] = leds1[i];
        }
        for (int i = 0; i < NUM_LEDS_2; i++) {
          leds2_source[i] = leds2[i];
        }
        sourceBrightness = NeoPixelL.getBrightness();
        copyLEDBuffer();
        //if (pending_release_leds == true) {
        //  setLEDControl(false);
        //}
        //pending_release_leds = false;
      }
    }
  }
  if (startMelody == true) {
    if (currentMillis - previousMelodyMillis >= pauseBetweenNotes) {
      previousMelodyMillis = currentMillis;
      if (melodyPlay == 0) {
        playMelody(booting_tone, booting_tone_dur, sizeof(booting_tone_dur) / sizeof(int));
      } else if (melodyPlay == 1) {
        playMelody(shutting_down_tone, shutting_down_dur, sizeof(shutting_down_dur) / sizeof(int));
      } else if (melodyPlay == 2) {
        playMelody(boot_tone, boot_tone_dur, sizeof(boot_tone_dur) / sizeof(int));
      } else if (melodyPlay == 3) {
        playMelody(warning_tone, warning_tone_dur, sizeof(warning_tone_dur) / sizeof(int));
      }
    }
  }
}

void cpu0Loop( void * pvParameters ) {
  Serial.print("Primary Task running on core ");
  Serial.println(xPortGetCoreID());

  for(;;) {
    runtime();
  }
}

void runtime() {
  int time_in_sec = esp_timer_get_time() / 1000000;
  unsigned long currentMillis = millis();
  int current_time = (time_in_sec - displayedSec) / 2;

  if (displayState != 1 && currentPowerState0 == -1) {
      const String power = getPowerAuth();
      displayIconDualMessage(1, false, false, (power == "Active") ? 491 : 490, "System Power", power);
      displayState = 1;
  } else if (currentPowerState0 != -1) {
    if (displayState != 0 && current_time < 1) {
      displayIconMessage(1, true, true, 250, "チュウニズム");
      displayState = 0;
    } else if (displayState != 1 && current_time >= 1 && current_time < 2) {
      const String power = getPowerAuth();
      displayIconDualMessage(1, (power == "Active"), false, (power == "Active") ? 491 : 490, "System Power", power);
      displayState = 1;
    } else if (displayState != 2 && current_time >= 2 && current_time < 3) {
      String volume = "";
      volume += map(currentVolume, 0, 127, 0, 100);
      volume += "%";
      displayIconDualMessage(1, (currentVolume >= 50), false, 484, "Speakers", volume.c_str());
      displayState = 2;
    } else if (displayState != 3 && current_time >= 3 && current_time < 4) {
      String fan = "";
      fan += map(currentFanSpeed, 0, 255, 0, 100);
      fan += "%";
      displayIconDualMessage(1, (map(currentFanSpeed, 0, 255, 0, 100) >= 75), false, 225, "Fans", fan.c_str());
      displayState = 3;
    } else if (displayState != 4 && current_time >= 4 && current_time < 5) {
      displayIconDualMessage(1, (currentMarqueeState == 1), false, 398, "Marquee", (currentMarqueeState == 1) ? "Enabled" : "Disabled");
      displayState = 4;
    } else if (displayState != 5 && current_time >= 5 && current_time < 6) {
      displayIconDualMessage(1, (displayMainState == false), false, (displayMainState == false) ? 250 : 129, "Display Source", (displayMainState == true) ? "PC" : "Game");
      displayState = 5;
    } else if (displayState != 6 && current_time >= 6 && current_time < 7) {
      displayIconDualMessage(1, (currentLEDState == 1), false, 398, "LED Control", (currentLEDState == 0) ? "MCU" : "Game");
      displayState = 6;
    } else if (displayState != 7 && current_time >= 7 && current_time < 8) {
      displayIconDualMessage(1, false, false, 141, "Game Disk", getGameSelect());
      displayState = 7;
    } else if (displayState != 8 && current_time >= 8 && current_time < 9) {
      displayIconDualMessage(1, (coinEnable == true), false, 71, "Card Reader", (coinEnable == true) ? "Enabled" : "Disabled");
      displayState = 8;
    } else if (displayState != 9 && current_time >= 9 && current_time < 10) {
      displayIconDualMessage(1, false, false, 510, "Net: Chunithm", getEthSwitchVal(1));
      displayState = 9;
    } else if (displayState != 10 && current_time >= 10 && current_time < 11) {
      displayIconDualMessage(1, false, false, 510, "Net: WACCA", getEthSwitchVal(0));
      displayState = 10;
    } else if (current_time >= 11) {
      displayedSec = time_in_sec;
    }
  }
}

void playMelody(int tones[], int durations[], int size) {
  if (currentNote < size) {
    int noteDuration = 1000 / durations[currentNote];
    tone(buzzer_pin, tones[currentNote], noteDuration);
    pauseBetweenNotes = noteDuration * 1.30;
    currentNote++;
  } else {
    // Melody is finished
    currentNote = 0;
    if (loopMelody != -1) {
      pauseBetweenNotes = loopMelody * 1000;
    } else {
      startMelody = false;
      melodyPlay = -1;
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
  assembledOutput += ((currentPowerState0 == -1) ? "Power Off" : (currentPowerState0 == 0) ? "Standby" : (coinEnable == false) ? "Startup" : "Active");
  return assembledOutput;
}

void defaultLEDState() {
  NeoPixelL.setBrightness(0);
  NeoPixelR.setBrightness(0);
  for (int i = 0; i < NUM_LEDS_1; i++) {
    leds1[i] = CRGB::Black;
  }
  for (int i = 0; i < NUM_LEDS_2; i++) {
    leds2[i] = CRGB::Black;
  }
  copyLEDBuffer();
}
void standbyLEDState() {
  NeoPixelL.setBrightness(32);
  NeoPixelR.setBrightness(32);
  setUpperLEDColor(CRGB::Black, false);
  setSideLEDs(CRGB::Yellow, CRGB::Yellow, false);
  copyLEDBuffer();
}
void startingLEDState() {
  setLEDControl(true);
  targetBrightness = 128;
  numSteps = 4.0 * 33.2;
  transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  setUpperLEDColor(CRGB::Black, true);
  setSideLEDs(CRGB::Green, CRGB::Green, true);
  pending_release_leds = true;
  animation_state = -1;
  animation_mode = 1;
  transition_leds = true;
}
void shuttingDownLEDState() {
  setLEDControl(true);
  targetBrightness = 255;
  numSteps = 4.0 * 33.2;
  transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  setUpperLEDColor(CRGB::Black, true);
  setSideLEDs(CRGB::Red, CRGB::Red, true);
  pending_release_leds = false;
  animation_state = -1;
  animation_mode = 1;
  transition_leds = true;
  melodyPlay = 3;
  loopMelody = 2;
  currentNote = 0;
  previousMelodyMillis = 0;
  startMelody = true;
}
void startLoadingScreen() {
  pending_release_display = true;
  kioskModeRequest("StartGame");
}

void setUpperLED(String ledValues, bool should_transition) {
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
void copyLEDBuffer() {
  for (int i = 0; i < NUM_LEDS_1; i++) {
    NeoPixelL.setPixelColor(i, NeoPixelL.Color(leds1[i].r, leds1[i].g, leds1[i].b));
  }
  for (int i = 0; i < NUM_LEDS_2; i++) {
    NeoPixelR.setPixelColor(i, NeoPixelR.Color(leds2[i].r, leds2[i].g, leds2[i].b));
  }
  NeoPixelL.show();
  NeoPixelR.show();
}

void handleSetLeds(String ledValues, int bankSelect, bool should_transition) {
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
    }
    index++;
  }
  if (should_transition == true) {
    for (int i = 0; i < NUM_LEDS_1; i++) {
      leds1_source[i] = leds1[i];
    }
    for (int i = 0; i < NUM_LEDS_2; i++) {
      leds2_source[i] = leds2[i];
    }
    bool all_same = true;
    for (int i = 0; i < NUM_LEDS_1; i++) {
      if (leds1_source[i].r != leds1_target[i].r && leds1_source[i].g != leds1_target[i].g && leds1_source[i].b != leds1_target[i].b) {
        all_same = false;
      }
    }
    if (sourceBrightness != targetBrightness) {
      all_same = false;
    }
    for (int i = 0; i < NUM_LEDS_2; i++) {
      if (leds2_source[i].r != leds2_target[i].r && leds2_source[i].g != leds2_target[i].g && leds2_source[i].b != leds2_target[i].b) {
        all_same = false;
      }
    }
    if (all_same == false) {
      transition_leds = true;
    } else {
      copyLEDBuffer();
    }
  } else {
    copyLEDBuffer();
  }
}
void handleSetLedColor(String ledValue, int bankSelect, bool should_transition) {
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
    for (int i = 0; i < NUM_LEDS_1; i++) {
      leds1_source[i] = leds1[i];
    }
    for (int i = 0; i < NUM_LEDS_2; i++) {
      leds2_source[i] = leds2[i];
    }
    bool all_same = true;
    for (int i = 0; i < NUM_LEDS_1; i++) {
      if (leds1_source[i].r != leds1_target[i].r && leds1_source[i].g != leds1_target[i].g && leds1_source[i].b != leds1_target[i].b) {
        all_same = false;
      }
    }
    if (sourceBrightness != targetBrightness) {
      all_same = false;
    }
    for (int i = 0; i < NUM_LEDS_2; i++) {
      if (leds2_source[i].r != leds2_target[i].r && leds2_source[i].g != leds2_target[i].g && leds2_source[i].b != leds2_target[i].b) {
        all_same = false;
      }
    }
    if (all_same == false) {
      transition_leds = true;
    } else {
      copyLEDBuffer();
    }
  } else {
    copyLEDBuffer();
  }
}

void setMasterPowerOn() {
  if (currentPowerState0 == -1) {
    setIOPower(true);
    delay(250);
    setLEDControl(true);
    kioskModeRequest("StartStandby");
    resetMarqueeState();
    setChassisFanSpeed(50);
    delay(500);
    defaultLEDState();
    standbyLEDState();
    setDisplayState(true);
    currentPowerState0 = 0;
  }
}
void setMasterPowerOff() {
  kioskModeRequest("StopAll");
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
  setDisplayState(true);
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
    startLoadingScreen();
    setMarqueeState(true, false);
    resetOmnimixState();
    delay(1000);
    setSysBoardPower(true);
    setDisplayState(true);
    melodyPlay = 0;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    pending_release_display = true;
  }
  currentPowerState0 = 1;
}
void setGameOff() {
  if (currentPowerState0 == 1) {
    pending_release_leds = false;
    pending_release_display = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    setDisplayState(true);
    setChassisFanSpeed(50);
    kioskModeRequest("StartStandby");
    setLEDControl(true);
    delay(250);
    standbyLEDState();
    resetMarqueeState();
    setSysBoardPower(false);
    delay(800);
    setOmnimixState(false, false);melodyPlay = 1;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
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
void setDisplayState(bool state) {
  if (state != displayMainState) {
    pushDisplaySwitch();
  }
}

void pushEthSwitch(int ethNum) {
  digitalWrite(ethButtons[ethNum], HIGH);
  digitalWrite(ethButtons[ethNum], LOW);
  delay(150);
  digitalWrite(ethButtons[ethNum], HIGH);
}
void pushDisplaySwitch() {
  digitalWrite(displayMainSelect, HIGH);
  digitalWrite(displayMainSelect, LOW);
  delay(500);
  digitalWrite(displayMainSelect, HIGH);
}

void kioskModeRequest(String command) {
  HTTPClient http;
  String url = String(hostURL + command);
  http.begin(url);
  int httpCode = http.GET();
  http.end();
  if (httpCode == 200) {
  }
}