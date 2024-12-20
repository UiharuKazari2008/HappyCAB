#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Adafruit_DS3502.h>
#include <Adafruit_NeoPixel.h>
#include <FastLED.h>
#include <SoftwareSerial.h>
#include <IRremote.hpp>
#include <HTTPClient.h>
#include "melody.h"
#include <WiFiUdp.h>
#include <WakeOnLan.h>

const char* ssid = "Radio Noise AX";
const char* password = "Radio Noise AX";
// Lifecycle Manager PC Mac Address
const char* KLM_MACAddress = "3C:52:82:58:B4:71";
WebServer server(80);
WiFiUDP UDP;
WakeOnLan WOL(UDP);

// ETHERNET PHY DIGITAL SWITCH
// WACCA "0" - PURPLE RED ZIP TIE (Sensor) WHITE (SELECT)
// CHUNI "1" - PURPLE BLUE ZIP TIE (Sensor) WHITE (SELECT)
const int numOfethPorts = 2;
const int ethSensors[2] = { 35, 34 };
const int ethButtons[2] = { 18, 19 };
// Nu CONTROL PORT (KEYCHIP REPLACEMENT)
// UART Serial to Arduino Nano
EspSoftwareSerial::UART nuControl;
const int nuControlTX = 15; // WHITE RED
const int nuControlRX = 5; // WHITE GREEN
String nuResponse = "";
// Power Controls
// 0 - PSU and Monitor Enable
// 1 - Nu Power Enable
// 2 - Marquee Enable
// 3 - LED Select
// 4 - Slider RS232
const int numberRelays = 5;
const int controlRelays[5] = { 12, 14, 27, 26, 25 };
// Fan Controller
const int fanPWM = 13;
// Card Reader Communication
HardwareSerial cardReaderSerial(2);
bool coinEnable = false;
bool allsOK = false;
bool has_cr_talked = false;
// Display Switch
// 23 - Select
// 15 - State
const int displayMainSelect = 23;
const int displayMainLDR = 36;
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
bool pending_alls_good_response = false;
bool transition_leds = false;
float transition_interval = 0;
int animation_mode = -1;
int animation_state = -1;
// Beeper Tones
int buzzer_pin = 4;
unsigned long previousMelodyMillis = 0;
int currentNote = 0;
int melodyPlay = -1;
bool startMelody = false;
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
int warning_tone_long[] = {
  NOTE_A5, NOTE_F5
};
int warning_tone_long_dur[] = {
  4, 4
};
int boot_tone[] = {
  NOTE_F5, NOTE_G5, NOTE_A5, NOTE_B5
};
int boot_tone_dur[] = {
  8, 8, 8, 4
};
// IR Recever
const int irRecPin = 2;
// Occupancy and Timer
int requestedPowerState0 = -1;
int defaultInactivityMinTimeout = 45;
int inactivityMinTimeout = 45;
int defaultPowerOffDelayMinTimeout = 35;
int powerOffDelayMinTimeout = 35;
int shutdownDelayMinTimeout = 3;
unsigned long previousInactivityMillis = 0;
unsigned long previousShutdownMillis = 0;
bool inactivityTimeout = true;

const int nuPostCooldownMinTimeout = 5;
const int allsPostCooldownMinTimeout = 10;
int previousCooldownMillis = 0;
int activeCooldownTimer = -1;

// DISPLAY_MESSAGE::BIG::icon::text::isJP/t::255::invert/t::timeout/20
int typeOfMessage = -1;
int messageIcon = 0;
String messageText = "";
bool isJpnMessage = false;
int brightMessage = 1;
bool invertMessage = false;
int timeoutMessage = 0;

bool lockedState = false;
bool requestPowerMgrOn = false;
bool keepManagerAwake = false;
bool ready_to_boot = false;
String inputString = "";
String attachedSoftwareCU = "Unknown";
int currentVolume = 0;
bool muteVolume = false;
int minimumVolume = 20;
int maximumVolume = 127;
bool inhibitNuState = false;
bool ultraPowerSaving = true;
int shutdownPCTimer = 0;
unsigned long powerInactivityMillis = 0;
unsigned long lastPingMillis = 0;
int currentGameSelected0 = -1;
int currentNuPowerState0 = -1;
int currentALLSState0 = -1;
int currentPowerState0 = -1;
int currentLEDState = 0;
int currentSliderState = 0;
int currentMarqueeState = 1;
int currentFanSpeed = 128;
int displayedSec = 0;
int refresh_time = 0;
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
TaskHandle_t Task11;

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
      //if (tryCount > 60 && currentPowerState0 != 1) {
      //  ESP.restart();
      //}
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
  inputString.reserve(200);
  attachedSoftwareCU.reserve(200);
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  WOL.setRepeat(3, 100);
  bootScreen("HARDWARE");
  pinMode(fanPWM, OUTPUT);
  pinMode(displayMainLDR, INPUT);
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
  IrReceiver.begin(irRecPin);
  tone(buzzer_pin, NOTE_C6);

  bootScreen("NU_CTRL_COM");
  nuControl.begin(9600, SWSERIAL_8N1, nuControlRX, nuControlTX, false);
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
  digitalWrite(controlRelays[1], HIGH);
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
  digitalWrite(controlRelays[1], LOW);
  for (int i = 0; i < 3; i++) {
    nuControl.println("PS::0");
    delay(100);
  }

  noTone(buzzer_pin);
  bootScreen("DEFAULTS");
  currentVolume = map(25, 0, 100, minimumVolume, maximumVolume);
  ds3502.setWiper(currentVolume);
  ds3502.setWiperDefault(currentVolume);
  displayMainState = (digitalRead(displayMainLDR) == LOW);
  kioskModeRequest("StopAll");
  Serial.println("LED_DATA::16::0::000000::");
  setChassisFanSpeed(40);
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
  server.on("/fan/set", [=]() {
    String response = "UNCHANGED";
    if (activeCooldownTimer >= 0) {
      response = "INHIBIT: COOLING DOWN";
    } else {
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
    }
    server.send(200, "text/plain", response);
  });
  server.on("/fan/reset", [=]() {
    String response = "OK";
    if (activeCooldownTimer >= 0) {
      response = "INHIBIT: COOLING DOWN";
    } else {
      if (currentPowerState0 == 0) {
        setChassisFanSpeed(50);
      } else if (currentPowerState0 == -2) {
        setChassisFanSpeed(0);
      } else if (currentPowerState0 == -1) {
        setChassisFanSpeed(30);
      } else if (currentPowerState0 == 1) {
        setChassisFanSpeed((currentGameSelected0 < 10) ? 75 : 100);
      }
    }
    server.send(200, "text/plain", response);
  });

  server.on("/fan", [=]() {
    String response = "";
    response += map(currentFanSpeed, 0, 255, 0, 100);
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
    assembledOutput += ((displayMainState == false) ? "PC" : "AUX");
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/display", [=]() {
    String assembledOutput = "";
    assembledOutput += ((displayMainState == true) ? "PC" : "GAME");
    server.send(200, "text/plain", assembledOutput);
  });

  server.on("/network/set/all", [=]() {
    for (int i=0; i < server.args(); i++) {
      if (server.argName(i) == "ethNum") {
        const String ethVal = server.arg(i);
        for (int o=0; o < numOfethPorts; o++) {
          if (digitalRead(ethSensors[o]) == (ethVal[o] == '1' ? LOW : HIGH)) {
            if (o == 1 && currentPowerState0 == 1) {
              nuControl.println("PS::128");
              if (pending_release_display == false) {
                startLoadingScreen();
              }
            }
            pushEthSwitch(o);
          }
        }
      }
    }
    server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
  });
  server.on("/network/set", [=]() {
    String assembledOutput = "";
    int ethNum = 0;
    int ethVal = 0;
    for (int i=0; i < server.args(); i++) {
      if (server.argName(i) == "cabNum")
        ethNum = server.arg(i).toInt();
      else if (server.argName(i) == "ethNum")
        ethVal = server.arg(i).toInt();
    }
    if (digitalRead(ethSensors[ethNum]) == ((ethVal == 1) ? LOW : HIGH)) {
      setEthernetState(ethNum, ethVal);
      server.send(200, "text/plain", (ethNum == 0 && currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/network", [=]() {
    String assembledOutput = "";
    int ethNum = 0;
    for (int i=0; i < server.args(); i++) {
      if (server.argName(i) == "cabNum") {
        ethNum = server.arg(i).toInt();
        if (ethNum == 0 && currentGameSelected0 >= 10) {
          assembledOutput += "Private";
        } else {
          String const val = getEthSwitchVal(ethNum);
          assembledOutput += val;
        }
      }
    }
    server.send(200, "text/plain", assembledOutput);
  });

  server.on("/select/game/omni", [=]() {
    if (currentGameSelected0 != 0) {
      setGameDisk(0);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/base", [=]() {
    if (currentGameSelected0 != 1) {
      setGameDisk(1);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/air", [=]() {
    if (currentGameSelected0 != 2) {
      setGameDisk(2);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/star", [=]() {
    if (currentGameSelected0 != 3) {
      setGameDisk(3);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/crystal", [=]() {
    if (currentGameSelected0 != 4) {
      setGameDisk(4);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/other", [=]() {
    if (currentGameSelected0 != 5) {
      setGameDisk(5);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/pdft", [=]() {
    if (currentGameSelected0 != 20) {
      setGameDisk(20);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/sun", [=]() {
    if (currentGameSelected0 != 10) {
      setGameDisk(10);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/sun_extra", [=]() {
    if (currentGameSelected0 != 12) {
      setGameDisk(12);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/new", [=]() {
    if (currentGameSelected0 != 11) {
      setGameDisk(11);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/lumi", [=]() {
    if (currentGameSelected0 != 14) {
      setGameDisk(14);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/lumi_extra", [=]() {
    if (currentGameSelected0 != 13) {
      setGameDisk(13);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/lumi_plus_extra", [=]() {
    if (currentGameSelected0 != 15) {
      setGameDisk(15);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game", [=]() {
    String const val = getGameSelect();
    server.send(200, "text/plain", val);
  });
  server.on("/select/game_id", [=]() {
    String const val = String(currentGameSelected0);
    server.send(200, "text/plain", val);
  });

  server.on("/sys_board", [=]() {
    server.send(200, "text/plain", (currentGameSelected0 >= 10) ? "ALLS MX2" : "Nu 1.1");
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
    } else if (currentPowerState0 <= -1) {
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
    if (currentPowerState0 == 1 && (coinEnable == true || currentGameSelected0 > 19)) {
      if (server.hasArg("nonauthoritive")) {
      } else {
        shuttingDownLEDState(1);
      }
    } else if (currentPowerState0 == 1 && coinEnable == false) {
      if (server.hasArg("nonauthoritive")) {
      } else {
        setGameOff();
      }
    } else if (currentPowerState0 <= -1) {
      setMasterPowerOn();
    }
  });
  server.on("/request/off", [=]() {
    server.send(200, "text/plain", "OK");
    requestPowerOff();
  });

  server.on("/test/nu/off", [=]() {
    nuResponse = "";
    digitalWrite(controlRelays[1], LOW);
    while (currentNuPowerState0 == 1) {
      nuControl.println("PS::0");
      delay(100);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/nu/on", [=]() {
    nuResponse = "";
    digitalWrite(controlRelays[1], HIGH);
    while (currentNuPowerState0 == 0) {
      nuControl.println("PS::1");
      delay(100);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/nu/reset", [=]() {
    nuResponse = "";
    while (nuResponse == "") {
      nuControl.println("PS::128");
      delay(100);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/sysbrd/off", [=]() {
    //setDisplayState(true);
    setSysBoardPower(false);
    setTouchControl(false);
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/sysbrd/on", [=]() {
    //setDisplayState(false);
    setSysBoardPower(true);
    setTouchControl((currentGameSelected0 < 10));
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/sysbrd/reset", [=]() {
    //setDisplayState(false);
    setSysBoardPower(true);
    setTouchControl(currentGameSelected0 < 10);
    if (currentGameSelected0 >= 10) {
      ALLSCtrl("PS", "128");
    } else {
      nuResponse = "";
      while (nuResponse == "") {
        nuControl.println("PS::128");
        delay(100);
      }
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
  server.on("/select_power", [=]() {
    server.send(200, "text/plain", (requestedPowerState0 >= 0) ? "Warning" : ((currentPowerState0 == 1) ? getGameSelect() : "Disabled"));
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
    int bankSelect = 0;
    if (server.hasArg("bankSelect")) {
      bankSelect = server.arg("bankSelect").toInt();
    }
    if ((pending_release_leds == false && currentPowerState0 != 1) || server.hasArg("takeOwnership")) {
      if (bankSelect == 10) {
        String req = "LED_DATA::";
        if (server.hasArg("ledBrightness")) {
          req += server.arg("ledBrightness");
        } else {
          req += "64";
        }
        req += "::";
        if (server.hasArg("transition_time")) {
          req += server.hasArg("transition_time");
        } else {
          req += "0";
        }
        req += "::";
        req += ledValues;
        req += "::";

        Serial.println(req);
      } else {
        int brightness = 64;
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

        handleSetLeds(ledValues, bankSelect, (server.hasArg("transition_time")));
      }
    }
  });
  server.on("/setLEDColor", [=]() {
    String ledValue = server.arg("ledColor");  // Get the LED values from the URL parameter
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Data Sent");
    if (pending_release_leds == false && (currentPowerState0 != 1 || currentGameSelected0 >= 10)) {
      int brightness = 64;
      int bankSelect = 0;
      brightness = server.arg("ledBrightness").toInt();
      if (server.hasArg("takeOwnership")) {
        setLEDControl((server.arg("ledValues").c_str() == "true"));
      }
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
  bootScreen("TASK_RA");
  xTaskCreatePinnedToCore(
                    remoteAccessLoop,   /* Task function. */
                    "raTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    20,           /* priority of the task */
                    &Task8,      /* Task handle to keep track of created task */
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
  bootScreen("TASK_IR");
  xTaskCreatePinnedToCore(
                    irRemoteLoop,   /* Task function. */
                    "irRemoteTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    20,           /* priority of the task */
                    &Task9,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  delay(101);
  displayedSec = esp_timer_get_time() / 1000000;
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

void loop() {
  unsigned long currentMillis = millis();
  // Handle ALLS OK Card Reader Redirect
  if (pending_alls_good_response == true && allsOK == true && coinEnable == false) {
    cardReaderSerial.println("BLOCKER_OPEN::NO_DATA");
    delay(100);
  }
  // Handle LED Handover
  if (pending_release_leds == true && coinEnable == true) {
    pending_release_leds = false;
    pending_alls_good_response = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    currentStep = 0;
    kioskModeRequest((currentGameSelected0 >= 10) ? "GameRunningALLS" : "GameRunning");
    triggerLEDUpdate();
    if (currentGameSelected0 > 19) {
      pdaLEDState();
    } else {
      setLEDControl(false);
    }
  }
  // else if (pending_release_leds == false && coinEnable == false && currentPowerState0 == 1 && !(currentALLSState0 == 1 && currentGameSelected0 == 10)) {
  //  startingLEDState();
  //}
  // Handle Display Handover
  displayMainState = (digitalRead(displayMainLDR) == LOW);
  if (pending_release_display == true && coinEnable == true) {
    pending_release_display = false;
    if (currentGameSelected0 < 10) {
      delay(6000);
      setDisplayState(false);
    }
  }
  // Drive LED Animations
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
    if (millis() - previousMillis >= transition_interval) {
      // Save the last time we updated the LED colors
      previousMillis = millis();
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
  // Post Shutdown Cooldown
  if (activeCooldownTimer >= 0) {
    if (activeCooldownTimer == 0 && currentMillis - previousCooldownMillis >= (nuPostCooldownMinTimeout * 60000)) {
      if (currentPowerState0 == 0) {
        setChassisFanSpeed(50);
      } else if (currentPowerState0 <= -1) {
        setChassisFanSpeed(30);
      }
      activeCooldownTimer = -1;
    }
    if (activeCooldownTimer == 1 && currentMillis - previousCooldownMillis >= (allsPostCooldownMinTimeout * 60000)) {
      if (currentPowerState0 == 0) {
        setChassisFanSpeed(50);
      } else if (currentPowerState0 <= -1) {
        setChassisFanSpeed(30);
      }
      activeCooldownTimer = -1;
    }
  }
  // Handle Inactivity Timer
  if (inactivityTimeout == true && currentGameSelected0 < 20 && currentPowerState0 == 1 && requestedPowerState0 == -1 && ((currentMillis - previousInactivityMillis) >= (inactivityMinTimeout * 60000))) {
    if (coinEnable == true || currentGameSelected0 > 19) {
      shuttingDownLEDState(1);
    } else if (coinEnable == false) {
      setGameOff();
    }
  }
  // Handle Shutdown Timer
  if (currentPowerState0 == 1 && requestedPowerState0 > -1 && ((currentMillis - previousShutdownMillis) >= (shutdownDelayMinTimeout * 60000))) {
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
}

void cpu0Loop( void * pvParameters ) {
  for(;;) {
    runtime();
  }
}
void remoteAccessLoop( void * pvParameters ) {
  for(;;) {
    checkWiFiConnection();
    server.handleClient();
    delay(1);
  }
}
bool enableIr = true;
void irRemoteLoop( void * pvParameters ) {
  for(;;) {
    if (enableIr && IrReceiver.decode()) {
      uint16_t command = IrReceiver.decodedIRData.command;
      uint16_t protocal = IrReceiver.decodedIRData.protocol;
      if (protocal == 19) {
        Serial.print("ACTION::IR_SIGNAL_");
        Serial.println(command);
        int current_percent = map(currentVolume, minimumVolume, maximumVolume, 0, 100);
        switch (command) {
          case 7:
            if (current_percent < 100 && muteVolume == false) {
              current_percent += 5;
              currentVolume = map(current_percent, 0, 100, minimumVolume, maximumVolume);
              muteVolume = false;
              ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
              displayVolumeMessage();
            }
            break;
          case 11:
            if (current_percent > 5 && muteVolume == false) {
              current_percent -= 5;
              currentVolume = map(current_percent, 0, 100, minimumVolume, maximumVolume);
              muteVolume = false;
              ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
              displayVolumeMessage();
            }
            break;
          case 15:
            muteVolume = !(muteVolume);
            ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
            displayVolumeMessage();
            break;
          case 2:
            if (currentPowerState0 == 1 && requestedPowerState0 == -1) {
              requestStandby();
            } else if (currentPowerState0 == 1 && requestedPowerState0 > -1) {
              setGameOff();
            } else if (currentPowerState0 == 0) {
              setGameOn();
            } else if (currentPowerState0 <= -1) {
              setMasterPowerOn();
            }
            delay(1000);
            break;
          default:
            break;
        }
      }
      delay(150);
      IrReceiver.resume();
    } else {
      delay(1);
    }
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
              } else if (header == "DS") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                if (valueInt >= 0 && valueInt <= 9 && inhibitNuState == false) {
                  currentGameSelected0 = valueInt;
                }
              } else if (header == "R") {
                int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
                String _nuResponse = receivedMessage.substring(headerIndex + 2, valueIndex);
                _nuResponse.trim();
                nuResponse = _nuResponse;
                Serial.println("NU_CTRL::" + nuResponse);
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
    } else if (ultraPowerSaving == true && keepManagerAwake == false && currentPowerState0 == -1 && powerInactivityMillis != -1 && shutdownPCTimer == 0) {
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
      displayIconMessage(1, true, true, 250, "チュウニズム");
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
      fan += map(currentFanSpeed, 0, 255, 0, 100);
      fan += "%";
      displayIconDualMessage(1, (map(currentFanSpeed, 0, 255, 0, 100) >= 75), false, 225, "Fans", fan.c_str());
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
      displayIconDualMessage(1, false, false, 510, "VPN: Chunithm", getEthSwitchVal(0));
      displayState = 9;
    } else if (displayState != 10 && current_time >= 10 && current_time < 11) {
      displayIconDualMessage(1, false, false, 510, "VPN: WACCA", getEthSwitchVal(1));
      displayState = 10;
    } else if (displayState != 11 && current_time >= 11 && current_time < 12) {
      displayIconDualMessage(1, (coinEnable == true), false, 71, "Card Reader", (has_cr_talked == false) ? "No Data" : (coinEnable == true) ? (currentGameSelected0 == 20) ? "Enabled*" : "Enabled" : "Disabled");
      displayState = 11;
    } else if (current_time >= 12) {
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
      currentNote = 0;
      previousMelodyMillis = 0;
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

String getEthSwitchVal(int cabNum) {
  return ((digitalRead(ethSensors[cabNum])) == LOW) ? "Official" : "Private";
}
String getGameSelect() {
  String assembledOutput = "";
  switch (currentGameSelected0) {
    case -1:
      assembledOutput = "No Data";
      break;
    case 0:
      assembledOutput = "PL Omni";
      break;
    case 1:
      assembledOutput = "PL Base";
      break;
    case 2:
      assembledOutput = "Air";
      break;
    case 3:
      assembledOutput = "Star";
      break;
    case 4:
      assembledOutput = "Crystal";
      break;
    case 5:
      assembledOutput = "Other";
      break;
    case 10:
      assembledOutput = "Sun+";
      break;
    case 12:
      assembledOutput = "Sun+E";
      break;
    case 11:
      assembledOutput = "New+";
      break;
    case 14:
      assembledOutput = "Luminous";
      break;
    case 13:
      assembledOutput = "Lumi E";
      break;
    case 15:
      assembledOutput = "Lumi+ E";
      break;
    case 20:
      assembledOutput = "PDA:FT";
      break;
    default:
      assembledOutput = "Invalid";
      break;
  }
  return assembledOutput;
}
String getPowerAuth() {
  String assembledOutput = "";
  assembledOutput += ((requestedPowerState0 != -1) ? "Warning" : ((currentPowerState0 == -2) ? "Eco Mode" : (currentPowerState0 == -1) ? "Power Off" : (currentPowerState0 == 0) ? "Standby" : (coinEnable == false) ? "Startup" : "Active"));
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
void pdaLEDState() {
  NeoPixelL.setBrightness(32);
  NeoPixelR.setBrightness(32);
  setUpperLEDColor(CRGB::Black, false);
  setSideLEDs(CRGB::Cyan, CRGB::Cyan, false);
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
  currentStep = 0;
  transition_leds = true;
  triggerLEDUpdate();
}
void shuttingDownLEDState(int state) {
  previousShutdownMillis = millis();

  currentNote = 0;
  previousMelodyMillis = 0;
  melodyPlay = 3;
  loopMelody = 2;
  startMelody = true;

  kioskModeRequest("WarningGame");
  setLEDControl(true);
  targetBrightness = 255;
  numSteps = 4.0 * 33.2;
  transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  setUpperLEDColor(CRGB::Black, true);
  setSideLEDs(CRGB::Red, CRGB::Red, true);
  pending_release_leds = false;
  animation_state = -1;
  animation_mode = 1;
  currentStep = 0;
  transition_leds = true;

  messageIcon = (requestedPowerState0 == -1) ? 96 : 223;
  messageText = (requestedPowerState0 == -1) ? "Power Off" : "Standby";
  isJpnMessage = false;
  brightMessage = 255;
  invertMessage = true;
  timeoutMessage = 25;
  typeOfMessage = 1;

  requestedPowerState0 = state;
}
void startLoadingScreen() {
  pending_release_display = true;
  kioskModeRequest((currentGameSelected0 >= 10) ? "StartALLSGame" : "StartGame");
  setDisplayState(true);
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
  enableIr = false;
  delay(1);
  if (IrReceiver.isIdle()) {
    NeoPixelL.show();
  }
  if (IrReceiver.isIdle()) {
    NeoPixelR.show();
  }
  enableIr = true;
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

      Serial.print("COIN_DISPENSE::");
      Serial.print(valueString);
      Serial.print("::");
      Serial.println(dataString);
      if (ignore_next_state == false) {
        ignore_next_state = true;
        if (responseCode >= 200 && responseCode < 300) {
          resetInactivityTimer();
        }
      }
    } else if (header == "BUTTON_REQUEST") {
      int valueIndex = inputString.indexOf("::", headerIndex + 2);
      String valueString = inputString.substring(headerIndex + 2, valueIndex);
      if (ignore_next_state == false) {
        ignore_next_state = true;
        kioskModeRequest("BUTTON_" + valueString);
      }
    } else if (header == "GPIO") {
      int valueIndex = inputString.indexOf("::", headerIndex + 2);
      String valueString = inputString.substring(headerIndex + 2, valueIndex);
      if (ignore_next_state == false) {
        ignore_next_state = true;
        Serial.println("");
        Serial.println("GPIO::" + valueString);
        Serial.println("");
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
    if (activeCooldownTimer == -1) {
      setChassisFanSpeed(50);
    } else {
      setChassisFanSpeed((currentGameSelected0 < 10) ? 75 : 100);
    }
    delay(500);
    defaultLEDState();
    standbyLEDState();
    setDisplayState(true);
    currentPowerState0 = 0;

    pending_release_leds = false;
    pending_release_display = false;
    pending_alls_good_response = false;
    allsOK = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    currentStep = 0;

    messageIcon = 223;
    messageText = "Standby Mode";
    isJpnMessage = false;
    brightMessage = 255;
    invertMessage = false;
    timeoutMessage = 10;
    typeOfMessage = 1;
    triggerLEDUpdate();
  }
  keepManagerAwake = false;
  powerInactivityMillis = -1;
  requestedPowerState0 = -1;
}
void setMasterPowerOff() {
  kioskModeRequest("StopAll");
  if (activeCooldownTimer == -1) {
    setChassisFanSpeed(30);
  } else {
      setChassisFanSpeed((currentGameSelected0 < 10) ? 75 : 100);
  }
  setLEDControl(true);
  setMarqueeState(false, false);
  if (currentPowerState0 == 1) {
    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    delay(500);
    setSysBoardPower(false);
    setTouchControl(false);
    delay(200);
  } else {
    melodyPlay = -1;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = false;
  }
  delay(500);
  setIOPower(false);
  setDisplayState(true);
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
  triggerLEDUpdate();
}
void setGameOn() {
  if (currentPowerState0 <= -1) {
    if (currentPowerState0 == -2 && currentGameSelected0 < 10) {
      requestPowerMgrOn = true;
    }
    setIOPower(true);
    delay(1000);
  }
  if (currentPowerState0 != 1) {
    melodyPlay = 0;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;

    inactivityMinTimeout = defaultInactivityMinTimeout + 5;
    previousInactivityMillis = millis();
    setTouchControl((currentGameSelected0 < 10));
    resetPSU();
    startingLEDState();
    setMarqueeState(true, false);
    setChassisFanSpeed((currentGameSelected0 < 10) ? 75 : 100);
    if (currentPowerState0 == -2 && currentGameSelected0 > 10) {
      if (requestPowerMgrOn == true) {
        while (requestPowerMgrOn == true) {
          delay(1);
        }
      } else {
        powerOnManager(true);
      }
    } else if (requestPowerMgrOn == true) {
      setDisplayState(currentGameSelected0 > 10);
    } else {
      startLoadingScreen();
      if (currentGameSelected0 >= 20) {
        pending_alls_good_response = true;
        setDisplayState(true);
      }
    }
    setSysBoardPower(true);
    
    pending_release_display = (currentGameSelected0 < 10);

    messageIcon = 96;
    messageText = "Game On";
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
  triggerLEDUpdate();
}
void setGameOff() {
  if (currentPowerState0 == 1) {
    currentPowerState0 = 0;

    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;

    pending_release_leds = false;
    pending_release_display = false;
    pending_alls_good_response = false;
    allsOK = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    currentStep = 0;

    for (int i = 0; i < 3; i++) {
      cardReaderSerial.println("BLOCKER_CLOSE::NO_DATA");
      delay(100);
    }
    setDisplayState(true);
    setSysBoardPower(false);
    setTouchControl(false);
    setLEDControl(true);
    resetPSU();
    activeCooldownTimer = 0;
    previousCooldownMillis = millis();
    kioskModeRequest("StartStandby");
    delay(250);
    standbyLEDState();
    resetMarqueeState();
    setChassisFanSpeed((currentGameSelected0 < 10) ? 75 : 100);

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
  triggerLEDUpdate();
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
    if (currentGameSelected0 > 10) {
      ALLSCtrl("DS", "PULL_ID");
    }
  }
}
void requestPowerOff() {
  if (currentPowerState0 == 1 && (coinEnable == true || currentGameSelected0 == 20)) {
    shuttingDownLEDState(0);
  } else if (currentPowerState0 == 1 && coinEnable == false) {
    setMasterPowerOff();
  } else if (currentPowerState0 == 0) {
    setMasterPowerOff();
  }
}
void requestStandby() {
  if (currentPowerState0 == 1 && (coinEnable == true || currentGameSelected0 > 19)) {
    shuttingDownLEDState(1);
  } else if (currentPowerState0 == 1 && coinEnable == false) {
    setGameOff();
  } else {
    setMasterPowerOn();
  }
}

void setSysBoardPower(bool state) {
  nuResponse = "";
  while (currentNuPowerState0 == ((currentGameSelected0 < 10) ? ((state == true) ? 0 : 1) : 1)) {
    nuControl.print("PS::");
    nuControl.println(((currentGameSelected0 < 10) ? ((state == true) ? "1" : "0") : "0"));
    delay(100);
  }
  digitalWrite(controlRelays[1], ((currentGameSelected0 < 10) ? ((state == true) ? HIGH : LOW) : LOW));
  ALLSCtrl("PS", ((currentGameSelected0 >= 10) ? ((state == true) ? "1" : "0") : "0"));
  delay((state == true) ? 200 : 500);
}
void setLEDControl(bool state) {
  digitalWrite(controlRelays[3], (state == true) ? LOW : HIGH);
  currentLEDState = (state == true) ? 0 : 1;
}
void setTouchControl(bool state) {
  digitalWrite(controlRelays[4], (state == true) ? HIGH : LOW);
  currentSliderState = (state == true) ? 0 : 1;
}
void setIOPower(bool state) {
  digitalWrite(controlRelays[0], (state == true) ? HIGH : LOW);
}
void resetPSU() {
  Serial.println("LED_DATA::16::0::000000::");
  digitalWrite(controlRelays[0], LOW);
  delay(2500);
  digitalWrite(controlRelays[0], HIGH);
}
void resetMarqueeState() {
  digitalWrite(controlRelays[2], (currentMarqueeState == 1) ? HIGH : LOW);
}
void setMarqueeState(bool state, bool save) {
  if (currentPowerState0 == 0) {
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
void setGameDisk(int number) {
  if (number < 10) {
    // Nu Switchover
    inhibitNuState = false;
    nuResponse = "";
    while (currentGameSelected0 != number) {
      nuControl.print("DS::");
      nuControl.println(String(number));
      delay(100);
    }
    if (currentPowerState0 == 1) {
      setSysBoardPower(true);
      resetPSU();
      setTouchControl(true);
      startingLEDState();
      startLoadingScreen();
    }
    for (int i = 0; i < 3; i++) {
      cardReaderSerial.println("BLOCKER_CLOSE::NO_DATA");
      delay(100);
    }
  } else {
    // ALLS Switchover
    inhibitNuState = true;
    currentGameSelected0 = number;
    ALLSCtrl("DS", "PULL_ID");
    if (currentPowerState0 == 1) {
      allsOK = false;
      pending_alls_good_response = (currentGameSelected0 == 20);
      for (int i = 0; i < 3; i++) {
        cardReaderSerial.println("BLOCKER_CLOSE::NO_DATA");
        delay(100);
      }
      setSysBoardPower(true);
      resetPSU();
      setTouchControl(false);
      startingLEDState();
      startLoadingScreen();
    }
  }
  messageIcon = 129;
  messageText = "GDD: ";
  switch (number) {
    case 0:
      messageText += "PL Omni (Nu)";
      break;
    case 1:
      messageText += "PL Base (Nu)";
      break;
    case 2:
      messageText += "Air+ (Nu)";
      break;
    case 3:
      messageText += "Star+ (Nu)";
      break;
    case 4:
      messageText += "Crystal+ (Nu)";
      break;
    case 5:
      messageText += "Other? (Nu)";
      break;
    case 10:
      messageText += "Sun+ (ALLS)";
      break;
    case 12:
      messageText += "Sun+ Ex (ALLS)";
      break;
    case 11:
      messageText += "New+ (ALLS)";
      break;
    case 13:
      messageText += "Lumi Ex (ALLS)";
      break;
    case 15:
      messageText += "Lumi+ Ex (ALLS)";
      break;
    case 14:
      messageText += "Lumi (ALLS)";
      break;
    case 20:
      messageText += "DIVA: FT (ALLS)";
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
void setEthernetState(int ethNum, int ethVal) {
  if (digitalRead(ethSensors[ethNum]) == ((ethVal == 1) ? LOW : HIGH)) {
    if (ethNum == 0 && currentPowerState0 == 1) {
      nuResponse = "";
      while (nuResponse == "") {
        nuControl.println("PS::128");
        delay(100);
      }
      if (pending_release_display == false) {
        startLoadingScreen();
      }
    }
    if (ethNum == 0) {
      messageIcon = 270;
      messageText = "Network: ";
      switch (ethVal) {
        case 0:
          messageText += "Official";
          break;
        case 1:
          messageText += "Missless";
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
    pushEthSwitch(ethNum);
  }
}
void triggerLEDUpdate() {
  if (currentPowerState0 == 1) {
    String req = "LED_DATA::";
    if (currentGameSelected0 >= 20) {
      req += "128";
    } else {
      req += "255";
    }
    req += "::4::";
    switch (currentGameSelected0) {
      case 0:
      case 1:
        // PLOST
        req += "00FF59 00F460 00EA68 00DF6F 00D577 00CA7E 00C085 00B58D 00AA94 00A09B 0095A3 008BAA 0080B2 0075B9 006BC0 0060C8 0056CF 004BD6 0041DE 0036E5 002BED 0021F4 0016FB 0411FF 0D10FF 160FFF 1F0EFF 280EFF 310DFF 3A0CFF 430BFF 4C0BFF 540AFF 5D09FF 6608FF 6F08FF 7807FF 8106FF 8A05FF 9305FF 9C04FF A403FF AD02FF B602FF BF01FF::";
        break;
      case 2:
        // Star
        req += "00B3FF 00AEFF 00A9FF 00A4FF 009FFF 0099FF 0094FF 008FFF 008AFF 0085FF 0080FF 007BFF 0076FF 0071FF 006BFF 0066FF 0061FF 005CFF 0057FF 0052FF 004DFF 0048FF 0043FF 0644FF 114DFF 1C55FF 285EFF 3366FF 3E6FFF 4A77FF 5580FF 6088FF 6C91FF 7799FF 82A2FF 8EAAFF 99B3FF A4BBFF B0C4FF BBCCFF C6D5FF D2DDFF DDE6FF E8EEFF F4F7FF::";
        break;
      case 3:
        // Air
        req += "FFA21F FFA01E FF9F1C FF9D1B FF9B19 FF9918 FF9817 FF9615 FF9414 FF9213 FF9111 FF8F10 FF8D0E FF8B0D FF8A0C FF880A FF8609 FF8508 FF8306 FF8105 FF7F03 FF7E02 FF7C01 FF7A00 FF7900 FF7700 FF7600 FF7400 FF7300 FF7100 FF7000 FF6E00 FF6D00 FF6B00 FF6A00 FF6800 FF6700 FF6500 FF6400 FF6200 FF6100 FF5F00 FF5E00 FF5C00 FF5B00::";
        break;
      case 5:
        // Crystal
        req += "0040FF 003FFF 003EFF 003DFF 003CFF 003BFF 003AFF 0039FF 0038FF 0037FF 0036FF 0035FF 0034FF 0033FF 0032FF 0031FF 0030FF 002FFF 002EFF 002DFF 002CFF 002BFF 002AFF 0329FF 0927FF 1025FF 1623FF 1C22FF 2220FF 281EFF 2F1CFF 351AFF 3B18FF 4116FF 4815FF 4E13FF 5411FF 5A0FFF 600DFF 670BFF 6D09FF 7307FF 7906FF 8004FF 8602FF::";
        break;
      case 10:
        // Sun
        req += "48CAFF::";
      case 12:
        // Sun (Extra)
        req += "FF9A47 F79E4E EFA355 E6A75B DEAC62 D6B069 CEB570 C6B977 BEBE7D B5C284 ADC78B A5CB92 9DD099 95D49F 8DD9A6 84DDAD 7CE2B4 74E6BB 6CEBC1 64EFC8 5BF4CF 53F8D6 4BFDDD 47FEE1 47FBE2 47F9E3 47F7E5 47F4E6 47F2E8 47F0E9 47EDEA 47EBEC 47E9ED 47E6EE 48E4F0 48E2F1 48DFF3 48DDF4 48DAF5 48D8F7 48D6F8 48D3F9 48D1FB 48CFFC 48CCFE::";
        break;
      case 13:
      case 15:
      case 14:
        // Luminous
        req += "FF40A0::";
        break;
      case 11:
        // New
        req += "FFFF00::";
        break;
      case 20:
        // DIVA FT
        req += "00FBFF 0BF0F8 17E5F1 22DAEB 2DCEE4 39C3DD 44B8D6 4FADCF 5BA2C9 6697C2 718BBB 7D80B4 8875AD 936AA7 9F5FA0 AA5499 B54992 C13D8B CC3285 D7277E E31C77 EE1170 F90669 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066 FF0066::";
        break;
      default:
        req += "000000::";
        break;
    }
    if (coinEnable == true) {
      req += "-1::";
    } else {
      req += "1::";
    }
    Serial.println(req);
  } else {
    Serial.println("LED_DATA::16::0::000000::");
  }
}
void resetState() {
  previousInactivityMillis = millis();
  previousShutdownMillis = millis();
  inactivityMinTimeout = defaultInactivityMinTimeout;

  startMelody = false;
  melodyPlay = -1;
  loopMelody = -1;
  currentNote = 0;
  previousMelodyMillis = 0;

  pending_release_leds = false;
  pending_release_display = false;
  pending_alls_good_response = false;
  transition_leds = false;
  animation_state = -1;
  animation_mode = -1;
  currentStep = 0;
  requestedPowerState0 = -1;

  setLEDControl(!(currentPowerState0 == 1 && currentGameSelected0 < 20));
  if (currentLEDState == 0) {
    delay(800);
    copyLEDBuffer();
  }
  triggerLEDUpdate();
  setDisplayState((currentGameSelected0 >= 10 && currentPowerState0 == 1) || (currentPowerState0 != 1));
  kioskModeRequest("ResetState");
  if (currentGameSelected0 == 20) {
    for (int i = 0; i < 3; i++) {
      cardReaderSerial.println("BLOCKER_OPEN::NO_DATA");
      delay(100);
    }
    pdaLEDState();
  }
}
void resetInactivityTimer() {
  previousInactivityMillis = millis();
  previousShutdownMillis = millis();
  inactivityMinTimeout = defaultInactivityMinTimeout + 20;
  resetState();
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

void ALLSCtrl(String command, String data) {
  Serial.println("");
  Serial.print(command);
  Serial.print("::");
  Serial.print(data);
  Serial.println("");
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
                } else if (currentPowerState0 == -1) {
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
            } else if (header == "INACTIVITY_TIMER") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "RESET") {
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
              } else if (valueString == "ID") {
                Serial.println("R::" + String(currentGameSelected0));
              } else {
                int valueInt = valueString.toInt();
                setGameDisk(valueInt);
              }
            } else if (header == "NETWORK_SELECT") {
              int cabIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String cabString = receivedMessage.substring(headerIndex + 2, cabIndex);
              int valueIndex = receivedMessage.indexOf("::", cabIndex + 2);
              String valueString = receivedMessage.substring(cabIndex + 2, valueIndex);
              if (valueString == "?") {
                String res = "INVALID";
                if (cabString == "CHUNITHM") {
                  res = getEthSwitchVal(0);
                } else if (cabString == "WACCA") {
                  res = getEthSwitchVal(1);
                }
                Serial.println("R::" + res);
              } else {
                if (cabString == "CHUNITHM") {
                  int valueInt = valueString.toInt();
                  if (valueInt >= 0 && valueInt <= 1) {
                    setEthernetState(0, valueInt);
                  }
                } else if (cabString == "WACCA") {
                  int valueInt = valueString.toInt();
                  if (valueInt >= 0 && valueInt <= 1) {
                    setEthernetState(1, valueInt);
                  }
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
            } else if (header == "ALLS_OK") {
              allsOK = true;
            } else if (header == "ALLS_NG") {
              allsOK = false;
            } else if (header == "DISPLAY_MESSAGE") {
              // DISPLAY_MESSAGE::BIG::icon::text::isJP/t::255::invert/t::timeout/20
              int iconIndex = receivedMessage.indexOf("::", headerIndex + 2);
              messageIcon = (receivedMessage.substring(headerIndex + 2, iconIndex)).toInt();
              int messageIndex = receivedMessage.indexOf("::", iconIndex + 2);
              messageText = receivedMessage.substring(iconIndex + 2, messageIndex);
              int isJpnIndex = receivedMessage.indexOf("::", messageIndex + 2);
              isJpnMessage = ((receivedMessage.substring(messageIndex + 2, isJpnIndex)).toInt() == 1);
              int brightIndex = receivedMessage.indexOf("::", isJpnIndex + 2);
              brightMessage = (receivedMessage.substring(isJpnIndex + 2, brightIndex)).toInt();
              int invertIndex = receivedMessage.indexOf("::", brightIndex + 2);
              invertMessage = ((receivedMessage.substring(brightIndex + 2, invertIndex)).toInt() == 1);
              int timeoutIndex = receivedMessage.indexOf("::", invertIndex + 2);
              timeoutMessage = (receivedMessage.substring(invertIndex + 2, timeoutIndex)).toInt();
              int typeIndex = receivedMessage.indexOf("::", timeoutIndex + 2);
              typeOfMessage = (receivedMessage.substring(timeoutIndex + 2, typeIndex)).toInt();
            } else if (header == "LED_DATA") {
              // LED_DATA::bank_id::brightness::time::take_own::is_stream::hex_data::
              int bankIndex = receivedMessage.indexOf("::", headerIndex + 2);
              int bankSelect = (receivedMessage.substring(headerIndex + 2, bankIndex)).toInt();
              int brightIndex = receivedMessage.indexOf("::", bankIndex + 2);
              int brightValue = (receivedMessage.substring(bankIndex + 2, brightIndex)).toInt();
              int timeIndex = receivedMessage.indexOf("::", brightIndex + 2);
              float timeValue = (receivedMessage.substring(brightIndex + 2, timeIndex)).toFloat();
              bool hasTransition = (timeValue > 0);
              int ownershipIndex = receivedMessage.indexOf("::", timeIndex + 2);
              bool takeOwnership = ((receivedMessage.substring(timeIndex + 2, ownershipIndex)).toInt() == 1);
              int isStreamIndex = receivedMessage.indexOf("::", ownershipIndex + 2);
              bool isStream = ((receivedMessage.substring(ownershipIndex + 2, isStreamIndex)).toInt() == 1);
              int dataIndex = receivedMessage.indexOf("::", isStreamIndex + 2);
              String ledValues = receivedMessage.substring(isStreamIndex + 2, dataIndex);
              if ((pending_release_leds == false && currentPowerState0 != 1) || takeOwnership) {
                int brightness = 64;
                if (brightValue != -1) {
                  if (brightValue > 0 && brightValue <= 255) {
                    if (hasTransition) {
                      targetBrightness = brightValue;
                    } else {
                      NeoPixelL.setBrightness(brightValue);
                      NeoPixelR.setBrightness(brightValue);
                    }
                  }
                }
                if (takeOwnership) {
                  setLEDControl((server.arg("ledValues").c_str() == "true"));
                }
                if (hasTransition) {
                  numSteps = timeValue * 33.2;
                  transition_interval = (unsigned long)(1000.0 * timeValue / (float)numSteps);
                }

                if (isStream) {
                  handleSetLeds(ledValues, bankSelect, hasTransition);
                } else {
                  handleSetLedColor(ledValues, bankSelect, hasTransition);
                }
              }
            } else if (header == "SYS_STATE") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "?") {

              } else {
                int valueInt = valueString.toInt();
                switch (valueInt) {
                  case 10: {
                    int stateIndex = receivedMessage.indexOf("::", valueIndex + 2);
                    String stateString = receivedMessage.substring(valueIndex + 2, stateIndex);
                    currentALLSState0 = stateString.toInt();
                    break;
                  }
                  default:
                    break;
                }
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
