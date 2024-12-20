#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Adafruit_DS3502.h>
#include <FastLED.h>
#include <SoftwareSerial.h>
#include "melody.h"
#include <WiFiUdp.h>
#include <WakeOnLan.h>

const char* ssid = "Radio Noise AX";
const char* password = "Radio Noise AX";
// Lifecycle Manager PC Mac Address
const char* KLM_MACAddress = "40:B0:34:F8:29:9A";
WebServer server(80);
WiFiUDP UDP;
WakeOnLan WOL(UDP);

// Power Controls
// 0 - Green - Display + PSU Enable
// 1 - Yellow - LED Select (Default ESP)
// 2 - Orange - LED 5V Enable
// 3 - Red - ALLS Power
// 4 - Red - IO 12V/5V Enable
// 5 - White - Marquee
int relayPins[6] = { 12, 14, 27, 26, 18, 25 };
int numRelays = 6;
// ALLS CONTROL PORT ("Wide" Pin Version)
// UART Serial to Arduino Nano
EspSoftwareSerial::UART nuControl;
const int nuControlTX = 15; // WHITE RED
const int nuControlRX = 5; // WHITE GREEN
String nuResponse = "";
// Fan Controller
const int fanPWM1 = 19;
const int fanPWM2 = 32;
// Aux
// Card Reader Communication
HardwareSerial cardReaderSerial(2);
bool coinEnable = false;
bool has_cr_talked = false;
// Display Switch
// 23 - Select
// 15 - State
const int displayMainSelect = 23;
const int displayMainLDR = 36;
bool displayMainState = false;
// WACCA LED Controls
#define ledOutput 13
#define NUM_LEDS 480 // 8 x 5 x 12 (8 Col, 60 Rows)
CRGB leds[NUM_LEDS];
CRGB leds_source[NUM_LEDS];
CRGB leds_target[NUM_LEDS];
CRGBPalette16 currentPalette;
TBlendType currentBlendType = LINEARBLEND;
uint8_t sourceBrightness = 0;
uint8_t targetBrightness = 0;
// Beeper Tones
int buzzer_pin = 33;
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
int warning_tone_long[] = {
  NOTE_A5, NOTE_F5
};
int warning_tone_long_dur[] = {
  4, 4
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
// Occupancy and Timer
int requestedPowerState0 = -1;
int defaultInactivityMinTimeout = 45;
int inactivityMinTimeout = 45;
int defaultPowerOffDelayMinTimeout = 35;
int powerOffDelayMinTimeout = 35;
const int shutdownDelayMinTimeout = 3;
unsigned long previousInactivityMillis = 0;
unsigned long previousShutdownMillis = 0;
bool inactivityTimeout = true;

// DISPLAY_MESSAGE::BIG::icon::text::isJP/t::255::invert/t::timeout/20
int typeOfMessage = -1;
int messageIcon = 0;
String messageText = "";
bool isJpnMessage = false;
int brightMessage = 1;
bool invertMessage = false;
int timeoutMessage = 0;

unsigned long previousMillis = 0;
uint8_t numSteps = 120; // Number of steps in the transition
uint8_t currentStep = 0;
bool pending_release_leds = false;
bool pending_release_display = false;
bool transition_leds = false;
float transition_interval = 0;
int animation_mode = -1;
int animation_state = -1;

bool lockedState = false;
bool requestPowerMgrOn = false;
bool keepManagerAwake = false;
bool ready_to_boot = false;
String inputString = "";
String attachedSoftwareCU = "Unknown";
int currentVolume = 0;
bool muteVolume = false;
int minimumVolume = 10;
int maximumVolume = 127;
bool inhibitNuState = false;
bool ultraPowerSaving = true;
int shutdownPCTimer = 0;
unsigned long powerInactivityMillis = 0;
unsigned long lastPingMillis = 0;
int currentGameSelected0 = -1;
int requestedGameSelected0 = 0;
int currentNuPowerState0 = -1;
int currentNuBoardState0 = -1;
bool enhancedStandby = false;
int currentALLSLANMode0 = 0;
int currentPowerState0 = -1;
int currentLEDState = 0;
int currentMarqueeState = 1;
int currentFan1Speed = 128;
int currentFan2Speed = 128;
int displayedSec = 0;
int displayState = -1;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
Adafruit_DS3502 ds3502 = Adafruit_DS3502();
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
TaskHandle_t Task5;
TaskHandle_t Task6;
TaskHandle_t Task7;
TaskHandle_t Task8;
TaskHandle_t Task9;
TaskHandle_t Task10;

const char *shutdown_url = "http://192.168.100.32:9052/shutdown";

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Attempting to reconnect...");
    WiFi.hostname("XLink-MCU-WACCA");
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    int tryCount = 0;
    tone(buzzer_pin, NOTE_CS5, 1000 / 8);
    while (WiFi.status() != WL_CONNECTED) {
      //if (tryCount > 60 && currentPowerState0 != 1) {
      //  ESP.restart();
      //}
      tone(buzzer_pin, (tryCount % 2 == 0) ? NOTE_GS5 : NOTE_CS5, 1000 / 8);
      delay(500);
      Serial.print(".");
      tryCount++;
    }
    noTone(buzzer_pin);
    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup() {
  pinMode(displayMainSelect, OUTPUT);
  digitalWrite(displayMainSelect, HIGH);
  inputString.reserve(200);
  attachedSoftwareCU.reserve(200);
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  WOL.setRepeat(3, 100);
  bootScreen("HARDWARE");
  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  if (!ds3502.begin()) {
    Serial.println("Couldn't find DS3502 chip");
    bootScreen("I2C VOL FAILURE");
  }
  FastLED.addLeds<WS2812, ledOutput, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);;
  FastLED.setBrightness(64);
  pinMode(displayMainLDR, INPUT);
  pinMode(fanPWM1, OUTPUT);
  pinMode(fanPWM2, OUTPUT);
  tone(buzzer_pin, NOTE_C6);

  bootScreen("NU_CTRL_COM");
  nuControl.begin(4800, SWSERIAL_8N1, nuControlRX, nuControlTX, false);
  if (!nuControl) {
    bootScreen("NU_COM_FAIL");
    while (1) { // Don't continue with invalid configuration
      delay (1000);
    }
  }
  xTaskCreatePinnedToCore(
                  nuControlRXLoop,   /* Task function. */
                  "nuControlRX",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &Task6,      /* Task handle to keep track of created task */
                  1);          /* pin task to core 1 */
  while (nuResponse.substring(0,1) != "P") {
    delay(500);
    nuControl.println("P::");
  }
  nuResponse = "";
  while (nuResponse == "") {
    nuControl.println("PS::0");
    delay(100);
  }
  nuResponse = "";

  bootScreen("NETWORK");
  checkWiFiConnection();
  tone(buzzer_pin, NOTE_C6);

  bootScreen("REQ_PC_PWR");
  WOL.sendMagicPacket(KLM_MACAddress);
  delay(500);

  bootScreen("PC_LINK");
  xTaskCreatePinnedToCore(
                    pingLoop,   /* Task function. */
                    "pingTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task3,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  kioskTest();
  tone(buzzer_pin, NOTE_C6);

  bootScreen("CARD_LINK");
  cardReaderSerial.begin(9600, SERIAL_8N1, 16, 17);
  if (!cardReaderSerial) {
    bootScreen("CARDREADER_FAIL_1");
    while (1) { // Don't continue with invalid configuration
      delay (1000);
    }
  }
  tone(buzzer_pin, NOTE_C6);
  delay(500);

  bootScreen("RST_READER");
  for (int i = 0; i < 3; i++) {
    cardReaderSerial.println("REBOOT::NO_DATA");
    delay(100);
  }
  testCardReader();

  bootScreen("CHK_HDMI");
  if (digitalRead(displayMainLDR) != LOW) {
    digitalWrite(displayMainSelect, HIGH);
    digitalWrite(displayMainSelect, LOW);
    delay(500);
    digitalWrite(displayMainSelect, HIGH);
  }
  digitalWrite(relayPins[3], HIGH);
  digitalWrite(relayPins[4], LOW);
  for (int i = 0; i < 3; i++) {
    nuControl.println("DS::0");
    delay(100);
  }
  for (int i = 0; i < 3; i++) {
    nuControl.println("PS::1");
    delay(100);
  }
  delay(6000);
  if (digitalRead(displayMainLDR) != LOW) {
    bootScreen("HDMI_FIX");
    digitalWrite(displayMainSelect, HIGH);
    digitalWrite(displayMainSelect, LOW);
    delay(5000);
    digitalWrite(displayMainSelect, HIGH);
  } else {
    bootScreen("HDMI_OK");
    delay(1000);
  }
  digitalWrite(relayPins[3], LOW);
  digitalWrite(relayPins[4], LOW);
  for (int i = 0; i < 3; i++) {
    nuControl.println("PS::0");
    delay(100);
  }

  noTone(buzzer_pin);
  bootScreen("DEFAULTS");
  currentVolume = map(25, 0, 100, minimumVolume, maximumVolume);
  ds3502.setWiper(currentVolume);
  ds3502.setWiperDefault(currentVolume);
  kioskModeRequest("StopAll");
  displayMainState = (digitalRead(displayMainLDR) == LOW);
  delay(2000);

  bootScreen("REMOTE_ACCESS");
  server.on("/volume/set", [=]() {
    int _currentVolume = currentVolume;
    if (server.hasArg("wiper")) {
      int _volVal = server.arg("wiper").toInt();
      if (_volVal > 0 && _volVal <= 127) {
        currentVolume = _volVal;
        muteVolume = false;
        server.send(200, "text/plain", String(currentVolume));
      }
    } else if (server.hasArg("down")) {
      int _volVal = server.arg("down").toInt();
      if (_volVal > 0 && _volVal <= 100) {
        int current_percent = map(currentVolume, minimumVolume, maximumVolume, 0, 100);
        current_percent -= _volVal;
        currentVolume = map(current_percent, 0, 100, minimumVolume, maximumVolume);
        muteVolume = false;
        server.send(200, "text/plain", String(current_percent));
      }
    } else if (server.hasArg("up")) {
      int _volVal = server.arg("up").toInt();
      if (_volVal > 0 && _volVal <= 100) {
        int current_percent = map(currentVolume, minimumVolume, maximumVolume, 0, 100);
        current_percent += _volVal;
        currentVolume = map(current_percent, 0, 100, minimumVolume, maximumVolume);
        muteVolume = false;
        server.send(200, "text/plain", String(current_percent));
      }
    } else if (server.hasArg("percent")) {
      int _volVal = server.arg("percent").toInt();
      if (_volVal > 0 && _volVal <= 100) {
        currentVolume = map(_volVal, 0, 100, minimumVolume, maximumVolume);
        muteVolume = false;
        server.send(200, "text/plain", String(_volVal));
      }
    } else if (server.hasArg("mute")) {
      String _muteVal = server.arg("mute");
      if (server.hasArg("invert")) {
        muteVolume = (_muteVal != "true");
        server.send(200, "text/plain", "OK");
      } else {
        muteVolume = (_muteVal == "true");
        server.send(200, "text/plain", "OK");
      }
    }
    ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    if (_currentVolume != currentVolume) {
      displayVolumeMessage();
    }
  });
  server.on("/volume", [=]() {
    String response = "";
    if (server.hasArg("mute")) {
      if (server.hasArg("invert")) {
        response += ((muteVolume == true) ? "0" : "1");
      } else {
        response += ((muteVolume == true) ? "1" : "0");
      }
    } else {
      response += map(currentVolume, minimumVolume, maximumVolume, 0, 100);
    }
    server.send(200, "text/plain", response);
  });
  server.on("/fan/1/set", [=]() {
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
  server.on("/fan/2/set", [=]() {
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
      setMainFanSpeed(fanSpeed);
    }
    server.send(200, "text/plain", response);
  });
  server.on("/fan/1/reset", [=]() {
    String response = "OK";
    if (currentPowerState0 == 0) {
      setChassisFanSpeed(35);
    } else if (currentPowerState0 <= -1) {
      setChassisFanSpeed(0);
    } else if (currentPowerState0 == 1) {
      setChassisFanSpeed(75);
    }
    server.send(200, "text/plain", response);
  });
  server.on("/fan/2/reset", [=]() {
    String response = "OK";
    if (currentPowerState0 == 0) {
      setMainFanSpeed((enhancedStandby == true) ? 100 : 25);
    } else if (currentPowerState0 == -2) {
      setMainFanSpeed(0);
    } else if (currentPowerState0 == -1) {
      setMainFanSpeed((enhancedStandby == true) ? 75 : 15);
    } else if (currentPowerState0 == 1) {
      setMainFanSpeed(100);
    }
    server.send(200, "text/plain", response);
  });
  server.on("/fan/1", [=]() {
    String response = "";
    response += map(currentFan1Speed, 0, 255, 0, 100);
    server.send(200, "text/plain", response);
  });
  server.on("/fan/2", [=]() {
    String response = "";
    response += map(currentFan2Speed, 0, 255, 0, 100);
    server.send(200, "text/plain", response);
  });

  server.on("/display/pc", [=]() {
    setDisplayState(true);
    server.send(200, "text/plain", (displayMainState == true) ? "UNCHANGED" : "OK");
  });
  server.on("/display/game", [=]() {
    setDisplayState(false);
    server.send(200, "text/plain", (displayMainState == false) ? "UNCHANGED" : "OK");
  });
  server.on("/display/switch", [=]() {
    String assembledOutput = "";
    pushDisplaySwitch();
    assembledOutput += ((displayMainState == false) ? "PC" : "GAME");
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/display", [=]() {
    String assembledOutput = "";
    assembledOutput += ((displayMainState == true) ? "PC" : "GAME");
    server.send(200, "text/plain", assembledOutput);
  });

  server.on("/select/game/reverse", [=]() {
    if (currentGameSelected0 != 0) {
      requestedGameSelected0 = 0;
      if (enhancedStandby == false || currentPowerState0 == 1) {
        setGameDisk(0);
      }
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/lilly", [=]() {
    if (currentGameSelected0 != 1) {
      requestedGameSelected0 = 1;
      if (enhancedStandby == false || currentPowerState0 == 1) {
        setGameDisk(1);
      }
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game", [=]() {
    String const val = getGameSelect();
    server.send(200, "text/plain", val);
  });

  server.on("/power/off", [=]() {
    server.send(200, "text/plain", (currentPowerState0 <= -1) ? "UNCHNAGED" : "OK");
    if (currentPowerState0 > -1) {
      setMasterPowerOff();
      currentPowerState0 = -1;
    }
  });
  server.on("/power/standby", [=]() {
    if (currentPowerState0 == 1) {
      setGameOff();
      server.send(200, "text/plain", "OK");
    } else if (currentPowerState0 <= -1 || (currentPowerState0 == 0 && currentNuPowerState0 != ((enhancedStandby == true) ? 1 : 0))) {
      setMasterPowerOn();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/power/on", [=]() {
    server.send(200, "text/plain", (currentPowerState0 == 1) ? "UNCHNAGED" : "OK");
    setGameOn();
  });
  server.on("/request/standby", [=]() {
    server.send(200, "text/plain", "OK");
    if (currentPowerState0 == 1 && coinEnable == false) {
      if (server.hasArg("nonauthoritive")) {
      } else {
        setGameOff();
      }
    } else if (currentPowerState0 == 1 && coinEnable == true) {
      if (server.hasArg("nonauthoritive")) {
      } else {
        shuttingDownLEDState(1);
      }
    } else if (currentPowerState0 <= -1 || (currentPowerState0 == 0 && currentNuPowerState0 != ((enhancedStandby == true) ? 1 : 0))) {
      setMasterPowerOn();
    }
  });
  server.on("/request/off", [=]() {
    server.send(200, "text/plain", "OK");
    requestPowerOff();
  });
  server.on("/enhanced_standby/enable", [=]() {
    server.send(200, "text/plain", (enhancedStandby == true) ? "UNCHANGED" : "OK");
    enhancedStandby = true;
    if (currentPowerState0 != 1 && currentNuPowerState0 == 0) {
      enterEnhancedStandby(true);
    }
  });
  server.on("/enhanced_standby/disable", [=]() {
    server.send(200, "text/plain", (enhancedStandby == false) ? "UNCHANGED" : "OK");
    enhancedStandby = false;
    if (currentPowerState0 != 1 && currentNuPowerState0 == 1) {
      enterEnhancedStandby(false);
    }
  });
  server.on("/enhanced_standby", [=]() {
    server.send(200, "text/plain", (enhancedStandby == true) ? "Enabled" : "Disabled");
  });
  server.on("/enhanced_standby/avalible", [=]() {
    server.send(200, "text/plain", (enhancedStandby == true) ? (currentPowerState0 == 1) ? "BLOCKED" : "YES" : "NO");
  });

  server.on("/test/alls/off", [=]() {
    setDisplayState(true);
    if (enhancedStandby == true) {
      enterEnhancedStandby(true);
    } else {
      nuResponse = "";
      while (currentNuPowerState0 == 1) {
        nuControl.println("PS::0");
        delay(100);
      }
      digitalWrite(relayPins[3], LOW);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/alls/on", [=]() {
    setDisplayState(false);
    digitalWrite(relayPins[3], HIGH);
    if (requestedGameSelected0 != currentGameSelected0) {
      setGameDisk(requestedGameSelected0);
    }
    nuResponse = "";
    while (currentNuPowerState0 == 0) {
      nuControl.println("PS::1");
      delay(100);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/alls/reset", [=]() {
    setDisplayState(false);
    int i = 0;
    while (currentGameSelected0 != 2 && i < 5) {
      nuControl.println("PS::128");
      delay(125);
      i++;
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/sysbrd/off", [=]() {
    setSysBoardPower(false);
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/sysbrd/on", [=]() {
    setSysBoardPower(true);
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/sysbrd/reset", [=]() {
    nuResponse = "";
    while (nuResponse == "") {
      nuControl.println("PS::128");
      delay(100);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/display_switch/hold", [=]() {
    digitalWrite(displayMainSelect, HIGH);
    digitalWrite(displayMainSelect, LOW);
    delay(5000);
    digitalWrite(displayMainSelect, HIGH);
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/melody", [=]() {
    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/display", [=]() {
    messageIcon = 223;
    messageText = "Display Test";
    isJpnMessage = false;
    brightMessage = 255;
    invertMessage = false;
    timeoutMessage = 5;
    typeOfMessage = 1;
    server.send(200, "text/plain", "OK");
  });

  server.on("/power", [=]() {
    String const val = getPowerAuth();
    server.send(200, "text/plain", val);
  });
  server.on("/master_power", [=]() {
    server.send(200, "text/plain", (requestedPowerState0 == 0) ? "Warning" : ((currentPowerState0 > -1) ? "Enabled" : (currentPowerState0 == -2) ? "Eco" : "Disabled"));
  });
  server.on("/game_power", [=]() {
    server.send(200, "text/plain", (requestedPowerState0 >= 0) ? "Warning" : ((currentPowerState0 == 1) ? "Enabled" : (currentPowerState0 == -2) ? "Eco" : "Disabled"));
  });

  server.on("/timeout", [=]() {
    server.send(200, "text/plain", ((inactivityTimeout == true) ? "ON" : "OFF"));
  });
  server.on("/timeout/on", [=]() {
    if (inactivityTimeout == false) {
      resetInactivityTimer();
      inactivityTimeout = true;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/timeout/off", [=]() {
     if (inactivityTimeout == true) {
      inactivityTimeout = false;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/timeout/time/set", [=]() {
    String response = "UNCHANGED";
    if (server.hasArg("time")) {
      int _time = server.arg("time").toInt();
      defaultInactivityMinTimeout = _time;
      resetInactivityTimer();
      response = "SET";
    }
    server.send(200, "text/plain", response);
  });

  server.on("/power_save", [=]() {
    server.send(200, "text/plain", ((ultraPowerSaving == true) ? (currentPowerState0 == -2) ? "ACTIVE" : "ON" : "OFF"));
  });
  server.on("/power_save/on", [=]() {
    if (ultraPowerSaving == false) {
      ultraPowerSaving = true;
      if (currentPowerState0 == -1) {
        powerInactivityMillis = millis();
      }
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/power_save/off", [=]() {
     if (ultraPowerSaving == true) {
      ultraPowerSaving = false;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/power_save/dlpm/state", [=]() {
    if (ultraPowerSaving == false)
    server.send(200, "text/plain", ((ultraPowerSaving == false) ? "DISABLED" : (shutdownPCTimer == 0) ? "POWER ON" : (shutdownPCTimer == 1) ? "REBOOTING" : (shutdownPCTimer == 2) ? "POWER OFF" : "????"));
  });
  server.on("/power_save/dlpm/timeout", [=]() {
    server.send(200, "text/plain", ((ultraPowerSaving == false) ? "DISABLED" : (shutdownPCTimer == 0) ? String((millis() - powerInactivityMillis) / 60000) : (shutdownPCTimer == 1) ? "REBOOTING" : (shutdownPCTimer == 2) ? "POWER OFF" : "????"));
  });
  server.on("/power_save/wake", [=]() {
    if (ultraPowerSaving == true) {
      if (shutdownPCTimer != 0) {
        requestPowerMgrOn = true;
        keepManagerAwake = true;
      }
      if (currentPowerState0 == -2) {
        currentPowerState0 = -1;
      }
      server.send(200, "text/plain", (shutdownPCTimer != 0) ? "POWERING ON" : "UNCHANGED");
    } else {
      server.send(200, "text/plain", "DISABLED");
    }
  });
  server.on("/power_save/unlock", [=]() {
    if (ultraPowerSaving == true) {
      keepManagerAwake = false;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "DISABLED");
    }
  });
  server.on("/power_save/lock", [=]() {
    if (ultraPowerSaving == true) {
      keepManagerAwake = true;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "DISABLED");
    }
  });
  server.on("/power_save/force", [=]() {
    if (ultraPowerSaving == true) {
      if (currentPowerState0 == -1) {
        powerInactivityMillis = (powerInactivityMillis - (powerOffDelayMinTimeout * 60000));
      }
      keepManagerAwake = false;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "DISABLED");
    }
  });
  server.on("/power_save/time/set", [=]() {
    String response = "UNCHANGED";
    if (server.hasArg("time") && ultraPowerSaving == true) {
      int _time = server.arg("time").toInt();
      powerOffDelayMinTimeout = _time;
      response = "SET";
    }
    server.send(200, "text/plain", response);
  });
  server.on("/power_save/time/reset", [=]() {
    String response = "RESET";
    powerOffDelayMinTimeout = defaultPowerOffDelayMinTimeout;
    server.send(200, "text/plain", response);
  });

  server.on("/marquee/on", [=]() {
    if (currentMarqueeState == 0) {
      setMarqueeState(true, true);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/marquee/off", [=]() {
    if (currentMarqueeState == 1) {
      setMarqueeState(false, true);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/marquee", [=]() {
    server.send(200, "text/plain", ((currentMarqueeState == 0) ? (currentPowerState0 == 1) ? "Enabled" : "Disabled" : "Enabled"));
  });

  server.on("/setLED", [=]() {
    String ledValues = server.arg("ledValues");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Data Sent");
    if (pending_release_leds == false && currentPowerState0 != 1) {
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
    }
  });
  server.on("/setLEDColor", [=]() {
    String ledValue = server.arg("ledColor");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Data Sent");
    if (pending_release_leds == false && currentPowerState0 != 1) {
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
    }
  });
  server.on("/state", [=]() {
    server.send(200, "text/plain", (currentLEDState == 0)  ? "MCU" : "GAME");
  });
  server.on("/state/return", [=]() {
    String ledValues = server.arg("ledValues");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Returned to owner");
    resetState();
  });
  server.on("/state/reset", [=]() {
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

  delay(250);
  //bootScreen("SYS_PWR_ON");
  //setMasterPowerOn();
  //delay(500);
  bootScreen("TASK_PCLINK");
  xTaskCreatePinnedToCore(
                  serialLoop,   /* Task function. */
                  "serialMonitor",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &Task3,      /* Task handle to keep track of created task */
                  1);          /* pin task to core 1 */
  delay(101);
  bootScreen("TASK_CARDCOM");
  xTaskCreatePinnedToCore(
                  cardReaderRXLoop,   /* Task function. */
                  "cardReaderRX",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &Task4,      /* Task handle to keep track of created task */
                  1);          /* pin task to core 1 */
  delay(101);
  xTaskCreatePinnedToCore(
                  cardReaderTXLoop,   /* Task function. */
                  "cardReaderTX",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &Task5,      /* Task handle to keep track of created task */
                  0);          /* pin task to core 1 */
  delay(101);
  bootScreen("TASK_SOUND");
  xTaskCreatePinnedToCore(
                    melodyPlayer,   /* Task function. */
                    "melodyTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    10,           /* priority of the task */
                    &Task7,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  delay(101);
  bootScreen("TASK_DLPM");
  xTaskCreatePinnedToCore(
                    powerManagerLoop,   /* Task function. */
                    "powerManager",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    20,           /* priority of the task */
                    &Task10,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  delay(101);
  bootScreen("TASK_DISPLAY");
  xTaskCreatePinnedToCore(
                    cpu0Loop,   /* Task function. */
                    "driverTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    10,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  delay(101);
  melodyPlay = 2;
  startMelody = true;
}

const int DEBOUNCE_COUNT = 4;  // Number of consistent readings required
int stateBuffer[DEBOUNCE_COUNT] = {0};  // Buffer to store recent readings
int bufferIndex = 0;                   // Current index in the buffer

void updateDisplayMainState() {
    // Read the current value
    int currentState = (digitalRead(displayMainLDR) == LOW) ? 1 : 0;

    // Update the buffer
    stateBuffer[bufferIndex] = currentState;
    bufferIndex = (bufferIndex + 1) % DEBOUNCE_COUNT;

    // Check if all values in the buffer are the same
    bool consistent = true;
    for (int i = 1; i < DEBOUNCE_COUNT; i++) {
        if (stateBuffer[i] != stateBuffer[0]) {
            consistent = false;
            break;
        }
    }

    // If consistent, update the final state
    if (consistent) {
        displayMainState = (stateBuffer[0] == 1);
    }
}

void loop() {
  checkWiFiConnection();
  server.handleClient();
  unsigned long currentMillis = millis();
  // Handle LED Handover
  if (pending_release_leds == true && coinEnable == true) {
    setLEDControl(false);
    pending_release_leds = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    currentStep = 0;
  } else if (pending_release_leds == false && coinEnable == false && currentPowerState0 == 1) {
    startingLEDState();
  }
  // Handle Display Handover
  updateDisplayMainState();
  if (pending_release_display == true && coinEnable == true) {
    pending_release_display = false;
    kioskModeRequest("GameRunning");
    if (currentGameSelected0 < 2) {
      delay(6000);
      setDisplayState(false);
    }
  }
  // Drive LED Animations
  if (transition_leds == true) {
    if (currentStep == 0) {
      sourceBrightness = FastLED.getBrightness();
      for (int i = 0; i < NUM_LEDS; i++) {
        leds_source[i] = leds[i];
      }
    }
    if (currentMillis - previousMillis >= transition_interval) {
      previousMillis = currentMillis;
      if (animation_mode == 2) {
        numSteps = 3 * 33.2;
        transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
        if (currentStep <= numSteps) {
          FastLED.setBrightness(map(currentStep, 0, numSteps, sourceBrightness, targetBrightness));
          for (int row = 0; row < 60; ++row) {
            for (int col = 0; col < 8; ++col) {
              int i = row * 8 + col;
              int hue = currentStep + i * 4; /* 4 is the hue step between LEDs */
              leds_target[i] = ColorFromPalette(currentPalette, hue, 255, currentBlendType);
              leds[i].r = map(currentStep, 0, numSteps, leds_source[i].r, leds_target[i].r);
              leds[i].g = map(currentStep, 0, numSteps, leds_source[i].g, leds_target[i].g);
              leds[i].b = map(currentStep, 0, numSteps, leds_source[i].b, leds_target[i].b);
            }
          }
        } else {
          for (int row = 0; row < 60; ++row) {
            for (int col = 0; col < 8; ++col) {
              int i = row * 8 + col;
              int hue = currentStep + i * 4; /* 4 is the hue step between LEDs */
              leds[i] = ColorFromPalette(currentPalette, hue, 255, currentBlendType);
            }
          }
        }
        FastLED.show();
        currentStep++;
        animation_state = 1;
      } else if (currentStep <= numSteps) {
        FastLED.setBrightness(map(currentStep, 0, numSteps, sourceBrightness, targetBrightness));
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i].r = map(currentStep, 0, numSteps, leds_source[i].r, leds_target[i].r);
          leds[i].g = map(currentStep, 0, numSteps, leds_source[i].g, leds_target[i].g);
          leds[i].b = map(currentStep, 0, numSteps, leds_source[i].b, leds_target[i].b);
        }
        FastLED.show();
        currentStep++;
      } else if (animation_mode == 1) {
        currentStep = 0;
        if (animation_state == -1) {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds_target[i] = CRGB::Black;
          }
          numSteps = 3 * 33.2;
          transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
          animation_state = 1;
        } else {
          for (int i = 0; i < NUM_LEDS; i++) {
            leds_target[i] = leds_source[i];
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
        //if (pending_release_leds == true) {
        //  setLEDControl(false);
        //}
        pending_release_leds = false;
        sourceBrightness = FastLED.getBrightness();
        FastLED.show();
      }
    }
  }
  // Handle Shutdown Timer
  if (currentPowerState0 == 1 && requestedPowerState0 > -1 && currentMillis - previousShutdownMillis >= (shutdownDelayMinTimeout * 60000)) {
    if (requestedPowerState0 == 0) {
      setMasterPowerOff();
    } else if (requestedPowerState0 == 1) {
      setGameOff();
    }
    requestedPowerState0 = -1;
  } else if (currentPowerState0 == 0 && requestedPowerState0 > -1) {
    if (requestedPowerState0 == 0) {
      setMasterPowerOff();
    }
    requestedPowerState0 = -1;
  }
  // Handle Inactivity Timer
  if (inactivityTimeout == true && currentPowerState0 == 1 && requestedPowerState0 == -1 && currentMillis - previousInactivityMillis >= (inactivityMinTimeout * 60000)) {
    if (coinEnable == false) {
      setGameOff();
    } else if (coinEnable == true) {
      shuttingDownLEDState(1);
    }
  }
}

void cpu0Loop( void * pvParameters ) {
  for(;;) {
    runtime();
  }
}
void remoteAccessLoop( void * pvParameters ) {
  for(;;) {
    delay(1);
  }
}
void melodyPlayer( void * pvParameters ) {
  for(;;) {
    // Play Melody Sheets
    if (startMelody == true) {
      if (millis() - previousMelodyMillis >= pauseBetweenNotes) {
        previousMelodyMillis = millis();
        if (melodyPlay == 0) {
          playMelody(booting_tone, booting_tone_dur, sizeof(booting_tone_dur) / sizeof(int));
        } else if (melodyPlay == 1) {
          playMelody(shutting_down_tone, shutting_down_dur, sizeof(shutting_down_dur) / sizeof(int));
        } else if (melodyPlay == 2) {
          playMelody(boot_tone, boot_tone_dur, sizeof(boot_tone_dur) / sizeof(int));
        } else if (melodyPlay == 3) {
          const float position = ((millis() - previousShutdownMillis) / 60000);
          int val = map(position, 0, 4, loopMelody, 0) * 1000;
          if (val <= 1) {
            playMelody(warning_tone_long, warning_tone_long_dur, sizeof(warning_tone_long_dur) / sizeof(int));
          } else {
            playMelody(warning_tone, warning_tone_dur, sizeof(warning_tone_dur) / sizeof(int));
          }
        }
      }
    }
    delay(1);
  }
}
void pingLoop( void * pvParameters ) {
  for(;;) {
    delay(1000);
    Serial.println("");
    Serial.println("PROBE::SEARCH");
    Serial.println("");
  }
}
void serialLoop( void * pvParameters ) {
  kioskCommand();
}
void cardReaderTXLoop( void * pvParameters ) {
  for(;;) {
    if (typeOfMessage != -1) {
      // DISPLAY_MESSAGE::BIG::icon::text::isJP/t::255::invert/t::timeout/20
      cardReaderSerial.print("DISPLAY_MESSAGE::");
      cardReaderSerial.print((typeOfMessage == 1) ? "BIG" : "SMALL");
      typeOfMessage = -1;
      cardReaderSerial.print("::");
      cardReaderSerial.print(String(messageIcon));
      messageIcon = 0;
      cardReaderSerial.print("::");
      cardReaderSerial.print(messageText);
      messageText = "";
      cardReaderSerial.print("::");
      cardReaderSerial.print((isJpnMessage == true) ? "t" : "f");
      isJpnMessage = false;
      cardReaderSerial.print("::");
      cardReaderSerial.print(String(brightMessage));
      brightMessage = 1;
      cardReaderSerial.print("::");
      cardReaderSerial.print((invertMessage == true) ? "t" : "f");
      invertMessage = false;
      cardReaderSerial.print("::");
      cardReaderSerial.println(String(timeoutMessage));
      timeoutMessage = 0;
    }
    int value = 0;
    if (currentPowerState0 <= -1 || (currentPowerState0 == 1 && requestedPowerState0 != -1)) {
      value = 0;
    } else if (currentPowerState0 == 0) {
      value = 1;
    } else if (currentPowerState0 == 1 && requestedPowerState0 == -1) {
      value = 2;
    }
    cardReaderSerial.println("POWER_SWITCH::" + String(value));
    delay(100);
  }
}
void cardReaderRXLoop( void * pvParameters ) {
  for(;;) {
    if (cardReaderSerial.available()) {
      static String receivedMessage = "";
      char c;
      bool messageStarted = false;

      while (cardReaderSerial.available()) {
        c = cardReaderSerial.read();
        if (c == '\n') {
          if (!receivedMessage.isEmpty()) {
            handleCRMessage(receivedMessage);
            //Serial.println("Received: " + receivedMessage);
          }
          receivedMessage = "";
        } else {
          receivedMessage += c;
        }

      }
    } else {
      delay(1);
    }
  }
}
void nuControlRXLoop( void * pvParameters ) {
  for(;;) {
    if (nuControl.available()) {
      static String receivedMessage = "";
      char c;
      bool messageStarted = false;

      while (nuControl.available()) {
        c = nuControl.read();
        if (c == '\n') {
          if (!receivedMessage.isEmpty()) {
            int delimiterIndex = receivedMessage.indexOf("::");
            if (delimiterIndex != -1) {
              int headerIndex = receivedMessage.indexOf("::");
              String header = receivedMessage.substring(0, headerIndex);
              if (header == "PS") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                if (valueInt >= 0 && valueInt <= 2) {
                  currentNuPowerState0 = valueInt;
                }
              } else if (header == "BS") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                if (valueInt >= 0 && valueInt <= 2) {
                  currentNuBoardState0 = valueInt;
                }
              } else if (header == "DS") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                if (valueInt >= 0 && valueInt <= 9 && inhibitNuState == false) {
                  currentGameSelected0 = valueInt;
                }
              } else if (header == "NS") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                if (valueInt >= 0 && valueInt <= 9 && inhibitNuState == false) {
                  currentALLSLANMode0 = valueInt;
                }
              } else if (header == "R") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String _nuResponse = receivedMessage.substring(headerIndex + 2, valueIndex);
                _nuResponse.trim();
                nuResponse = _nuResponse;
                Serial.println("NU_CTRL::" + nuResponse);
              } else if (header == "DBG") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String _nuResponse = receivedMessage.substring(headerIndex + 2, valueIndex);
                _nuResponse.trim();
                Serial.println("NU_CTRL_DEBUGGER::" + _nuResponse);
              }
            }
          }
          receivedMessage = "";
        } else {
          receivedMessage += c;
        }

      }
    } else {
      delay(1);
    }
  }
}
void powerManagerLoop( void * pvParameters ) {
  for(;;) {
    if (ultraPowerSaving == true && requestPowerMgrOn == true) {
      powerOnManager(true);
      if (coinEnable == true) {
        kioskModeRequest((currentGameSelected0 >= 10) ? "GameRunningALLS" : "GameRunning");
      } else if (currentPowerState0 == 0) {
        setLEDControl(true);
        defaultLEDState();
        standbyLEDState();
        kioskModeRequest("StartStandby");
      }
      requestPowerMgrOn = false;
    }  else if (ultraPowerSaving == true && keepManagerAwake == false && currentPowerState0 == -1 && powerInactivityMillis != -1 && shutdownPCTimer == 0) {
      unsigned long currentMillis = millis();
      if ((currentMillis - powerInactivityMillis) >= (powerOffDelayMinTimeout * 60000)) {
        lockedState = true;
        while (shutdownPCTimer != 2) {
          Serial.println("");
          Serial.println("DLPM::POWER_OFF");
          Serial.println("");
          delay(10000);
        }
        powerInactivityMillis = -1;
        currentPowerState0 = -2;
        setChassisFanSpeed(0);
        setMainFanSpeed((enhancedStandby == true) ? 75 : 0);
        delay(10000);
      } else {
        delay(60000);
      }
    } else if (ultraPowerSaving == false && currentPowerState0 == -2 && shutdownPCTimer != 0) {
      powerOnManager(false);
      delay(5000);
    } else {
      delay(500);
    }
  }
}

void runtime() {
  int time_in_sec = esp_timer_get_time() / 1000000;
  unsigned long currentMillis = millis();
  int current_time = (time_in_sec - displayedSec) / 2;

  if (displayState != 1 && currentPowerState0 <= -1) {
    const String power = getPowerAuth();
    displayIconDualMessage(1, false, false, (power == "Active") ? 491 : 490, "System Power", power);
    displayState = 1;
  } else if (currentPowerState0 <= -1) {
    delay(500);
    displayState = 0;
  } else if (currentPowerState0 != -1) {
    if (displayState != 0 && current_time < 1) {
      displayIconMessage(1, true, true, 250, "ワッカ");
      displayState = 0;
    } else if (displayState != 1 && current_time >= 1 && current_time < 2) {
      const String power = getPowerAuth();
      displayIconDualMessage(1, (power == "Active"), false, (power == "Active") ? 491 : 490, "System Power", power);
      displayState = 1;
    } else if (displayState != 2 && current_time >= 2 && current_time < 3) {
      String timeout = "";
      if (requestedPowerState0 != -1) {
        timeout = "Expired";
      } else if (currentPowerState0 == 1 && inactivityTimeout == true) {
        timeout = String(inactivityMinTimeout - ((millis() - previousInactivityMillis) / 60000));
        timeout += " Min";
      } else {
        timeout = "Disabled";
      }
      displayIconDualMessage(1, ((currentPowerState0 == 1 && inactivityTimeout == true && (((millis() - previousInactivityMillis) / 60000) < (inactivityMinTimeout - 5))) ? true : false), false, 459, "Timeout", timeout);
      displayState = 2;
    } else if (displayState != 3 && current_time >= 3 && current_time < 4) {
      String volume = "";
      if (muteVolume == false) {
        volume += map(currentVolume, minimumVolume, maximumVolume, 0, 100);
        volume += "%";
      } else {
        volume = "Muted";
      }
      displayIconDualMessage(1, (currentVolume >= 50 || muteVolume == true), false, 578, "Speakers", volume.c_str());
      displayState = 3;
    } else if (displayState != 4 && current_time >= 4 && current_time < 5) {
      String fan = "";
      fan += map(currentFan1Speed, 0, 255, 0, 100);
      fan += "% / ";
      fan += map(currentFan2Speed, 0, 255, 0, 100);
      fan += "%";
      displayIconDualMessage(1, (map(currentFan1Speed, 0, 255, 0, 100) >= 75 || map(currentFan2Speed, 0, 255, 0, 100) >= 75), false, 225, "Fans", fan.c_str());
      displayState = 4;
    } else if (displayState != 5 && current_time >= 5 && current_time < 6) {
      displayIconDualMessage(1, (currentMarqueeState == 1), false, 398, "Marquee", (currentMarqueeState == 1) ? "Enabled" : "Disabled");
      displayState = 5;
    } else if (displayState != 6 && current_time >= 6 && current_time < 7) {
      displayIconDualMessage(1, (displayMainState == false), false, (displayMainState == false) ? 250 : 129, "Display Source", (displayMainState == true) ? "PC" : "Game");
      displayState = 6;
    } else if (displayState != 7 && current_time >= 7 && current_time < 8) {
      displayIconDualMessage(1, (currentLEDState == 1), false, 398, "LED Control", (currentLEDState == 0) ? "MCU" : "Game");
      displayState = 7;
    } else if (displayState != 8 && current_time >= 8 && current_time < 9) {
      displayIconDualMessage(1, false, false, 141, "Game Disk", getGameSelect());
      displayState = 8;
    } else if (displayState != 9 && current_time >= 9 && current_time < 10) {
      displayIconDualMessage(1, (currentALLSLANMode0 == 0), false, 109, "ALLS Ethernet", ((currentALLSLANMode0 == 0) ? "VPN" : "LAN"));
      displayState = 9;
    } else if (displayState != 10 && current_time >= 10 && current_time < 11) {
      displayIconDualMessage(1, (coinEnable == true), false, 71, "Card Reader", (has_cr_talked == false) ? "No Data" : (coinEnable == true) ? "Enabled" : "Disabled");
      displayState = 10;
    } else if (current_time >= 11) {
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
      if (melodyPlay == 3) {
        const float position = ((millis() - previousShutdownMillis) / 60000);
        pauseBetweenNotes = map(position, 0, 4, loopMelody, 0) * 1000;
      } else {
        pauseBetweenNotes = loopMelody * 1000;
      }
    } else {
      startMelody = false;
      melodyPlay = -1;
    }
  }
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

String getGameSelect() {
  String assembledOutput = "";
  switch (requestedGameSelected0) {
    case -1:
      assembledOutput = "No Data";
      break;
    case 0:
      assembledOutput = "Reverse";
      break;
    case 1:
      assembledOutput = "Lilly";
      break;
    default:
      assembledOutput = "Other";
      break;
  }
  if (enhancedStandby == true) {
    assembledOutput += "*";
  }
  return assembledOutput;
}
String getPowerAuth() {
  String assembledOutput = "";
  assembledOutput += ((requestedPowerState0 != -1) ? "Warning" : ((currentPowerState0 == -2) ? ((enhancedStandby == true) ? "Eco Mode (E)" : "Eco Mode") : (currentPowerState0 == -1) ? ((enhancedStandby == true) ? "Power Off (E)" : "Power Off") : (currentPowerState0 == 0) ? ((enhancedStandby == true) ? "Standby (E)" : "Standby") : (coinEnable == false) ? "Startup" : "Active"));
  return assembledOutput;
}
void displayVolumeMessage() {
  int curVol = map(currentVolume, minimumVolume, maximumVolume, 0, 100);
  if (muteVolume == true) {
    messageIcon = 279;
    messageText = "Volume Muted";
    invertMessage = true;
  } else {
    messageIcon = 277;
    messageText = "Volume: ";
    messageText += String(curVol);
    messageText += "%";
    invertMessage = (curVol >= 40);
  }
  isJpnMessage = false;
  brightMessage = 255;
  timeoutMessage = 10;
  typeOfMessage = 1;
}

void defaultLEDState() {
  FastLED.setBrightness(0);
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
  delay(50);
  FastLED.show();
}
void standbyLEDState() {
  FastLED.setBrightness(64);
  int hue = 37;
  int rowCount = 7;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(map(hue, 0,360,0,255), 255, 255 - (rowCount * 10));
    rowCount--;
    if (rowCount <= -1) {
      rowCount = 7;
    }
  }
  FastLED.show();
  delay(50);
  FastLED.show();
}
void startingLEDState() {
  setLEDControl(true);
  targetBrightness = 255;
  numSteps = 4.0 * 33.2;
  transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  // int hue = 120;
  // int rowCount = 7;
  // for (int i = 0; i < NUM_LEDS; i++) {
  //   CHSV in = CHSV(map(hue, 0,360,0,255), 255, 255 - (map((rowCount * 10), 0, 100, 0, 255)));
  //   CRGB out;
  //   hsv2rgb_rainbow(in, out);
  //   leds_target[i] = out;
  //   rowCount--;
  //   if (rowCount <= -1) {
  //     rowCount = 7;
  //   }
  // }
  pending_release_leds = true;
  animation_state = -1;
  // animation_mode = 1;
  animation_mode = 2;
  currentStep = 0;
  currentPalette = RainbowColors_p;
  transition_leds = true;
}
void shuttingDownLEDState(int state) {
  kioskModeRequest("WarningGame");
  setLEDControl(true);
  targetBrightness = 255;
  numSteps = 4.0 * 33.2;
  transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  int hue = 0;
  int rowCount = 7;
  for (int i = 0; i < NUM_LEDS; i++) {
    CHSV in = CHSV(map(hue, 0,360,0,255), 255, 255 - (map((rowCount * 10), 0, 100, 0, 255)));
    CRGB out;
    hsv2rgb_rainbow(in, out);
    leds_target[i] = out;
    rowCount--;
    if (rowCount <= -1) {
      rowCount = 7;
    }
  }
  previousShutdownMillis = millis();
  requestedPowerState0 = state;
  pending_release_leds = false;
  animation_state = -1;
  animation_mode = 1;
  currentStep = 0;
  transition_leds = true;

  melodyPlay = 3;
  loopMelody = 2;
  currentNote = 0;
  previousMelodyMillis = 0;
  startMelody = true;

  messageIcon = (requestedPowerState0 == -1) ? 96 : 223;
  messageText = (requestedPowerState0 == -1) ? "Power Off" : "Standby";
  isJpnMessage = false;
  brightMessage = 255;
  invertMessage = true;
  timeoutMessage = 25;
  typeOfMessage = 1;
}
void startLoadingScreen() {
  pending_release_display = true;
  kioskModeRequest("StartGame");
}

void handleSetLeds(String ledValues, int bankSelect, bool should_transition) {
  Serial.println("Received LED values: " + ledValues);
  int index = 0;
  while (ledValues.length() > 0 && index < NUM_LEDS) {
    String hexValue = ledValues.substring(0, ledValues.indexOf(' '));
    ledValues = ledValues.substring(ledValues.indexOf(' ') + 1);
    uint32_t color = (uint32_t)strtol(hexValue.c_str(), NULL, 16);

    if (index < NUM_LEDS) {
      if (bankSelect == 0) {
        if (should_transition == true) {
          leds_target[index] = color;
        } else {
          leds[index] = color;
        }
      }
    } else {
      Serial.println("Received LED Value Index OUT OF RANGE OF CONTROLLER: " + index);
    }
    index++;
  }
  if (should_transition == true) {
    bool all_same = true;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (leds_source[i].r != leds_target[i].r && leds_source[i].g != leds_target[i].g && leds_source[i].b != leds_target[i].b) {
        all_same = false;
      }
    }
    if (all_same == false) {
      transition_leds = true;
    } else {
      FastLED.show();
      delay(33);
      FastLED.show();
    }
  } else {
    FastLED.show();
    delay(33);
    FastLED.show();
  }
}
void handleSetLedColor(String ledValue, int bankSelect, bool should_transition) {
  Serial.println("Received LED value: " + ledValue);
  uint32_t color = (uint32_t)strtol(ledValue.c_str(), NULL, 16);
  int index = 0;

  if (bankSelect == 0) {
    for (int i = 0; i < NUM_LEDS; i++) {
      if (should_transition == true) {
        leds_target[i] = color;
      } else {
        leds[i] = color;
      }
    }
  }
  if (should_transition == true) {
    bool all_same = true;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (leds_source[i].r != leds_target[i].r && leds_source[i].g != leds_target[i].g && leds_source[i].b != leds_target[i].b) {
        all_same = false;
      }
    }
    if (all_same == false) {
      transition_leds = true;
    } else {
      FastLED.show();
      delay(33);
      FastLED.show();
    }
  } else {
    FastLED.show();
    delay(133);
    FastLED.show();
  }
}
bool ignore_next_state = false;
void handleCRMessage(String inputString) {
  int delimiterIndex = inputString.indexOf("::");
  if (delimiterIndex != -1) {
    int headerIndex = inputString.indexOf("::");
    String header = inputString.substring(0, headerIndex);
    if (header == "COIN_ENABLE") {
      has_cr_talked = true;
      ignore_next_state = false;
      int valueIndex = inputString.indexOf("::", headerIndex + 2);
      String valueString = inputString.substring(headerIndex + 2, valueIndex);
      int responseCode = valueString.toInt();
      switch (responseCode) {
        case 0:
          coinEnable = false;
          break;
        case 1:
          coinEnable = true;
          break;
        default:
          break;
      }
    } else if (header == "COIN_DISPENSE") {
      has_cr_talked = true;
      int valueIndex = inputString.indexOf("::", headerIndex + 2);
      String valueString = inputString.substring(headerIndex + 2, valueIndex);
      int responseCode = valueString.toInt();
      int dataIndex = inputString.indexOf("::", valueIndex + 2);
      String dataString = inputString.substring(valueIndex + 2, dataIndex);

      Serial.println(valueString);
      Serial.println(dataString);
      if (ignore_next_state == false) {
        ignore_next_state = true;
        if (responseCode >= 200 && responseCode < 300) {
          resetInactivityTimer();
        } else if (valueString == "FALSE") {
        }
      }
    } else if (header == "BOOT_REQUEST") {
      has_cr_talked = true;
      if (ignore_next_state == false) {
        ignore_next_state = true;
        setGameOn();
      }
    }
  }
}

void setMasterPowerOn() {
  if (currentPowerState0 <= -1) {
    if (currentPowerState0 == -2 && requestPowerMgrOn == false) {
      powerOnManager(false);
    }
    setIOPower(true);
    delay(250);
    setLEDControl(true);
    kioskModeRequest("StartStandby");
    resetMarqueeState();
    setChassisFanSpeed(35);
    setMainFanSpeed((enhancedStandby == true) ? 100 : 25);
    delay(500);
    defaultLEDState();
    standbyLEDState();
    setDisplayState(true);
    if (enhancedStandby == true) {
      enterEnhancedStandby(true);
    } else {
      setSysBoardPower(false);
    }
    delay(2000);
    setDisplayState(true);
    currentPowerState0 = 0;

    pending_release_leds = false;
    pending_release_display = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    currentStep = 0;

    messageIcon = 223;
    messageText = (enhancedStandby == true) ? "E. Standby Mode" : "Standby Mode";
    isJpnMessage = false;
    brightMessage = 255;
    invertMessage = false;
    timeoutMessage = 10;
    typeOfMessage = 1;
  }
  keepManagerAwake = false;
  powerInactivityMillis = -1;
  requestedPowerState0 = -1;
}
void setMasterPowerOff() {
  kioskModeRequest("StopAll");
  setChassisFanSpeed(0);
  setMainFanSpeed((enhancedStandby == true) ? 75 : 15);
  setLEDControl(true);
  setMarqueeState(false, false);
  if (currentPowerState0 == 1) {
    if (enhancedStandby == true) {
      enterEnhancedStandby(true);
    } else {
      delay(500);
      setSysBoardPower(false);
      delay(200);
    }
    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
  } else {
    melodyPlay = -1;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = false;
  }
  delay(500);
  setIOPower(false);
  setDisplayState(false);
  if (currentPowerState0 != -1) {
    messageIcon = 96;
    messageText = "Power Off";
    isJpnMessage = false;
    brightMessage = 1;
    invertMessage = true;
    timeoutMessage = 10;
    typeOfMessage = 1;
  }
  keepManagerAwake = false;
  powerInactivityMillis = millis();
  requestedPowerState0 = -1;
  currentPowerState0 = -1;
}
void setGameOn() {
  if (currentPowerState0 <= -1) {
    if (currentPowerState0 == -2) {
      requestPowerMgrOn = true;
    }
    setIOPower(true);
    delay(1000);
  }
  if (currentPowerState0 != 1) {
    inactivityMinTimeout = defaultInactivityMinTimeout + 5;
    previousInactivityMillis = millis();
    if (requestPowerMgrOn == true) {
      setDisplayState(false);
    } else {
      startLoadingScreen();
    }
    setChassisFanSpeed(75);
    setMainFanSpeed(100);
    startingLEDState();
    setMarqueeState(true, false);
    if (enhancedStandby == true) {
      delay(15000);
      exitEnhancedStandby();
    }
    if (requestedGameSelected0 != currentGameSelected0) {
      setGameDisk(requestedGameSelected0);
    }
    setDisplayState(true);
    setSysBoardPower(true);
    delay(2000);
    setDisplayState(true);

    melodyPlay = 0;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;

    pending_release_display = true;

    messageIcon = 96;
    messageText = "Power On";
    isJpnMessage = false;
    brightMessage = 255;
    invertMessage = true;
    timeoutMessage = 10;
    typeOfMessage = 1;
  }
  if (requestedPowerState0 != -1) {
    resetState();
  }
  keepManagerAwake = false;
  powerInactivityMillis = -1;
  requestedPowerState0 = -1;
  currentPowerState0 = 1;
}
void setGameOff() {
  if (currentPowerState0 == 1) {
    currentPowerState0 = 0;

    pending_release_leds = false;
    pending_release_display = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    currentStep = 0;

    setDisplayState(true);
    setChassisFanSpeed(35);
    setMainFanSpeed((enhancedStandby == true) ? 100 : 25);
    kioskModeRequest("StartStandby");
    setLEDControl(true);
    delay(250);
    standbyLEDState();
    resetMarqueeState();
    if (enhancedStandby == true) {
      enterEnhancedStandby(true);
    } else {
      setSysBoardPower(false);
    }
    delay(2000);
    setDisplayState(true);

    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;

    messageIcon = 223;
    messageText = "Standby Mode";
    isJpnMessage = false;
    brightMessage = 255;
    invertMessage = false;
    timeoutMessage = 10;
    typeOfMessage = 1;
  }
  keepManagerAwake = false;
  powerInactivityMillis = -1;
  currentPowerState0 = 0;
  requestedPowerState0 = -1;
}
void powerOnManager(bool beeper_disable) {
  if (ultraPowerSaving == true && shutdownPCTimer != 0) {
    powerInactivityMillis = -1;
    if (!beeper_disable) {
      tone(buzzer_pin, NOTE_CS3, 1000 / 8);
    }
    int tryCount = 0;
    lockedState = false;
    while (shutdownPCTimer != 0) {
      Serial.println("");
      Serial.println("PING");
      Serial.println("");
      WOL.sendMagicPacket(KLM_MACAddress);
      delay(1000);
      if (!beeper_disable) {
        tone(buzzer_pin, (tryCount % 2 == 0) ? NOTE_GS3 : NOTE_CS3, 1000 / 8);
      }
      tryCount++;
    }
    delay(500);
    Serial.println("");
    Serial.println("PROBE::SEARCH");
    Serial.println("");
    if (!beeper_disable) {
      noTone(buzzer_pin);
    }
  }
}
void requestPowerOff() {
  if (currentPowerState0 == 1 && coinEnable == false) {
    setMasterPowerOff();
  } else if (currentPowerState0 == 1 && coinEnable == true) {
    shuttingDownLEDState(0);
  } else if (currentPowerState0 == 0) {
    setMasterPowerOff();
  }
}
void requestStandby() {
  if (currentPowerState0 == 1) {
    setGameOff();
  } else {
    setMasterPowerOn();
  }
}

void enterEnhancedStandby(bool state) {
  inhibitNuState = false;
  if (state == true) {
    digitalWrite(relayPins[3], HIGH);
    digitalWrite(relayPins[4], LOW);
    nuResponse = "";
    int i = 0;
    while (currentGameSelected0 != 2 && i < 10) {
      nuControl.println("DS::2");
      delay(125);
      i++;
    }
    i = 0;
    if (currentNuPowerState0 == 0) {
      nuResponse = "";
      while (currentNuPowerState0 == 0 && i < 10) {
        nuControl.println("PS::1::");
        delay(125);
        i++;
      }
    }
  } else if (state == false) {
    if (currentNuPowerState0 == 1) {
      exitEnhancedStandby();
      int i = 0;
      nuResponse = "";
      while (currentNuPowerState0 == 1 && i < 10) {
        nuControl.println("PS::0::");
        delay(125);
        i++;
      }
    }
    digitalWrite(relayPins[3], LOW);
    digitalWrite(relayPins[4], LOW);
  }
}
void exitEnhancedStandby() {
  tone(buzzer_pin, NOTE_C5);
  HTTPClient http;
  String url = String(shutdown_url);
  Serial.println("Sending GET request to: " + url);
  http.begin(url);
  http.setConnectTimeout(5000);
  http.setTimeout(120000); // Wait 120 Sec for complete Shutdown
  int httpCode = http.GET();
  String payload = http.getString();
  Serial.println(payload);
  http.end();
  Serial.println("HTTP Response code: " + String(httpCode));
  noTone(buzzer_pin);
  delay(5);
}
void setSysBoardPower(bool state) {
  nuResponse = "";
  digitalWrite(relayPins[3], (state == true) ? HIGH : LOW);
  while (currentNuPowerState0 == ((state == true) ? 0 : 1)) {
    nuControl.print("PS::");
    nuControl.println(((state == true) ? "1" : "0"));
    delay(100);
  }
  digitalWrite(relayPins[4], (currentGameSelected0 < 2) ? ((state == true) ? HIGH : LOW) : LOW);
  delay((state == true) ? 200 : 500);
}
void setLEDControl(bool state) {
  digitalWrite(relayPins[1], (state == true) ? LOW : HIGH);
  int last_state = currentLEDState;
  currentLEDState = (state == true) ? 0 : 1;
  if (last_state == false && state == true) {
    delay(500);
  }
}
void setIOPower(bool state) {
  digitalWrite(relayPins[0], (state == true) ? HIGH : LOW);
  delay(750);
  digitalWrite(relayPins[2], (state == true) ? HIGH : LOW);
}
void resetPSU() {
  digitalWrite(relayPins[4], LOW);
  delay(2000);
  digitalWrite(relayPins[4], HIGH);
}
void resetMarqueeState() {
  digitalWrite(relayPins[5], (currentMarqueeState == 1) ? HIGH : LOW);
}
void setMarqueeState(bool state, bool save) {
  if (currentPowerState0 == 0) {
    digitalWrite(relayPins[5], (state == true) ? HIGH : LOW);
  }
  if (save == true) {
    currentMarqueeState = (state == true) ? 1 : 0;
  }
}
void setChassisFanSpeed(int speed) {
  currentFan1Speed = map(speed, 0, 100, 0, 255);
  analogWrite(fanPWM1, currentFan1Speed);
}
void setMainFanSpeed(int speed) {
  currentFan2Speed = map(speed, 0, 100, 0, 255);
  analogWrite(fanPWM2, currentFan2Speed);
}
void setGameDisk(int number) {
  inhibitNuState = false;
  nuResponse = "";
  while (currentGameSelected0 != number) {
    nuControl.print("DS::");
    nuControl.println(String(number));
    delay(100);
  }
  if (currentPowerState0 == 1) {
      setSysBoardPower(true);
      setDisplayState(false);
    if (number < 2) {
      resetPSU();
      startingLEDState();
      //startLoadingScreen();
    }
  }
  messageIcon = 129;
  messageText = "HDD: ";
  switch (number) {
    case 0:
      messageText += "Reverse";
      break;
    case 1:
      messageText += "Lilly";
      break;
    case 2:
      messageText += "Enhanced STB";
      break;
    default:
      messageText += "???";
      break;
  }
  invertMessage = (currentPowerState0 == 1);
  isJpnMessage = false;
  brightMessage = 255;
  timeoutMessage = 10;
  typeOfMessage = 1;
}
void setDisplayState(bool state) {
  if (state != displayMainState) {
    pushDisplaySwitch();
  }
}
void resetState() {
  setLEDControl((currentPowerState0 != 1));
  if (currentLEDState == 0) {
    delay(800);
    setLEDControl(false);
  }
  setDisplayState((currentGameSelected0 >= 2 || currentPowerState0 != 1));
  kioskModeRequest("ResetState");
  startMelody = false;
  melodyPlay = -1;
  loopMelody = -1;
  currentNote = 0;
  previousMelodyMillis = 0;

  pending_release_leds = false;
  pending_release_display = false;
  animation_state = -1;
  animation_mode = -1;
  currentStep = 0;
  requestedPowerState0 = -1;
  previousInactivityMillis = millis();
  inactivityMinTimeout = defaultInactivityMinTimeout;
}
void resetInactivityTimer() {
  resetState();
  previousInactivityMillis = millis();
  inactivityMinTimeout = defaultInactivityMinTimeout + 20;
}

void pushDisplaySwitch() {
  digitalWrite(displayMainSelect, HIGH);
  digitalWrite(displayMainSelect, LOW);
  delay(500);
  digitalWrite(displayMainSelect, HIGH);
}

void kioskModeRequest(String command) {
  Serial.println("");
  Serial.println("ACTION::" + command);
  Serial.println("");
}
void kioskTest() {
  static String receivedMessage = "";
  char c;
  bool messageStarted = false;


  while (ready_to_boot == false) {
    while (Serial.available()) {
      c = Serial.read();
      if (c == '\n') {
        if (!receivedMessage.isEmpty()) {
          int delimiterIndex = receivedMessage.indexOf("::");
          if (delimiterIndex != -1) {
            int headerIndex = receivedMessage.indexOf("::");
            String header = receivedMessage.substring(0, headerIndex);
            if (header == "PROBE") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "HELLO") {
                int softwareIndex = receivedMessage.indexOf("::", valueIndex + 2);
                attachedSoftwareCU = receivedMessage.substring(valueIndex + 2, softwareIndex);
                ready_to_boot = true;
              }
            }
          }
        }
        receivedMessage = "";
      } else {
        receivedMessage += c;
      }
    }
    delay(1);
  }
  if(Task3 != NULL) {
    vTaskDelete(Task3);
  }
}
void testCardReader() {
  static String receivedMessage = "";
  char c;
  bool card_reader_txrx_ok = false;


  while (card_reader_txrx_ok == false) {
    while (cardReaderSerial.available()) {
      c = cardReaderSerial.read();
      if (c == '\n') {
        if (!receivedMessage.isEmpty()) {
          int delimiterIndex = receivedMessage.indexOf("::");
          if (delimiterIndex != -1) {
            int headerIndex = receivedMessage.indexOf("::");
            String header = receivedMessage.substring(0, headerIndex);
            if (header == "HELLO") {
              card_reader_txrx_ok = true;
            }
          }
        }
        receivedMessage = "";
      } else {
        receivedMessage += c;
      }
    }
    delay(1);
  }
}
void kioskCommand() {
  static String receivedMessage = "";
  char c;
  bool messageStarted = false;

  for(;;) {
    while (Serial.available()) {
      c = Serial.read();
      if (c == '\n') {
        if (!receivedMessage.isEmpty()) {
          int delimiterIndex = receivedMessage.indexOf("::");
          if (delimiterIndex != -1) {
            int headerIndex = receivedMessage.indexOf("::");
            String header = receivedMessage.substring(0, headerIndex);
            if (header == "POWER_SWITCH") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "OFF") {
                setMasterPowerOff();
              } else if (valueString == "STANDBY") {
                if (currentPowerState0 == 1) {
                  setGameOff();
                } else if (currentPowerState0 == -1 || (currentPowerState0 == 0 && currentNuPowerState0 != ((enhancedStandby == true) ? 1 : 0))) {
                  setMasterPowerOn();
                }
              } else if (valueString == "ON") {
                setGameOn();
              } else if (valueString == "?") {
                Serial.println("R::" + getPowerAuth());
              }
            } else if (header == "POWER_REQUEST") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "OFF") {
                requestPowerOff();
              } else if (valueString == "STANDBY_NA") {
                if (currentPowerState0 == 1) {
                  setGameOn();
                } else if (currentPowerState0 == -1) {
                  setMasterPowerOn();
                }
              } else if (valueString == "STANDBY") {
                requestStandby();
              } else if (valueString == "ON") {
                setGameOn();
              } else if (valueString == "?") {
                Serial.println("R::" + String((requestedPowerState0 == -1) ? "Power Off" : (requestedPowerState0 == 0) ? "Standby" : "None"));
              }
            } else if (header == "ENHANCED_STANDBY") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "DISABLE") {
                enhancedStandby = false;
                if (currentPowerState0 != 1) {
                  enterEnhancedStandby(false);
                }
              } else if (valueString == "ENABLE") {
                enhancedStandby = true;
                if (currentPowerState0 != 1) {
                  enterEnhancedStandby(true);
                }
              } else if (valueString == "?") {
                Serial.println("R::" + String((enhancedStandby == true) ? "Enable" : "Disabled"));
              }
            } else if (header == "INACTIVITY_TIMER") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "RESET") {
                resetInactivityTimer();
              } else if (valueString == "SET") {
                int timeIndex = receivedMessage.indexOf("::", valueIndex + 2);
                String timeString = receivedMessage.substring(valueIndex + 2, timeIndex);
                int timeInt = timeString.toInt();
                defaultInactivityMinTimeout = timeInt;
                resetInactivityTimer();
              } else if (valueString == "?") {
                Serial.println("R::" + String((currentPowerState0 == 1 && inactivityTimeout == true) ? String(inactivityMinTimeout - ((millis() - previousInactivityMillis) / 1000)) : "0"));
              }
            } else if (header == "SHUTDOWN_TIMER") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "?") {
                Serial.println("R::" + String((currentPowerState0 == 1) ? String(shutdownDelayMinTimeout - ((millis() - previousShutdownMillis) / 1000)) : "0"));
              }
            } else if (header == "DISK_SELECT") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "?") {
                Serial.println("R::" + getGameSelect());
              } else {
                int valueInt = valueString.toInt();
                requestedGameSelected0 = valueInt;
                if (enhancedStandby == false || currentPowerState0 == 1) {
                  setGameDisk(valueInt);
                }
              }
            } else if (header == "STATE") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "RESET") {
                resetState();
              }
            } else if (header == "VOLUME") {
              int optionIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String optionString = receivedMessage.substring(headerIndex + 2, optionIndex);
              if (optionString == "?") {
                Serial.println("R::" + ((muteVolume == true) ? "Muted" : String(map(currentVolume, minimumVolume, maximumVolume, 0, 100))));
              } else if (optionString == "SET") {
                int valueIndex = receivedMessage.indexOf("::", optionIndex + 2);
                String valueString = receivedMessage.substring(optionIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                if (valueInt >= 0 && valueInt <= 100) {
                  currentVolume = map(valueInt, 0, 100, minimumVolume, maximumVolume);
                  muteVolume = false;
                  ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
                  displayVolumeMessage();
                }
              } else if (optionString == "UP") {
                int valueIndex = receivedMessage.indexOf("::", optionIndex + 2);
                String valueString = receivedMessage.substring(optionIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                int current_percent = map(currentVolume, minimumVolume, maximumVolume, 0, 100);
                current_percent += valueInt;
                currentVolume = map(current_percent, 0, 100, minimumVolume, maximumVolume);
                muteVolume = false;
                ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
                displayVolumeMessage();
              } else if (optionString == "DOWN") {
                int valueIndex = receivedMessage.indexOf("::", optionIndex + 2);
                String valueString = receivedMessage.substring(optionIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                int current_percent = map(currentVolume, minimumVolume, maximumVolume, 0, 100);
                current_percent -= valueInt;
                currentVolume = map(current_percent, 0, 100, minimumVolume, maximumVolume);
                muteVolume = false;
                ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
                displayVolumeMessage();
              } else if (optionString == "MUTE_ON") {
                muteVolume = true;
                ds3502.setWiper(minimumVolume);
                displayVolumeMessage();
              } else if (optionString == "MUTE_OFF") {
                muteVolume = false;
                ds3502.setWiper(currentVolume);
                displayVolumeMessage();
              } else if (optionString == "MUTE") {
                muteVolume = !(muteVolume);
                ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
                displayVolumeMessage();
              }
            } else if (header == "DLPM") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "CANCEL") {
                shutdownPCTimer = 0;
              } else if (valueString == "REQ_POWER_OFF") {
                shutdownPCTimer = 2;
              } else if (valueString == "REQ_REBOOT") {
                shutdownPCTimer = 1;
              }
            } else if (header == "PING") {
              Serial.println("R::PONG");
              lastPingMillis = millis();
              if (shutdownPCTimer != 0 && lockedState == false) {
                shutdownPCTimer = 0;
                powerInactivityMillis = -1;
              }
            }
          }
        }
        receivedMessage = "";
      } else {
        receivedMessage += c;
      }
    }
    delay(1);
  }
}
