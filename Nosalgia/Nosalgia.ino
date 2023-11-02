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
const int ioPowerRelay = 17;
const int powerToggleRelay = 18;
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
const int fanPWM1 = 15;

// Card Reader Communication
//HardwareSerial cardReaderSerial(2);
bool coinEnable = true;
bool has_cr_talked = false;

int currentGameSelected = 0;
int currentPowerState0 = -1;
int currentMuteState = 0;
int triggered_count = 0;
int last_trigger_time = 0;
int last_sonar_check = 0;
int unoccupied_time = 0;
int last_sonar_distance = -1;

int requestedPowerState0 = -1;
const int defaultInactivityMinTimeout = 45;
int inactivityMinTimeout = 45;
const int shutdownDelayMinTimeout = 5;
unsigned long previousInactivityMillis = 0; 
unsigned long previousShutdownMillis = 0; 
bool inactivityTimeout = true;

int displayedSec = 0;
int displayState = -1;
bool occupied = false;
bool occupany_lock = false;
bool occupied_triggered = false;
bool enable_sonar_search = true;
const char *occupied_url = "http://192.168.100.62:3001/button-streamdeck8?event=single-press";
const char *unoccupied_url = "http://192.168.100.62:3001/button-streamdeck8?event=double-press";

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
int currentLEDState = 0;
int currentSliderState = 0;
int currentMarqueeState = 1;
int currentFan1Speed = 128;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

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
        last_trigger_time = time_in_sec;
        previousInactivityMillis = millis();
        inactivityMinTimeout = defaultInactivityMinTimeout + 20;

        HTTPClient http;
        String url = String(occupied_url);
        http.begin(url);
        int httpCode = http.GET();
        http.end();
        if (httpCode == 200) {
          occupied = true;
          occupied_triggered = true;
          kioskModeRequest("Occupied");
        }
      } else if (distance > 0 && distance <= TRIGGER_DISTANCE) {
        triggered_count += 1;
        Serial.print("TRIGGER - Distance: ");
        Serial.print(distance);
        Serial.println(" cm");
        if (occupied == true) {
          last_trigger_time = time_in_sec;
          previousInactivityMillis = millis();
          inactivityMinTimeout = defaultInactivityMinTimeout + 20;

          HTTPClient http;
          String url = String(occupied_url);
          http.begin(url);
          int httpCode = http.GET();
          http.end();
          if (httpCode == 200) {
            occupied = true;
            occupied_triggered = true;
          }
        }
      } else {
        if (occupied_triggered == true) {
          unoccupied_time = time_in_sec;
        }
        triggered_count = 0;
        occupied_triggered = false;
        Serial.print("SEARCH - Distance: ");
        Serial.print(distance);
        Serial.println(" cm");
        if (occupany_lock == false & occupied == true && time_in_sec - unoccupied_time > (UNOCCUPIED_CALLBACK_DELAY_MIN * 60)) {
          HTTPClient http;
          String url = String(unoccupied_url);
          http.begin(url);
          int httpCode = http.GET();
          http.end();
          if (httpCode == 200) {
            occupied = false;
            kioskModeRequest("Unoccupied");
          }
        }
      }
  }
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  inputString.reserve(200); 
  attachedSoftwareCU.reserve(200); 

  bootScreen("HARDWARE");
  pinMode(HDMISwitch1LDR, INPUT);
  pinMode(ioPowerRelay, OUTPUT);
  pinMode(powerToggleRelay, OUTPUT);
  pinMode(touchSelectPin, OUTPUT);
  pinMode(touchResetRelay, OUTPUT);
  pinMode(gameSelectRelay0, OUTPUT);
  pinMode(ledSelectRelay, OUTPUT);
  pinMode(muteAudioRelay, OUTPUT);
  pinMode(HDMISwitch1Key, OUTPUT);
  pinMode(fanPWM1, OUTPUT);

  digitalWrite(HDMISwitch1Key, HIGH);
  digitalWrite(touchSelectPin, HIGH);
  digitalWrite(gameSelectRelay0, LOW);
  digitalWrite(ledSelectRelay, HIGH);
  digitalWrite(touchResetRelay, HIGH);
  digitalWrite(muteAudioRelay, LOW);
  digitalWrite(ioPowerRelay, HIGH);
  digitalWrite(powerToggleRelay, HIGH);

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

  bootScreen("NETWORK");
  checkWiFiConnection();

  bootScreen("DEFAULTS");
  if (analogRead(touchSelectLDR) >= 3000) {
    toggleTouchState();
  }
  if (digitalRead(HDMISwitch1LDR) == LOW) {
    toggleHDMISwitch0State();
  }
  setChassisFanSpeed(40);
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
  kioskModeRequest("StopAll");


  server.on("/display/bottom/pc", [=]() {
    setDisplayState(0,0);
    server.send(200, "text/plain", (digitalRead(HDMISwitch1LDR) == LOW) ? "UNCHANGED" : "OK");
  });
  server.on("/display/bottom/game", [=]() {
    setDisplayState(0,1);
    server.send(200, "text/plain", (digitalRead(HDMISwitch1LDR) == LOW) ? "UNCHANGED" : "OK");
  });
  server.on("/display/bottom/aux", [=]() {
    setDisplayState(0,2);
    server.send(200, "text/plain", (digitalRead(HDMISwitch1LDR) == HIGH) ? "UNCHANGED" : "OK");
  });
  server.on("/display/bottom/switch", [=]() {
    String assembledOutput = "";
    toggleHDMISwitch0State();
    assembledOutput += ((digitalRead(HDMISwitch1LDR) == HIGH) ? "PC" : "AUX");
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/display/bottom", [=]() {
    String assembledOutput = "";
    assembledOutput += ((digitalRead(HDMISwitch1LDR) == HIGH) ? "PC" : "AUX");
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/display/top/pc", [=]() {
    server.send(200, "text/plain", "NOT_SETUP");
  });
  server.on("/display/top/game", [=]() {
    server.send(200, "text/plain", "NOT_SETUP");
  });
  server.on("/display/top/switch", [=]() {
    server.send(200, "text/plain", "NOT_SETUP");
  });
  server.on("/display/top", [=]() {
    server.send(200, "text/plain", "NOT_SETUP");
  });

  server.on("/select/game/pan", [=]() {
    if (currentGameSelected != 0) {
      setGameDisk(0);
      server.send(200, "text/plain", (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/select/game/nbt", [=]() {
    if (currentGameSelected != 1) {
      setGameDisk(1);
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
  server.on("/power/on/pan", [=]() {
    server.send(200, "text/plain", (currentPowerState0 == 1 && currentGameSelected == 0) ? "UNCHNAGED" : (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    setGameDisk(0);
    setGameOn();
  });
  server.on("/power/on/nbt", [=]() {
    server.send(200, "text/plain", (currentPowerState0 == 1 && currentGameSelected == 1) ? "UNCHNAGED" : (currentPowerState0 == 1) ? "REBOOTING" : "OK");
    setGameDisk(1);
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
  server.on("/game_power_select", [=]() {
    server.send(200, "text/plain", (requestedPowerState0 >= 0) ? "Warning" : ((currentPowerState0 == 1) ? getGameSelect() : "Disabled"));
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

  server.on("/occupany/lock", [=]() {
    if (occupany_lock == false) {
      lockOccupancy();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/occupany/unlock", [=]() {
    if (occupany_lock == true) {
      occupany_lock = false;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "UNCHANGED");
    }
  });
  server.on("/occupany", [=]() {
    String assembledOutput = "";
    if (occupany_lock == true) {
      assembledOutput += "LOCKED";
    }
    if (occupied_triggered == true) {
      assembledOutput += "OCCUPIED";
    } else {
      assembledOutput += "UNOCCUPIED";
    }
    server.send(200, "text/plain", assembledOutput);
  });
  server.on("/occupany/distance", [=]() {
    String assembledOutput = "";
    if (occupany_lock == true) {
      assembledOutput += "LOCKED:";
    } else {
      if (occupied_triggered == true) {
        assembledOutput += "OCCUPIED:";
      } else {
        assembledOutput += "UNOCCUPIED:";
      }
    }
    assembledOutput += last_sonar_distance;
    assembledOutput += "CM";
    server.send(200, "text/plain", assembledOutput);
  });
  
  server.on("/touch/game", [=]() {
    setTouchControl(true);
    server.send(200, "text/plain", "OK");
  });
  server.on("/touch/pc", [=]() {
    setTouchControl(false);
    server.send(200, "text/plain", "OK");
  });
  server.on("/touch/disable", [=]() {
    digitalWrite(touchResetRelay, LOW);
    server.send(200, "text/plain", "OK");
  });
  server.on("/touch/enable", [=]() {
    digitalWrite(touchResetRelay, HIGH);
    server.send(200, "text/plain", "OK");
  });
  server.on("/touch/reset", [=]() {
    digitalWrite(touchResetRelay, LOW);
    delay(100);
    digitalWrite(touchResetRelay, HIGH);
    delay(100);
    digitalWrite(touchResetRelay, LOW);
    delay(500);
    digitalWrite(touchResetRelay, HIGH);
    server.send(200, "text/plain", "RESET");
  });
  server.on("/touch", [=]() {
    String assembledOutput = "";
    assembledOutput += ((analogRead(touchSelectLDR) >= 3000) ? "GAME" : "PC");
    server.send(200, "text/plain", assembledOutput);
  });
  
  server.on("/speakers/disable", [=]() {
    digitalWrite(muteAudioRelay, HIGH);
    currentMuteState = 1;
    server.send(200, "text/plain", "OK");
  });
  server.on("/speakers/enable", [=]() {
    digitalWrite(muteAudioRelay, LOW);
    currentMuteState = 0;
    server.send(200, "text/plain", "OK");
  });
  server.on("/speakers", [=]() {
    String assembledOutput = "";
    assembledOutput += ((currentMuteState == 0) ? "ENABLED" : "DISABLED");
    server.send(200, "text/plain", assembledOutput);
  });
  
  server.begin();

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
  bootScreen("BOOT_CPU1");
  xTaskCreatePinnedToCore(
                    cpu2Loop,   /* Task function. */
                    "Watchdog",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */
  delay(101);
  bootScreen("BOOT_CPU2");
  delay(101);
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
  // Handle Display Handover
  //displayMainState = (digitalRead(displayMainLDR) == LOW);
  if (pending_release_display == true && coinEnable == true) {
    pending_release_display = false;
    kioskModeRequest("GameRunning");
    delay(6000);
    setDisplayState(0,1);
    setDisplayState(1,1);
  }
  // Drive LED Animations

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
  if (currentPowerState0 == 1 && requestedPowerState0 > -1) {
    const float position = ((millis() - previousShutdownMillis) / 60000);
    int val = map(position, 0, 4, loopMelody, 0) * 1000;
    if (val <= 1 && currentNote != val) {      
      Serial.println("");
      Serial.print("AUDIO_PLAY::SHUTDOWN");
      Serial.println("");
      currentNote = val;
    } else if (currentNote != val) {      
      Serial.println("");
      Serial.println("AUDIO_PLAY::SHUTDOWN::" + String(val));
      Serial.println("");
      currentNote = val;
    }
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

void runtime() {
  int time_in_sec = esp_timer_get_time() / 1000000;
  int current_time = (time_in_sec - displayedSec) / 3;
  
  if (displayState != 1 && currentPowerState0 == -1) {
      const String power = getPowerAuth();
      displayIconDualMessage(1, false, false, (power == "Active") ? 491 : 490, "System Power", power);
      displayState = 1;
  } else if (currentPowerState0 == -1) {
    delay(500);
  } else if (currentPowerState0 != -1) {
    if (displayState != 0 && current_time < 1) {
      displayIconMessage(255, true, true, 250, "KONAMI");
      displayState = 0;
    } else if (displayState != 1 && current_time >= 1 && current_time < 2) {
      const String power = getPowerAuth();
      displayIconDualMessage(1, (power == "Active"), false, (power == "Active") ? 491 : 490, "System Power", power);
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
      displayIconDualMessage(1, false, false, 692, "Top Display", (digitalRead(HDMISwitch1LDR) == HIGH) ? "PC" : "Aux In");
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


String getGameSelect() {
  String assembledOutput = "";
  switch (currentGameSelected) {
    case 0:
      assembledOutput = "Nostalgia";
      break;
    case 1:
      assembledOutput = "BeatStream";
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
  if (currentMuteState == 1) {
    messageIcon = 279;
    messageText = "Volume Muted";
    invertMessage = true;
  } else {
    messageIcon = 277;
    messageText = "Volume Enabled";
    invertMessage = false;
  }
  isJpnMessage = false;
  brightMessage = 255;
  timeoutMessage = 10;
  typeOfMessage = 1;
}

void defaultLEDState() {
  // FastLED.setBrightness(0);
  // for (int i = 0; i < NUM_LEDS; i++) {
  //   leds[i] = CRGB::Black;
  // }
  // FastLED.show();
  // delay(50);
  // FastLED.show();
}
void standbyLEDState() {
  // FastLED.setBrightness(64);
  // int hue = 37;
  // int rowCount = 7;
  // for (int i = 0; i < NUM_LEDS; i++) {
  //   leds[i] = CHSV(map(hue, 0,360,0,255), 255, 255 - (rowCount * 10));
  //   rowCount--;
  //   if (rowCount <= -1) {
  //     rowCount = 7;
  //   }
  // }
  // FastLED.show();
  // delay(50);
  // FastLED.show();
}
void startingLEDState() {
  // setLEDControl(true);
  // targetBrightness = 255;
  // numSteps = 4.0 * 33.2;
  // transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
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
  // pending_release_leds = true;
  // animation_state = -1;
  // animation_mode = 1;
  // currentStep = 0;
  // transition_leds = true;
}
void shuttingDownLEDState(int state) {
  kioskModeRequest("WarningGame");
  setLEDControl(true);
  // targetBrightness = 255;
  // numSteps = 4.0 * 33.2;
  // transition_interval = (unsigned long)(1000.0 * 4.0 / (float)numSteps);
  // int hue = 0;
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

void setMasterPowerOn() {
  if (currentPowerState0 == -1) {
    if (currentMuteState == 0) {
      digitalWrite(muteAudioRelay, LOW);
    }
    setIOPower(true);
    delay(250);
    setLEDControl(true);
    kioskModeRequest("StartStandby");
    resetMarqueeState();
    setChassisFanSpeed(50);
    delay(500);
    defaultLEDState();
    standbyLEDState();
    setDisplayState(0,0);
    setDisplayState(1,0);
    setTouchControl(false);
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
  digitalWrite(muteAudioRelay, HIGH);
  kioskModeRequest("StopAll");
  setChassisFanSpeed(0);
  setLEDControl(true);
  setMarqueeState(false, false);
  setTouchControl(false);
  if (currentPowerState0 == 1) {
    delay(500);
    setSysBoardPower(false);
    delay(200);  
    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    kioskAudioPlayback("GAME_OFF");
  } else {
    melodyPlay = -1;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = false;
    kioskAudioPlayback("STOP");
  }
  delay(500);
  setIOPower(false);
  setDisplayState(0,0);
  setDisplayState(1,0);
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
  if (currentPowerState0 == -1) {
    setIOPower(true);
    delay(1000);
  }
  if (currentPowerState0 != 1) {
    inactivityMinTimeout = defaultInactivityMinTimeout + 5;
    previousInactivityMillis = millis();
    startLoadingScreen();
    setChassisFanSpeed(100);
    startingLEDState();
    setMarqueeState(true, false);
    setSysBoardPower(true);
    setDisplayState(0,1);
    setDisplayState(1,1);
    setTouchControl(true);
    if (currentMuteState == 0) {
      digitalWrite(muteAudioRelay, LOW);
    }
    
    melodyPlay = 0;
    loopMelody = -1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    kioskAudioPlayback("GAME_START");
    
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

    setDisplayState(0,0);
    setDisplayState(1,0);
    setChassisFanSpeed(50);
    kioskModeRequest("StartStandby");
    setLEDControl(true);
    setTouchControl(false);
    delay(250);
    standbyLEDState();
    resetMarqueeState();
    setSysBoardPower(false);
    if (currentMuteState == 0) {
      digitalWrite(muteAudioRelay, LOW);
    }
    
    loopMelody = -1;
    melodyPlay = 1;
    currentNote = 0;
    previousMelodyMillis = 0;
    startMelody = true;
    kioskAudioPlayback("GAME_OFF");
    
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
  if (currentPowerState0 == 1) {
    setGameOff();
  } else {
    setMasterPowerOn();
  }
}

void setSysBoardPower(bool state) {
  digitalWrite(powerToggleRelay, (state == true) ? LOW : HIGH);
  delay((state == true) ? 200 : 500);
}
// TODO: Add LED Control select relay
void setLEDControl(bool state) {
  //digitalWrite(controlRelays[3], (state == true) ? LOW : HIGH);
  //currentLEDState = (state == true) ? 0 : 1;
}
void setTouchControl(bool state) {
  if (state == false) {
    if (analogRead(touchSelectLDR) >= 3000) {
      toggleTouchState();
    }
  } else {
    if (analogRead(touchSelectLDR) <= 3000) {
      toggleTouchState();
    }
  }
  digitalWrite(touchResetRelay, HIGH);
}
void setIOPower(bool state) {
  digitalWrite(ioPowerRelay, (state == true) ? LOW : HIGH);
}
// TODO: Add 12v LED Driver
void resetMarqueeState() {
  //digitalWrite(relayPins[4], (currentMarqueeState == 1) ? HIGH : LOW);
}
// TODO: Add 12v LED Driver
void setMarqueeState(bool state, bool save) {
  //if (currentPowerState0 != -1) {
  //  digitalWrite(relayPins[4], (state == true) ? HIGH : LOW);
  //}
  if (save == true) {
    currentMarqueeState = (state == true) ? 1 : 0;
  }
}
// UPDATE
void setGameDisk(int number) {
  if (currentGameSelected != number) {
    if (currentPowerState0 == 1) {
      if (currentMuteState == 0) {
        digitalWrite(muteAudioRelay, HIGH);
        delay(100);
      }
      setSysBoardPower(false);
      delay(800);
    }
    switch (number) {
      case 0:
        // PAN
        digitalWrite(gameSelectRelay0, LOW);
        digitalWrite(ledSelectRelay, HIGH);
        break;
      case 1:
        // NBT
        digitalWrite(gameSelectRelay0, HIGH);
        digitalWrite(ledSelectRelay, LOW);
        break;
      default:
        break;
    }
    currentGameSelected = number;
    if (currentPowerState0 == 1) {
      setSysBoardPower(true);
      setTouchControl(true);
      setDisplayState(0,1);
      setDisplayState(1,1);
      if (currentMuteState == 0) {
        delay(2000);
        digitalWrite(muteAudioRelay, LOW);
      }
    }
  } 
}
// TODO: Add PWM Fans to top of chassis
void setChassisFanSpeed(int speed) {
  currentFan1Speed = map(speed, 0, 100, 0, 255);
  analogWrite(fanPWM1, currentFan1Speed);
}
void setDisplayState(int display, int state) {
  switch (display) {
    case 0:
      switch (state) {
        case 0:
          // Impliment Top Display Switch for PC and Game
        case 1:
          if (digitalRead(HDMISwitch1LDR) == LOW) {
            toggleHDMISwitch0State();
          }
          break;
        case 2:
          if (digitalRead(HDMISwitch1LDR) == HIGH) {
            toggleHDMISwitch0State();
          }
          break;
        default:
          break;    
      }
      break;
    default:
      break;
  }
}
void resetState() {
  setLEDControl((currentPowerState0 != 1));
  if (currentLEDState == 0) {
    delay(800);
    setLEDControl(false);
  }
  setDisplayState(0,1);
  setDisplayState(1,1);
  kioskModeRequest("ResetState");
  startMelody = false;
  melodyPlay = -1;
  loopMelody = -1;
  currentNote = 0;
  previousMelodyMillis = 0;
  kioskAudioPlayback("STOP");

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
void lockOccupancy() {
  if (occupany_lock == false) {
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
      occupany_lock = true;
    }
  }
}


void kioskAudioPlayback(String audio) {
  Serial.println("");
  Serial.println("AUDIO_PLAY::" + audio);
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
            if (header == "OCCUPANCY") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "LOCK") {
                lockOccupancy();
              } else if (valueString == "UNLOCK") {
                occupany_lock = false;
              } else if (valueString == "?DISTANCE") {
                Serial.println("R::" + String(last_sonar_distance));
              } else if (valueString == "?") {
                String assembledOutput = "";
                if (occupany_lock == true) {
                  assembledOutput += "LOCKED";
                } else {
                  if (occupied_triggered == true) {
                    assembledOutput += "OCCUPIED";
                  } else {
                    assembledOutput += "UNOCCUPIED";
                  }
                }
                Serial.println("R::" + assembledOutput);
              }
            } else if (header == "POWER_SWITCH") {
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
            } else if (header == "DISPLAY_SELECT") {
              int commIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String commString = receivedMessage.substring(headerIndex + 2, commIndex);
              if (commString == "?") {
                Serial.println("R::NO_DATA");
              } else if (commString == "SWITCH") {
                toggleHDMISwitch0State();
              } else if (commString == "BOTTOM") {
                int valueIndex = receivedMessage.indexOf("::", commIndex + 2);
                String valueString = receivedMessage.substring(commIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                switch (valueInt) {
                  case 0:
                    setDisplayState(0,0);
                    break;
                  case 1:
                    setDisplayState(0,1);
                    break;
                  case 2:
                    setDisplayState(0,2);
                    break;
                  default:
                    break;
                }
              } else if (commString == "TOP") {
                int valueIndex = receivedMessage.indexOf("::", commIndex + 2);
                String valueString = receivedMessage.substring(commIndex + 2, valueIndex);
                int valueInt = valueString.toInt();
                switch (valueInt) {
                  case 0:
                    setDisplayState(1,0);
                    break;
                  case 1:
                    setDisplayState(1,1);
                    break;
                  case 2:
                    setDisplayState(1,2);
                    break;
                  default:
                    break;
                }
              }
            } else if (header == "DISK_SELECT") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "?") {
                Serial.println("R::" + getGameSelect());
              } else {
                int valueInt = valueString.toInt();
                switch (valueInt) {
                  case 0:
                    setGameDisk(0);
                    break;
                  case 1:
                    setGameDisk(1);
                    break;
                  default:
                    break;
                }
              }
            } else if (header == "POWER_SELECT") {
              int valueIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String valueString = receivedMessage.substring(headerIndex + 2, valueIndex);
              if (valueString == "?") {
                Serial.println("R::" + getGameSelect());
              } else {
                int valueInt = valueString.toInt();
                switch (valueInt) {
                  case 0:
                    setGameDisk(0);
                    break;
                  case 1:
                    setGameDisk(1);
                    break;
                  default:
                    break;
                }
                setGameOn();
              }
            } else if (header == "VOLUME") {
              int optionIndex = receivedMessage.indexOf("::", headerIndex + 2);
              String optionString = receivedMessage.substring(headerIndex + 2, optionIndex);
              if (optionString == "?") {

              } else if (optionString == "MUTE_ON") {
                digitalWrite(muteAudioRelay, HIGH);
                currentMuteState = 1;
                displayVolumeMessage();
              } else if (optionString == "MUTE_OFF") {
                digitalWrite(muteAudioRelay, LOW);
                currentMuteState = 0;
                displayVolumeMessage();
              } else if (optionString == "MUTE") {
                digitalWrite(muteAudioRelay, (currentMuteState == 1) ? LOW : HIGH);
                currentMuteState = ((currentMuteState == 1) ? 0 : 1);
                displayVolumeMessage();
              }
            } else if (header == "PING") {
              Serial.println("R::PONG");
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