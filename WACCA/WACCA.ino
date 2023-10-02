#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Adafruit_DS3502.h>
#include <FastLED.h>
#include "melody.h"

const char* ssid = "Radio Noise AX";
const char* password = "Radio Noise AX";
WebServer server(80);

// Power Controls
// 0 - Green - Display + PSU Enable
// 1 - Yellow - LED Select (Default ESP)
// 2 - Orange - LED 5V Enable
// 3 - Red - ALLS + IO 12V/5V Enable
// 4 - White - Marquee
int relayPins[5] = { 12, 14, 27, 26, 25 };
int numRelays = 5;
// Fan Controller
const int fanPWM = 32;
// Aux
// Card Reader Communication
const int cardReaderRX = 2;
const int cardReaderTX = 4;
bool coinEnable = false;
int cardReaderState = -1;
// WACCA LED Controls
#define ledOutput 13
#define NUM_LEDS 480 // 8 x 5 x 12 (8 Col, 60 Rows)
CRGB leds[NUM_LEDS];
CRGB leds_source[NUM_LEDS];
CRGB leds_target[NUM_LEDS];
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
const int inactivityMinTimeout = 45;
const int shutdownDelayMinTimeout = 5;
unsigned long previousInactivityMillis = 0; 
unsigned long previousShutdownMillis = 0; 
bool inactivityTimeout = true;

unsigned long previousMillis = 0; 
uint8_t numSteps = 120; // Number of steps in the transition
uint8_t currentStep = 0;
bool pending_release_leds = false;
bool pending_release_display = false;
bool transition_leds = false;
float transition_interval = 0;
int animation_mode = -1;
int animation_state = -1;

bool ready_to_boot = false;
String inputString = ""; 
int currentVolume = 0;
bool muteVolume = false;
int minimumVolume = 0;
int currentPowerState0 = -1;
int currentLEDState = 0;
int currentMarqueeState = 1;
int currentFanSpeed = 128;
int displayedSec = 0;
int displayState = -1;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
Adafruit_DS3502 ds3502 = Adafruit_DS3502();
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
TaskHandle_t Task5;

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Attempting to reconnect...");
    WiFi.hostname("CabinetManager");
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    int tryCount = 0;
    tone(buzzer_pin, NOTE_CS5, 1000 / 8);
    while (WiFi.status() != WL_CONNECTED) {
      if (tryCount > 60 && currentPowerState0 != 1) {
        ESP.restart();
      }
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
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  bootScreen("HappyCAB MCU");

  bootScreen("HARDWARE");
  pinMode(cardReaderRX, INPUT_PULLDOWN);
  pinMode(cardReaderTX, OUTPUT);
  digitalWrite(cardReaderTX, LOW);
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

  bootScreen("NETWORK");
  checkWiFiConnection();

  bootScreen("RESET_CR");
  digitalWrite(cardReaderTX, LOW);
  delay(500);
  for (int i = 0; i < 3; i++) {
    digitalWrite(cardReaderTX, HIGH);
    delay(100);
    digitalWrite(cardReaderTX, LOW);
    delay(100);
    digitalWrite(cardReaderTX, HIGH);
    delay(1300);
    digitalWrite(cardReaderTX, LOW);
    delay(500);
  }

  bootScreen("PC_LINK");
  xTaskCreatePinnedToCore(
                    pingLoop,   /* Task function. */
                    "pingTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task3,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
  while (!ready_to_boot) {
    kioskTest();
    delay(100);
  }

  bootScreen("DEFAULTS");
  currentVolume = 30;
  ds3502.setWiperDefault(30);
  ds3502.setWiper(30);
  kioskModeRequest("StopAll");
  delay(2000);

  bootScreen("REMOTE_ACCESS");
  server.on("/volume/set", [=]() {
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
        int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
        current_percent -= _volVal;
        currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
        muteVolume = false;
        server.send(200, "text/plain", String(current_percent));
      }
    } else if (server.hasArg("up")) {
      int _volVal = server.arg("up").toInt();
      if (_volVal > 0 && _volVal <= 100) {
        int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
        current_percent += _volVal;
        currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
        muteVolume = false;
        server.send(200, "text/plain", String(current_percent));
      }
    } else if (server.hasArg("percent")) {
      int _volVal = server.arg("percent").toInt();
      if (_volVal > 0 && _volVal <= 100) {
        currentVolume = map(_volVal, 0, 100, minimumVolume, 127);
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
      response += map(currentVolume, minimumVolume, 127, 0, 100);
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

  // setEthSelects
  //
  // setEthSelect
  //
  // getEthSelect
  //

  // enableOmnimix
  //
  // disableOmnimix
  //
  // getOmnimix
  //
  
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
    if (currentPowerState0 == 1 && coinEnable == false) {
      setGameOff();
    } else if (currentPowerState0 == 1 && coinEnable == true) {
      shuttingDownLEDState(0);
    } else if (currentPowerState0 == 0) {
      setMasterPowerOff();
    }
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
  bootScreen("BOOT_CRLC");
  xTaskCreatePinnedToCore(
                  cardReaderRXLoop,   /* Task function. */
                  "cardReaderRX",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &Task4,      /* Task handle to keep track of created task */
                  0);          /* pin task to core 1 */
  delay(500);
  xTaskCreatePinnedToCore(
                  cardReaderTXLoop,   /* Task function. */
                  "cardReaderTX",     /* name of task. */
                  10000,       /* Stack size of task */
                  NULL,        /* parameter of the task */
                  1,           /* priority of the task */
                  &Task5,      /* Task handle to keep track of created task */
                  0);          /* pin task to core 1 */
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

bool ignore_next_state = false;
void loop() {
  checkWiFiConnection();
  server.handleClient();
  unsigned long currentMillis = millis();
  // Proccess Reported Card Reader State
  switch (cardReaderState) {
    case 1:
      coinEnable = false;
      ignore_next_state = false;
      break;
    case 3:
      coinEnable = true;
      ignore_next_state = false;
      break;
    case 5:
      if (ignore_next_state == false) {
        ignore_next_state = true;
        resetInactivityTimer();
      }
      break;
    case 7:
      if (ignore_next_state == false) {
        ignore_next_state = true;
        setGameOn();
      }
      break;
    default:
      break;
  }
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
  //displayMainState = (digitalRead(displayMainLDR) == LOW);
  if (pending_release_display == true && coinEnable == true) {
    pending_release_display = false;
    kioskModeRequest("GameRunning");
    //delay(6000);
    setDisplayState(false);
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
      if (currentStep <= numSteps) {
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
  // Play Melody Sheets
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
        int position = map(position, 0, 4, loopMelody, 0) * 1000;
        if (position <= 1) {
          playMelody(warning_tone_long, warning_tone_long_dur, sizeof(warning_tone_long_dur) / sizeof(int));
        } else {
          playMelody(warning_tone, warning_tone_dur, sizeof(warning_tone_dur) / sizeof(int));
        }
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
void pingLoop( void * pvParameters ) {
  for(;;) {
    delay(1000);
    Serial.println("");
    Serial.println("_KIOSK_HELLO?_");
    Serial.println("");
  }
}
void serialLoop( void * pvParameters ) {
  for(;;) {
    delay(1);
    kioskCommand();
  }
}
void cardReaderTXLoop( void * pvParameters ) {
  for(;;) {
    // Send Block State
    digitalWrite(cardReaderTX, HIGH);
    delay(100);
    digitalWrite(cardReaderTX, LOW);
    delay(100);
    digitalWrite(cardReaderTX, HIGH);
    if (currentPowerState0 == -1 || (currentPowerState0 == 1 && requestedPowerState0 != -1)) {
      delay(100);
    } else if (currentPowerState0 == 0) {
      delay(300);
    } else if (currentPowerState0 == 1 && requestedPowerState0 == -1) {
      delay(500);
    }
    digitalWrite(cardReaderTX, LOW);
    delay(500);
  }
}
void cardReaderRXLoop( void * pvParameters ) {
  for(;;) {
    bool abort = false;
    bool found_header = false;
    int lastPulse = 0;
    if (digitalRead(cardReaderRX) == LOW) {
      delay(1);
    } else {
      lastPulse = millis();
      while (found_header == false) {
        // Continue to wait for header to complete
        delay(1);
        if (millis() - lastPulse >= 300 && digitalRead(cardReaderRX) == LOW) {
          found_header = true;
        } else if (digitalRead(cardReaderRX) == HIGH) {
          lastPulse = millis();
          found_header = false;
        }
      }
      while (abort == false && digitalRead(cardReaderRX) == LOW) {
        // Continue to wait for header to drop off
        delay(1);
        if (millis() - lastPulse >= 2000) {
          abort = true;
        }
      }
      lastPulse = millis();
      while (abort == false && digitalRead(cardReaderRX) == HIGH) {
        // Continue to wait for header to drop off
        delay(1);
        if (millis() - lastPulse >= 2000) {
          abort = true;
        }
      }
      lastPulse = millis();
      while (abort == false && digitalRead(cardReaderRX) == LOW) {
        // Continue to wait for header to drop off
        delay(1);
        if (millis() - lastPulse >= 2000) {
          abort = true;
        }
      }
      lastPulse = millis();
      while (abort == false && digitalRead(cardReaderRX) == HIGH) {
        // Start Calculating State
        delay(1);
        if (millis() - lastPulse >= 2000) {
          abort = true;
        }
      }
      if (abort == true) {
        delay(1000);
      } else {
        cardReaderState = round((millis() - lastPulse) / 100);
        Serial.println("");
        Serial.println("_KIOSK_CRS_" + String(cardReaderState) + "_");
        Serial.println("");
      }
    }
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
  } else if (currentPowerState0 == -1) {
    delay(500);
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
      if (currentPowerState0 == 1 && inactivityTimeout == true) {
        timeout = String(inactivityMinTimeout - ((millis() - previousInactivityMillis) / 60000));
        timeout += " Min";
      } else if (requestedPowerState0 != -1) {
        timeout = "Expired";
      } else {
        timeout = "Inactive";
      }
      displayIconDualMessage(1, ((currentPowerState0 == 1 && inactivityTimeout == true) ? false : (((millis() - previousInactivityMillis) / 60000) < (inactivityMinTimeout - 5))), false, 459, "Timeout", timeout);
      displayState = 2;
    } else if (displayState != 3 && current_time >= 3 && current_time < 4) {
      String volume = "";
      if (muteVolume == false) {
        volume += map(currentVolume, minimumVolume, 127, 0, 100);
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
      displayIconDualMessage(1, (currentLEDState == 1), false, 398, "LED Control", (currentLEDState == 0) ? "MCU" : "Game");
      displayState = 6;
    } else if (displayState != 7 && current_time >= 7 && current_time < 8) {
      displayIconDualMessage(1, (coinEnable == true), false, 71, "Card Reader", (cardReaderState == -1) ? "No Data" : (coinEnable == true) ? "Enabled" : "Disabled");
      displayState = 7;
    } else if (current_time >= 8) {
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

String getPowerAuth() {
  String assembledOutput = "";
  assembledOutput += ((requestedPowerState0 != -1) ? "Warning" : ((currentPowerState0 == -1) ? "Power Off" : (currentPowerState0 == 0) ? "Standby" : (coinEnable == false) ? "Startup" : "Active"));
  return assembledOutput;
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
  int hue = 120;
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
  requestedPowerState0 = -1;
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
  setDisplayState(false);
  requestedPowerState0 = -1;
  currentPowerState0 = -1;
}
void setGameOn() {
  if (currentPowerState0 == -1) {
    setIOPower(true);
    delay(250);
  }
  if (currentPowerState0 != 1) {
    previousInactivityMillis = millis();
    startLoadingScreen();
    setChassisFanSpeed(100);
    startingLEDState();
    setMarqueeState(true, false);
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
  if (requestedPowerState0 != -1) {
    resetInactivityTimer();
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
    setDisplayState(true);
    setChassisFanSpeed(50);
    kioskModeRequest("StartStandby");
    setLEDControl(true);
    delay(250);
    standbyLEDState();
    resetMarqueeState();
    setSysBoardPower(false);
    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
  }
  currentPowerState0 = 0;
  requestedPowerState0 = -1;
}

void setSysBoardPower(bool state) {
  digitalWrite(relayPins[3], (state == true) ? HIGH : LOW);
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
void resetMarqueeState() {
  digitalWrite(relayPins[4], (currentMarqueeState == 1) ? HIGH : LOW);
}
void setMarqueeState(bool state, bool save) {
  if (currentPowerState0 != -1) {
    digitalWrite(relayPins[4], (state == true) ? HIGH : LOW);
  }
  if (save == true) {
    currentMarqueeState = (state == true) ? 1 : 0;
  }
}
void setChassisFanSpeed(int speed) {
  currentFanSpeed = map(speed, 0, 100, 0, 255);
  analogWrite(fanPWM, currentFanSpeed);
}
void setDisplayState(bool state) {
  //if (state != displayMainState) {
  //  pushDisplaySwitch();
  //}
}
void resetState() {
  setLEDControl((currentPowerState0 != 1));
  if (currentLEDState == 0) {
    delay(800);
    setLEDControl(false);
  }
  setDisplayState(false);
  kioskModeRequest("ResetState");
  melodyPlay = -1;
  loopMelody = -1;
  currentNote = 0;
  previousMelodyMillis = 0;
  startMelody = false;
  pending_release_leds = false;
  pending_release_display = false;
  animation_state = -1;
  animation_mode = -1;
  currentStep = 0;
  requestedPowerState0 = -1;
}
void resetInactivityTimer() {
  previousInactivityMillis = millis();
  resetState();
}


void kioskModeRequest(String command) {
  Serial.println("");
  Serial.println("_KIOSK_" + command + "_");
  Serial.println("");
}
void kioskTest() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inputString.length() > 13) {
      inputString = inputString.substring(1);
    }
    if (inputString == "_KIOSK_READY_") {
      ready_to_boot = true;
      if(Task3 != NULL) {
        vTaskDelete(Task3);
      }
    }
  }
}
void kioskCommand() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inputString.length() > 13) {
      inputString = inputString.substring(1);
    }
    if (inputString == "_KIOSK_POFF _") {
      setMasterPowerOff();
    } else if (inputString == "_KIOSK_ROFF _") {
      if (currentPowerState0 == 1 && coinEnable == false) {
        setGameOff();
      } else if (currentPowerState0 == 1 && coinEnable == true) {
        shuttingDownLEDState(0);
      } else if (currentPowerState0 == 0) {
        setMasterPowerOff();
      }
    } else if (inputString == "_KIOSK_PSTB _") {
      if (currentPowerState0 == 1) {
        setGameOff();
      } else {
        setMasterPowerOn();
      }
    } else if (inputString == "_KIOSK_RSTB _") {
      if (currentPowerState0 == 1 && coinEnable == false) {
        setGameOff();
      } else if (currentPowerState0 == 1 && coinEnable == true) {
        shuttingDownLEDState(1);
      } else if (currentPowerState0 == -1) {
        setMasterPowerOn();
      }
    } else if (inputString == "_KIOSK_PON  _") {
      setGameOn();
    } else if (inputString == "_KIOSK_PS   _") {
      Serial.println("_KIOSK_DATA_[" + getPowerAuth() + "]_");
    } else if (inputString == "_KIOSK_ITS  _") {
      Serial.println("_KIOSK_DATA_[" + String((currentPowerState0 == 1 && inactivityTimeout == true) ? 0 : (inactivityMinTimeout - ((millis() - previousInactivityMillis) / 1000))) + "]_");
    } else if (inputString == "_KIOSK_STS  _") {
      Serial.println("_KIOSK_DATA_[" + String((currentPowerState0 == 1) ? 0 : (shutdownDelayMinTimeout - ((millis() - previousShutdownMillis) / 1000))) + "]_");
    } else if (inputString == "_KIOSK_SR   _") {
      resetState();
    } else if (inputString == "_KIOSK_VU05 _") {
      int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
      current_percent += 5;
      currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
      muteVolume = false;
      ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    } else if (inputString == "_KIOSK_VU10 _") {
      int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
      current_percent += 10;
      currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
      muteVolume = false;
      ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    } else if (inputString == "_KIOSK_VU25 _") {
      int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
      current_percent += 25;
      currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
      muteVolume = false;
      ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    } else if (inputString == "_KIOSK_VD05 _") {
      int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
      current_percent -= 5;
      currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
      muteVolume = false;
      ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    } else if (inputString == "_KIOSK_VD10 _") {
      int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
      current_percent -= 10;
      currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
      muteVolume = false;
      ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    } else if (inputString == "_KIOSK_VD25 _") {
      int current_percent = map(currentVolume, minimumVolume, 127, 0, 100);
      current_percent -= 25;
      currentVolume = map(current_percent, 0, 100, minimumVolume, 127);
      muteVolume = false;
      ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    } else if (inputString == "_KIOSK_VS   _") {
      Serial.println("_KIOSK_DATA_[" + String(map(currentVolume, minimumVolume, 127, 0, 100)) + "]_");
    } else if (inputString == "_KIOSK_VM1  _") {
      muteVolume = true;
      ds3502.setWiper(minimumVolume);
    } else if (inputString == "_KIOSK_VM0  _") {
      muteVolume = false;
      ds3502.setWiper(currentVolume);
    } else if (inputString == "_KIOSK_VMT  _") {
      muteVolume = !(muteVolume);
      ds3502.setWiper((muteVolume == true) ? minimumVolume : currentVolume);
    } else if (inputString == "_KIOSK_VMS  _") {
      Serial.println("_KIOSK_DATA_[" + String((muteVolume == true) ? "Muted" : "Unmuted") + "]_");
    } else if (inputString == "_KIOSK_PING _") {
      Serial.println("_KIOSK_DATA_[PONG]_");
    }
  }
}