#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Adafruit_DS3502.h>
#include <Adafruit_NeoPixel.h>
#include <FastLED.h>
#include <SoftwareSerial.h>
#include <IRremote.hpp>
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
const int defaultInactivityMinTimeout = 45;
int inactivityMinTimeout = 45;
const int shutdownDelayMinTimeout = 5;
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

bool ready_to_boot = false;
String inputString = ""; 
String attachedSoftwareCU = "Unknown";
int currentVolume = 0;
bool muteVolume = false;
int minimumVolume = 20;
int maximumVolume = 127;
bool inhibitNuState = false;
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
  inputString.reserve(200); 
  attachedSoftwareCU.reserve(200); 
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
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

  bootScreen("CARD_LINK");
  cardReaderSerial.begin(9600, SERIAL_8N1, 16, 17);
  if (!cardReaderSerial) {
    bootScreen("CARDREADER_FAIL_1");
    while (1) { // Don't continue with invalid configuration
      delay (1000);
    }
  }
  noTone(buzzer_pin);
  delay(500);

  bootScreen("NETWORK");
  checkWiFiConnection();

  tone(buzzer_pin, NOTE_C6);
  bootScreen("RST_READER");
  for (int i = 0; i < 3; i++) {
    cardReaderSerial.println("REBOOT::NO_DATA");
    delay(100);
  }
  testCardReader();
  noTone(buzzer_pin);


  bootScreen("DEFAULTS");
  currentVolume = map(25, 0, 100, minimumVolume, maximumVolume);
  ds3502.setWiper(currentVolume);
  ds3502.setWiperDefault(currentVolume);
  displayMainState = (digitalRead(displayMainLDR) == LOW);
  kioskModeRequest("StopAll");
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
  server.on("/fan", [=]() {
    String response = "";
    response += map(currentFanSpeed, 0, 255, 0, 100);
    server.send(200, "text/plain", response);
  });

  server.on("/display/pc", [=]() {
    setDisplayState(true);
    server.send(200, "text/plain", (displayMainSelect == true) ? "UNCHANGED" : "OK");
  });
  server.on("/display/game", [=]() {
    setDisplayState(false);
    server.send(200, "text/plain", (displayMainSelect == false) ? "UNCHANGED" : "OK");
  });
  server.on("/display/switch", [=]() {
    String assembledOutput = "";
    pushDisplaySwitch();
    assembledOutput += ((displayMainSelect == false) ? "PC" : "AUX");
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/display", [=]() {
    String assembledOutput = "";
    assembledOutput += ((displayMainSelect == true) ? "PC" : "GAME");
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
                setDisplayState(true);
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
  server.on("/select/game/other_1", [=]() {
    if (currentGameSelected0 != 2) {
      setGameDisk(2);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/other_2", [=]() {
    if (currentGameSelected0 != 3) {
      setGameDisk(3);
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
  server.on("/select/game/new", [=]() {
    if (currentGameSelected0 != 11) {
      setGameDisk(11);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game", [=]() {
    String const val = getGameSelect();
    server.send(200, "text/plain", val);
  });
  server.on("/sys_board", [=]() {
    server.send(200, "text/plain", (currentGameSelected0 >= 10) ? "ALLS MX2" : "Nu 1.1");
  });
  
  server.on("/power/off", [=]() {
    server.send(200, "text/plain", (currentPowerState0 == -1) ? "UNCHNAGED" : "OK");
    setMasterPowerOff();
    currentPowerState0 = -1;
  });
  server.on("/power/standby", [=]() {
    if (currentPowerState0 == 1) {
      setGameOff();
      server.send(200, "text/plain", "OK");
    } else if (currentPowerState0 == -1) {
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
    } else if (currentPowerState0 == -1) {
      setMasterPowerOn();
    }
  });
  server.on("/request/off", [=]() {
    server.send(200, "text/plain", "OK");
    requestPowerOff();
  });

  server.on("/test/nu/off", [=]() {
    setDisplayState(true);
    nuResponse = "";
    while (currentNuPowerState0 == 1) {
      nuControl.println("PS::0");
      delay(100);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/nu/on", [=]() {
    setDisplayState(false);
    nuResponse = "";
    while (currentNuPowerState0 == 0) {
      nuControl.println("PS::1");
      delay(100);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/test/nu/reset", [=]() {
    setDisplayState(false);
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
    server.send(200, "text/plain", (requestedPowerState0 == 0) ? "Warning" : ((currentPowerState0 != -1) ? "Enabled" : "Disabled"));
  });
  server.on("/game_power", [=]() {
    server.send(200, "text/plain", (requestedPowerState0 >= 0) ? "Warning" : ((currentPowerState0 == 1) ? "Enabled" : "Disabled"));
  });
  
  server.on("/timeout/on", [=]() {
    if (inactivityTimeout == false) {
      resetInactivityTimer();
      inactivityTimeout == true;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/timeout/off", [=]() {
     if (inactivityTimeout == true) {
      inactivityTimeout == false;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
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
    if (pending_release_leds == false && (currentPowerState0 != 1 || currentGameSelected0 >= 10)) {
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
    if (pending_release_leds == false && (currentPowerState0 != 1 || currentGameSelected0 >= 10)) {
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
  // Handle LED Handover
  if (pending_release_leds == true && coinEnable == true) {
    setLEDControl(false);
    pending_release_leds = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    currentStep = 0;
  }
  // else if (pending_release_leds == false && coinEnable == false && currentPowerState0 == 1 && !(currentALLSState0 == 1 && currentGameSelected0 == 10)) {
  //  startingLEDState();
  //}
  // Handle Display Handover
  displayMainState = (digitalRead(displayMainLDR) == LOW);
  if (pending_release_display == true && coinEnable == true) {
    pending_release_display = false;
    kioskModeRequest("GameRunning");
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
      } else if (currentPowerState0 == -1) {
        setChassisFanSpeed(30);
      } 
      activeCooldownTimer = -1;
    }
    if (activeCooldownTimer == 1 && currentMillis - previousCooldownMillis >= (allsPostCooldownMinTimeout * 60000)) {
      if (currentPowerState0 == 0) {
        setChassisFanSpeed(50);
      } else if (currentPowerState0 == -1) {
        setChassisFanSpeed(30);
      } 
      activeCooldownTimer = -1;
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
    checkWiFiConnection();
    server.handleClient();
    delay(1);
  }
}
void irRemoteLoop( void * pvParameters ) {
  for(;;) {
    if (IrReceiver.decode()) {
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
            } else if (currentPowerState0 == -1) {
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
    if (currentPowerState0 == -1 || (currentPowerState0 == 1 && requestedPowerState0 != -1)) {
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


void runtime() {
  int time_in_sec = esp_timer_get_time() / 1000000;
  int current_time = (time_in_sec - displayedSec) / 2;

  if (displayState != 1 && currentPowerState0 == -1) {
      const String power = getPowerAuth();
      displayIconDualMessage(1, false, false, (power == "Active") ? 491 : 490, "System Power", power);
      displayState = 1;
  } else if (currentPowerState0 == -1) {
    delay(500);
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
      displayIconDualMessage(1, (currentVolume >= 50 || muteVolume == true), false, 484, "Speakers", volume.c_str());
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
      displayIconDualMessage(1, (coinEnable == true), false, 71, "Card Reader", (has_cr_talked == false) ? "No Data" : (coinEnable == true) ? "Enabled" : "Disabled");
      displayState = 9;
    } else if (displayState != 10 && current_time >= 10 && current_time < 11) {
      displayIconDualMessage(1, false, false, 510, "Net: Chunithm", getEthSwitchVal(1));
      displayState = 10;
    } else if (displayState != 11 && current_time >= 11 && current_time < 12) {
      displayIconDualMessage(1, false, false, 510, "Net: WACCA", getEthSwitchVal(0));
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
      assembledOutput = "Omnimix";
      break;
    case 1:
      assembledOutput = "Base";
      break;
    case 2:
      assembledOutput = "Other #1";
      break;
    case 3:
      assembledOutput = "Other #2";
      break;
    case 10:
      assembledOutput = "Sun Plus";
      break;
    case 11:
      assembledOutput = "New Plus";
      break;
    default:
      assembledOutput = "Other";
      break;
  }
  return assembledOutput;
}
String getPowerAuth() {
  String assembledOutput = "";
  assembledOutput += ((requestedPowerState0 != -1) ? "Warning" : ((currentPowerState0 == -1) ? "Power Off" : (currentPowerState0 == 0) ? "Standby" : (coinEnable == false) ? "Startup" : "Active"));
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
}
void shuttingDownLEDState(int state) {
  kioskModeRequest("WarningGame");
  setLEDControl(true);
  targetBrightness = 255;
  numSteps = 4.0 * 33.2;
  transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  setUpperLEDColor(CRGB::Black, true);
  setSideLEDs(CRGB::Red, CRGB::Red, true);
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
  kioskModeRequest((currentGameSelected0 >= 10) ? "StartALLSGame" : "StartGame");
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
  if (IrReceiver.isIdle()) {
    NeoPixelL.show();
    NeoPixelR.show();
  }
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

      Serial.println(valueString);
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
  if (currentPowerState0 == -1) {
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
  }
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
    delay(500);
    setSysBoardPower(false);
    setTouchControl(false);
    delay(200);    
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
  requestedPowerState0 = -1;
  currentPowerState0 = -1;
}
void setGameOn() {
  if (currentPowerState0 == -1) {
    setIOPower(true);
    delay(1000);
  }
  if (currentPowerState0 != 1) {
    inactivityMinTimeout = defaultInactivityMinTimeout + 5;
    previousInactivityMillis = millis();
    resetPSU();
    startLoadingScreen();
    setChassisFanSpeed((currentGameSelected0 < 10) ? 75 : 100);
    startingLEDState();
    setMarqueeState(true, false);
    setSysBoardPower(true);
    setTouchControl((currentGameSelected0 < 10));
    setDisplayState(true);
    
    melodyPlay = 0;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    
    pending_release_display = true;
    
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

    
    setSysBoardPower(false);
    resetPSU();
    setDisplayState(true);
    activeCooldownTimer = 0;
    previousCooldownMillis = millis();
    setChassisFanSpeed((currentGameSelected0 < 10) ? 75 : 100);
    kioskModeRequest("StartStandby");
    setLEDControl(true);
    delay(250);
    standbyLEDState();
    resetMarqueeState();
    setTouchControl(false);
    
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
  currentPowerState0 = 0;
  requestedPowerState0 = -1;
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
  if (currentPowerState0 == 1 && coinEnable == false) {
    setGameOff();
  } else if (currentPowerState0 == 1 && coinEnable == true) {
    shuttingDownLEDState(1);
  } else {
    setMasterPowerOn();
  }
}

void setSysBoardPower(bool state) {
  nuResponse = "";
  while (currentNuPowerState0 == ((currentGameSelected0 < 10) ? ((state == true) ? 0 : 1) : 1)) {
    nuControl.print("PS::");
    nuControl.println(((currentGameSelected0 < 10) ? ((state == true) ? "1" : "0") : "0");
    delay(100);
  }
  digitalWrite(controlRelays[1], ((currentGameSelected0 < 10) ? ((state == true) ? HIGH : LOW) : LOW));
  ALLSCtrl("PS", ((currentGameSelected0 >= 10) ? ((state == true) ? "1" : "0") : 0));
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
  digitalWrite(controlRelays[0], LOW);
  delay(2000);
  digitalWrite(controlRelays[0], HIGH);
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
      setTouchControl(true);
      resetPSU();
      setDisplayState(true);
      startingLEDState();
      startLoadingScreen();
    }
  } else {
    // ALLS Switchover
    inhibitNuState = true;
    currentGameSelected0 = number;
    ALLSCtrl("DS", number);
    if (currentPowerState0 == 1) {
      setSysBoardPower(true);
      resetPSU();
      setTouchControl(false);
      setDisplayState(true);
      startingLEDState();
      startLoadingScreen();
    }
  }
  messageIcon = 129;
  messageText = "HDD: ";
  switch (number) {
    case 0:
      messageText += "PL Omni (Nu)";
      break;
    case 1:
      messageText += "PL Base (Nu)";
      break;
    case 2:
      messageText += "Other #1 (Nu)";
      break;
    case 3:
      messageText += "Other #2 (Nu)";
      break;
    case 10:
      messageText += "Sun+ (ALLS)";
      break;
    case 11:
      messageText += "New+ (ALLS)";
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
        setDisplayState(true);
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
void resetState() {
  setLEDControl((currentPowerState0 != 1 && currentGameSelected0 < 10));
  if (currentLEDState == 0) {
    delay(800);
    copyLEDBuffer();
  }
  setDisplayState((currentGameSelected0 >= 10 && currentPowerState0 == 1) || (currentPowerState0 != 1));
  kioskModeRequest("ResetState");
  startMelody = false;
  melodyPlay = -1;
  loopMelody = -1;
  currentNote = 0;
  previousMelodyMillis = 0;

  pending_release_leds = false;
  pending_release_display = false;
  transition_leds = false;
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
            } else if (header == "PING") {
              Serial.println("R::PONG");
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