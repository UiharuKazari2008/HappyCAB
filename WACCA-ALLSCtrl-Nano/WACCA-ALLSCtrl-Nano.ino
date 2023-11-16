bool system_power_state = 0;
int selected_game_disk = 0;

const int num_of_disks = 3;
const int game_disks[3] = { 2, 3, 4 };
const int power_en_pin = 5;

const int eth_select_state = 7;
const int eth_select_btn = 6;

const int motherboard_pwr_btn = 8;
const int motherboard_rst_btn = 9;
const int motherboard_pwr_led = 10;
const int standby_led = 11;

void setup() {
  Serial.begin(4800);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(standby_led, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(power_en_pin, OUTPUT);
  digitalWrite(power_en_pin, LOW);
  pinMode(eth_select_state, INPUT);
  pinMode(eth_select_btn, OUTPUT);
  digitalWrite(eth_select_btn, HIGH);
  pinMode(motherboard_pwr_btn, OUTPUT);
  pinMode(motherboard_rst_btn, OUTPUT);
  pinMode(motherboard_pwr_led, INPUT);
  digitalWrite(motherboard_pwr_btn, HIGH);
  digitalWrite(motherboard_rst_btn, HIGH);
  digitalWrite(standby_led, LOW);
  for (int i=0; i < num_of_disks; i++) {
    pinMode(game_disks[i], OUTPUT);
    digitalWrite(game_disks[i], LOW);
  }
  Serial.println("I::R");
}

int sendDelay = 100;
int previousSendMillis = 0;
int led_state = LOW;
int blinkNum = 0;
int blinkDelay = 250;
int blinkWaitDelay = 2000;
int previousBlinkMillis = 0;
void loop() {
  receivedMessage();
  int currentMillis = millis();
  if (currentMillis - previousSendMillis >= sendDelay) {
    previousSendMillis = currentMillis;
    sendStatus();
  }
  if (system_power_state == 1) {
    if (blinkNum <= ((selected_game_disk * 2) + 1) && currentMillis - previousBlinkMillis >= blinkDelay) {
      previousBlinkMillis = currentMillis;
      blinkNum++;
      if (blinkNum == selected_game_disk * 2) {
        led_state = LOW;
      } else {
        led_state = (led_state == LOW) ? HIGH : LOW;
      }
      digitalWrite(LED_BUILTIN, led_state);
    } else if (blinkNum >= ((selected_game_disk * 2) + 1) && currentMillis - previousBlinkMillis >= blinkWaitDelay) {
      previousBlinkMillis = currentMillis;
      blinkNum = 0;
      led_state = LOW;
      digitalWrite(LED_BUILTIN, LOW);
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}

int sentState = 0;
void sendStatus() {
  switch (sentState) {
    case 0:
      Serial.println("PS::" + String(system_power_state));
      break;
    case 1:
      Serial.println("BS::" + String(digitalRead(motherboard_pwr_led) == LOW));
      break;
    case 2:
      Serial.println("DS::" + String(selected_game_disk));
      break;
    case 3:
      Serial.println("NS::" + String((digitalRead(eth_select_state) == LOW) ? "0" : "1"));
      break;
    default:
      break;
  }
  sentState++;
  if (sentState > 3) {
    sentState = 0;
  }
};
void receivedMessage() {
  static String receivedMessage = "";
  char c;
  bool messageStarted = false;

  if (Serial.available()) {
    c = Serial.read();
    if (c == '\n') {
      if (receivedMessage != "") {
        handleCRMessage(receivedMessage);
      }
      receivedMessage = "";
    } else {
      receivedMessage += c;
    }
  }
};
void handleCRMessage(String inputString) {
  int delimiterIndex = inputString.indexOf("::");
  if (delimiterIndex != -1) {
    int headerIndex = inputString.indexOf("::");
    String header = inputString.substring(0, headerIndex);
    if (header == "PS") {
      int valueIndex = inputString.indexOf("::", headerIndex + 2);
      String valueString = inputString.substring(headerIndex + 2, valueIndex);
      int stateNumber = valueString.toInt();
      if (stateNumber == 1) {
        String response = setPowerOn();
        Serial.println("R::" + response);
      } else if (stateNumber == 0) {
        String response = setPowerOff();
        Serial.println("R::" + response);
      } else if (stateNumber == 128) {
        if (system_power_state == 1) {
          String response0 = setPowerOff();
          String response1 = setPowerOn();
          Serial.println("R::" + response1);
        } else {
          Serial.println("R::NC");
        }
      } else {
        Serial.println("R::INVD");
      }  
    } else if (header == "DS") {
      int valueIndex = inputString.indexOf("::", headerIndex + 2);
      String valueString = inputString.substring(headerIndex + 2, valueIndex);
      int diskNumber = valueString.toInt();
      if (diskNumber >= 0 && diskNumber <= (num_of_disks - 1)) {
        String response = setGameDisk(diskNumber);
        Serial.println("R::" + response);
      } else {
        Serial.println("R::INVD");
      }
    } else if (header == "P") {
      Serial.println("R::P");
    }
  }
}

String setPowerOn() {
  if (selected_game_disk < 10) {
    if (system_power_state != 1) {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("DBG::NET_SET");
      selectNetwork((selected_game_disk == 2));
      Serial.println("DBG::DISK_SET");
      digitalWrite(game_disks[selected_game_disk], HIGH);
      Serial.println("DBG::POWER_EN");
      digitalWrite(power_en_pin, HIGH);
      digitalWrite(standby_led, (selected_game_disk == 2));
      Serial.println("DBG::POWER_CHECK");
      ensurePowerOn();
      Serial.println("DBG::DONE");
      system_power_state = 1;
      blinkNum = 0;
      return "OK";
    } else {
      return "NC";
    }
  } else {
      digitalWrite(power_en_pin, LOW);
      for (int i=0; i < num_of_disks; i++) {
        digitalWrite(game_disks[i], LOW);
      }
      system_power_state = 0;
      return "INHIBIT";
  }
}
String setPowerOff() {
  if (system_power_state != 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("DBG::SHUTDOWN_REQ");
    handleShutdown();
    Serial.println("DBG::EN_RELEASE");
    digitalWrite(power_en_pin, LOW);
    delay(450);
    Serial.println("DBG::DISK_RELEASE");
    for (int i=0; i < num_of_disks; i++) {
      digitalWrite(game_disks[i], LOW);
    }
    digitalWrite(standby_led, LOW);
    Serial.println("DBG::DONE");
    system_power_state = 0;
    return "OK";
  } else {
    return "NC";
  }
}
void handleShutdown() {
  // Enhanced Standby, Proper Shutdown
  if (selected_game_disk == 2) {
    digitalWrite(motherboard_pwr_btn, HIGH);
    digitalWrite(motherboard_pwr_btn, LOW);
    delay(200);
    digitalWrite(motherboard_pwr_btn, HIGH);
    while (digitalRead(motherboard_pwr_led) == LOW) {
      Serial.println("DBG::POWER_WAIT");
      delay(100);
    }
    delay(500);
  }
}
String resetPower(int disk_num) {
  if (system_power_state != 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    if (selected_game_disk == 2) {
      Serial.println("DBG::SHUTDOWN_REQ");
      handleShutdown();
    } else {
      Serial.println("DBG::RESET_REQ");
      digitalWrite(motherboard_rst_btn, HIGH);
      digitalWrite(motherboard_rst_btn, LOW);
      delay(500);
      digitalWrite(motherboard_rst_btn, HIGH);
    }
    Serial.println("DBG::DISK_RELEASE");
    for (int i=0; i < num_of_disks; i++) {
      digitalWrite(game_disks[i], LOW);
    }
    selected_game_disk = disk_num;
    Serial.println("DBG::DISK_SET");
    digitalWrite(game_disks[selected_game_disk], HIGH);
    Serial.println("DBG::NET_SET");
    selectNetwork((selected_game_disk == 2));
    Serial.println("DBG::POWER_CHECK");
    digitalWrite(standby_led, (selected_game_disk == 2));
    ensurePowerOn();
    return "OK";
  } else {
    return setPowerOn();
  }
}
String setGameDisk(int disk_num) {
  if (selected_game_disk != disk_num) {
    if (system_power_state == 1 && selected_game_disk < 10) {
      String power_rst = resetPower(disk_num);
      return "RST";
    } else {
      selected_game_disk = disk_num;
      return "OK";
    }
  } else {
    return "NC";
  }
}
void selectNetwork(bool state) {
  if (state == (digitalRead(eth_select_state) == LOW)) {
    digitalWrite(eth_select_btn, HIGH);
    digitalWrite(eth_select_btn, LOW);
    delay(250);
    digitalWrite(eth_select_btn, HIGH);
    delay(200);
  }
}
void ensurePowerOn() {
  delay(1000);
  while (digitalRead(motherboard_pwr_led) == HIGH) {
    Serial.println("DBG::POWER_ON_REQ");
    digitalWrite(motherboard_pwr_btn, HIGH);
    digitalWrite(motherboard_pwr_btn, LOW);
    delay(500);
    digitalWrite(motherboard_pwr_btn, HIGH);
    delay(1000);
  }
}