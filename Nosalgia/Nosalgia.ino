#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <NewPing.h>
#include <HTTPClient.h>
#include <U8g2lib.h>

#define MAX_DISTANCE 150 // Define the maximum distance in centimeters (adjust as needed)
#define TRIGGER_DISTANCE 50
#define TRIGGER_COUNT 3
#define OCCUPIED_SLEEP 5
#define PING_DELAY 1 
#define UNOCCUPIED_CALLBACK_DELAY_MIN 1

const char* ssid = "Radio Noise AX";
const char* password = "Radio Noise AX";
WebServer server(80);

const int numOfRelays = 1;
const int powerToggleRelay = 12;
const int gameSelectRelay0 = 14;
const int touchResetRelay = 25;
const int ledSelectRelay = 26;
const int muteAudioRelay = 33;
const int touchSelectLDR = 35;
const int touchSelectPin = 27;
const int sonarEchoPin = 32; // PURPLE
const int sonarTriggerPin = 13; // BLUE
const int sonarMaxDistance = 150;
const int HDMISwitch1LDR = 34;
const int HDMISwitch1Key = 19;

int currentGameSelected = 0;
int currentPowerState = 0;
int currentMuteState = 0;
int triggered_count = 0;
int last_trigger_time = 0;
int last_sonar_check = 0;
int unoccupied_time = 0;
int last_sonar_distance = -1;

int displayedSec = 0;
int displayState = -1;
bool occupied = false;
bool occupied_triggered = false;
bool enable_sonar_search = true;
const char *occupied_url = "http://192.168.100.62:3001/button-streamdeck8?event=single-press";
const char *unoccupied_url = "http://192.168.100.62:3001/button-streamdeck8?event=double-press";

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
TaskHandle_t Task1;
TaskHandle_t Task2;

NewPing sonar(sonarTriggerPin, sonarEchoPin, MAX_DISTANCE);

void toggleTouchState() {
  digitalWrite(touchSelectPin, HIGH);
  digitalWrite(touchSelectPin, LOW);
  delay(150);
  digitalWrite(touchSelectPin, HIGH);
}
void toggleHDMISwitch0State() {
  digitalWrite(HDMISwitch1Key, HIGH);
  digitalWrite(HDMISwitch1Key, LOW);
  delay(350);
  digitalWrite(HDMISwitch1Key, HIGH);
}
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
      if (tryCount > 30 && analogRead(touchSelectLDR) <= 3000) {
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
void sonarOccupancyTest() {
  int time_in_sec = esp_timer_get_time() / 1000000;
  if (enable_sonar_search == true && time_in_sec - last_sonar_check > PING_DELAY && (occupied_triggered == false || (occupied_triggered == true && time_in_sec - last_trigger_time > (OCCUPIED_SLEEP * 60)))) {
    last_sonar_check = time_in_sec;
    unsigned int distance = sonar.ping_cm(); // Send a ping and get the distance in centimeters
    last_sonar_distance = distance;
    if (triggered_count >= TRIGGER_COUNT && distance <= TRIGGER_DISTANCE) {
        Serial.println("OCCUPIED LOCK");
        last_trigger_time = time_in_sec;
        if (occupied == false && occupied_triggered == false) {
          HTTPClient http;
          String url = String(occupied_url);
          Serial.println("Sending GET request to: " + url);
          http.begin(url);
          int httpCode = http.GET();
          http.end();
          Serial.println("HTTP Response code: " + String(httpCode));
          Serial.println("CALL OCCUPIED URL");
          if (httpCode == 200) {
            occupied = true;
            occupied_triggered = true;
          }
        }
      } else if (distance > 0 && distance <= TRIGGER_DISTANCE) {
        triggered_count += 1;
        Serial.print("TRIGGER - Distance: ");
        Serial.print(distance);
        Serial.println(" cm");
      } else {
        if (occupied_triggered == true) {
          unoccupied_time = time_in_sec;
        }
        triggered_count = 0;
        occupied_triggered = false;
        Serial.print("SEARCH - Distance: ");
        Serial.print(distance);
        Serial.println(" cm");
        if (occupied == true && time_in_sec - unoccupied_time > (UNOCCUPIED_CALLBACK_DELAY_MIN * 60)) {
          HTTPClient http;
          String url = String(unoccupied_url);
          Serial.println("Sending GET request to: " + url);
          http.begin(url);
          int httpCode = http.GET();
          http.end();
          Serial.println("HTTP Response code: " + String(httpCode));
          Serial.println("CALL OCCUPIED URL");
          if (httpCode == 200) {
            occupied = false;
          }
        }
      }
  }
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  bootScreen("CABiNET MCU");
  delay(5000);

  bootScreen("HARDWARE");
  pinMode(HDMISwitch1LDR, INPUT);
  pinMode(powerToggleRelay, OUTPUT);
  pinMode(touchSelectPin, OUTPUT);
  pinMode(touchResetRelay, OUTPUT);
  pinMode(gameSelectRelay0, OUTPUT);
  pinMode(ledSelectRelay, OUTPUT);
  pinMode(muteAudioRelay, OUTPUT);
  pinMode(HDMISwitch1Key, OUTPUT);

  digitalWrite(HDMISwitch1Key, HIGH);
  digitalWrite(touchSelectPin, HIGH);
  digitalWrite(gameSelectRelay0, LOW);
  digitalWrite(ledSelectRelay, HIGH);
  digitalWrite(touchResetRelay, HIGH);
  digitalWrite(muteAudioRelay, LOW);

  bootScreen("NETWORK");
  checkWiFiConnection();

  bootScreen("DEFAULTS");
  digitalWrite(powerToggleRelay, LOW);
  if (analogRead(touchSelectLDR) >= 3000) {
    toggleTouchState();
  }
  if (digitalRead(HDMISwitch1LDR) == LOW) {
    toggleHDMISwitch0State();
  }
  
  server.on("/setBeatstream", [=]() {
    if (currentGameSelected == 0 && currentPowerState == 1) {
      digitalWrite(powerToggleRelay, LOW);
      delay(800);
    }
    if (currentPowerState != 1) {
      digitalWrite(muteAudioRelay, LOW);
      currentMuteState = 0;
    }
    digitalWrite(gameSelectRelay0, HIGH);
    currentGameSelected = 1;
    currentPowerState = 1;
    digitalWrite(powerToggleRelay, HIGH);
    digitalWrite(ledSelectRelay, LOW);
    if (analogRead(touchSelectLDR) <= 3000) {
      toggleTouchState();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/setNostalgia", [=]() {
    if (currentGameSelected == 1 && currentPowerState == 1) {
      digitalWrite(powerToggleRelay, LOW);
      delay(800);
    }
    if (currentPowerState != 1) {
      digitalWrite(muteAudioRelay, LOW);
      currentMuteState = 0;
    }
    digitalWrite(gameSelectRelay0, LOW);
    currentGameSelected = 0;
    currentPowerState = 1;
    digitalWrite(powerToggleRelay, HIGH);
    digitalWrite(ledSelectRelay, HIGH);
    if (analogRead(touchSelectLDR) <= 3000) {
      toggleTouchState();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/setGameOff", [=]() {
    digitalWrite(powerToggleRelay, LOW);
    digitalWrite(ledSelectRelay, HIGH);
    currentPowerState = 0;
    digitalWrite(muteAudioRelay, LOW);
    currentMuteState = 0;
    if (analogRead(touchSelectLDR) >= 3000) {
      toggleTouchState();
    }
    if (digitalRead(HDMISwitch1LDR) == LOW) {
      toggleHDMISwitch0State();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/setGameOn", [=]() {
    digitalWrite(powerToggleRelay, HIGH);
    currentPowerState = 1;
    digitalWrite(muteAudioRelay, LOW);
    currentMuteState = 0;
    if (analogRead(touchSelectLDR) <= 3000) {
      toggleTouchState();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/getGameState", [=]() {
    String assembledOutput = "";
    assembledOutput += ((currentPowerState == 0) ? "DISABLED" : (analogRead(touchSelectLDR) <= 3000) ? "OFF" : ((currentGameSelected == 1) ? "BeatStream" : "Nostalgia"));
    server.send(200, "text/plain", assembledOutput);
  });

  server.on("/setTouchGame", [=]() {
    if (analogRead(touchSelectLDR) <= 3000) {
      toggleTouchState();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/setTouchPC", [=]() {
    if (analogRead(touchSelectLDR) >= 3000) {
      toggleTouchState();
    }
    if (digitalRead(HDMISwitch1LDR) == LOW) {
      toggleHDMISwitch0State();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/disableTouch", [=]() {
    digitalWrite(touchResetRelay, LOW);
    server.send(200, "text/plain", "OK");
  });
  server.on("/enableTouch", [=]() {
    digitalWrite(touchResetRelay, HIGH);
    server.send(200, "text/plain", "OK");
  });
  server.on("/resetTouch", [=]() {
    digitalWrite(touchResetRelay, LOW);
    delay(100);
    digitalWrite(touchResetRelay, HIGH);
    delay(100);
    digitalWrite(touchResetRelay, LOW);
    delay(500);
    digitalWrite(touchResetRelay, HIGH);
    server.send(200, "text/plain", "RESET");
  });
  server.on("/getTouchState", [=]() {
    String assembledOutput = "";
    assembledOutput += ((analogRead(touchSelectLDR) >= 3000) ? "GAME" : "PC");
    server.send(200, "text/plain", assembledOutput);
  });

  
  server.on("/getHDMI0State", [=]() {
    String assembledOutput = "";
    assembledOutput += ((digitalRead(HDMISwitch1LDR) == HIGH) ? "PC" : "AUX");
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/setHDMI0-PC", [=]() {
    if (digitalRead(HDMISwitch1LDR) == LOW) {
      toggleHDMISwitch0State();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/setHDMI0-Aux", [=]() {
    if (digitalRead(HDMISwitch1LDR) == HIGH) {
      toggleHDMISwitch0State();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/setHDMI0", [=]() {
    String assembledOutput = "";
    toggleHDMISwitch0State();
    assembledOutput += ((digitalRead(HDMISwitch1LDR) == HIGH) ? "PC" : "AUX");
    server.send(200, "text/plain", assembledOutput);
  });
  
  server.on("/disableSpeakers", [=]() {
    digitalWrite(muteAudioRelay, HIGH);
    currentMuteState = 1;
    server.send(200, "text/plain", "OK");
  });
  server.on("/enableSpeakers", [=]() {
    digitalWrite(muteAudioRelay, LOW);
    currentMuteState = 0;
    server.send(200, "text/plain", "OK");
  });
  server.on("/getSpeakers", [=]() {
    String assembledOutput = "";
    assembledOutput += ((currentMuteState == 0) ? "ENABLED" : "DISABLED");
    server.send(200, "text/plain", assembledOutput);
  });
  
  server.begin();

  bootScreen("BOOT_CPU1");
  xTaskCreatePinnedToCore(
                    cpu2Loop,   /* Task function. */
                    "Watchdog",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */
  delay(250);
  bootScreen("BOOT_CPU2");
  delay(250);
  displayedSec = esp_timer_get_time() / 1000000;;
  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    cpu1Loop,   /* Task function. */
                    "Display",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
  delay(500);
}

void loop() {
  
}


void cpu1Loop( void * pvParameters ) {
  Serial.print("Primary Task running on core ");
  Serial.println(xPortGetCoreID());

  for(;;) {
    runtime();
  }
}
void cpu2Loop( void * pvParameters ) {
  Serial.print("Primary Task running on core ");
  Serial.println(xPortGetCoreID());

  for(;;) {
    checkWiFiConnection();
    server.handleClient();
    sonarOccupancyTest();
  }
}

void runtime() {
  int time_in_sec = esp_timer_get_time() / 1000000;
  int current_time = (time_in_sec - displayedSec) / 3;
  
  if (displayState != 0 && current_time < 1) {
    displayIconMessage(255, true, true, 250, "KONAMI");
    displayState = 0;
  } else if (displayState != 1 && current_time >= 1 && current_time < 2) {
    displayIconDualMessage(1, (currentPowerState == 1), false, (currentPowerState == 1) ? 491 : 490, "System Board", (currentPowerState == 0) ? "Disabled" : "Enabled");
    displayState = 1;
  } else if (displayState != 2 && current_time >= 2 && current_time < 3) {
    displayIconDualMessage(1, (currentMuteState == 1), false, 484, "Speakers", (currentMuteState == 0) ? "Enabled" : "Muted");
    displayState = 2;
  } else if (displayState != 3 && current_time >= 3 && current_time < 4) {
    bool touchState = (analogRead(touchSelectLDR) >= 3000);
    displayIconDualMessage(1, (touchState == true), false, 140, "Touch Screen", (touchState == true) ? "Game" : "PC");
    displayState = 3;
  } else if (displayState != 4 && current_time >= 5 && current_time < 6) {
    displayIconDualMessage(1, false, false, 141, "Game Disk", (currentGameSelected == 1) ? "BeatStream" : "Nostalgia");
    displayState = 4;
  } else if (displayState != 5 && current_time >= 6 && current_time < 7) {
    displayIconDualMessage(1, false, false, 692, "HDMI Input", (digitalRead(HDMISwitch1LDR) == HIGH) ? "PC" : "Aux In");
    displayState = 5;
  } else if (displayState != 6 && current_time >= 7 && current_time < 8) {
    String sonar_text = (occupied_triggered == true) ? "Occupied" : "Unoccupied";
    sonar_text += " (";
    sonar_text += last_sonar_distance;
    sonar_text += "cm )";
    displayIconDualMessage(1, (occupied_triggered == true), false, 365, "Occupancy", sonar_text);
    displayState = 6;
  } else if (current_time >= 8) {
    displayedSec = time_in_sec;
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

