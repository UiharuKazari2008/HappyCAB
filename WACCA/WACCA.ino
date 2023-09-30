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
const int coinBlocker = 35;
bool coinEnable = false;
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
bool loopMelody = false;
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

unsigned long previousMillis = 0; 
uint8_t numSteps = 120; // Number of steps in the transition
uint8_t currentStep = 0;
bool pending_release_leds = false;
bool transition_leds = false;
float transition_interval = 0;
int animation_mode = -1;
int animation_state = -1;

int currentVolume = 0;
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
  delay(5000);

  bootScreen("HARDWARE");
  pinMode(coinBlocker, INPUT);
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
    }
    shuttingDownLEDState();
    melodyPlay = 3;
    loopMelody = true;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    server.send(200, "text/plain", "OK");
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
    if (pending_release_leds == false) {
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
      if (pending_release_leds == false) {
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
  server.on("/getLEDState", [=]() {
    server.send(200, "text/plain", (currentLEDState == 0)  ? "MCU" : "GAME");
  });
  server.on("/returnLED", [=]() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type X-Requested-With");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.send(200, "text/plain", "LED Returned to owner");
    setLEDControl((currentPowerState0 != 1));
    if (currentLEDState == 0) {
      delay(800);
      FastLED.show();
    }
    melodyPlay = -1;
    loopMelody = false;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = false;
    pending_release_leds = false;
  });
  server.on("/resetLED", [=]() {
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
  if (pending_release_leds == true && coinEnable == true) {
    setLEDControl(false);
    pending_release_leds = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
  } else if (pending_release_leds == false && coinEnable == false && currentPowerState0 == 1) {
    pending_release_leds = true;
    startingLEDState();
  }
  unsigned long currentMillis = millis();
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
      displayIconMessage(1, true, true, 250, "ワッカ");
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
      displayIconDualMessage(1, (currentLEDState == 1), false, 398, "LED Control", (currentLEDState == 0) ? "MCU" : "Game");
      displayState = 5;
    } else if (displayState != 6 && current_time >= 6 && current_time < 7) {
      displayIconDualMessage(1, (coinEnable == true), false, 71, "Card Reader", (coinEnable == true) ? "Enabled" : "Disabled");
      displayState = 6;
    } else if (current_time >= 7) {
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
    if (loopMelody == true) {
      pauseBetweenNotes = 5000;
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
  assembledOutput += ((currentPowerState0 == -1) ? "Power Off" : (currentPowerState0 == 0) ? "Standby" : (coinEnable == false) ? "Startup" : "Active");
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
  transition_leds = true;
}
void shuttingDownLEDState() {
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
    resetMarqueeState();
    setChassisFanSpeed(50);
    delay(750);
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
  }
  delay(500);
  setIOPower(false);
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
    delay(1000);
    setSysBoardPower(true);
    melodyPlay = 0;
    loopMelody = false;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
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
    pending_release_leds = false;
    transition_leds = false;
    animation_state = -1;
    animation_mode = -1;
    melodyPlay = 1;
    loopMelody = false;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
  }
  currentPowerState0 = 0;
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