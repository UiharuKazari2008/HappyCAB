# [Migrated to BlueSteel](https://yozora.bluesteel.737.jp.net/UiharuKazari2008/XLink-ACC-MCU)

# HappyCAB
Personal total arcade cabinet controller based on the ESP32

## Projects
* NuCtrl (Simple UART disk swap controller)
  * ALLSCtrl uses the same protocal but has ethPhy switching added sense ALLS are more usable when not running games
* Chunithm HappyCAB Cabinet Controller
* WACCA HappyCAB Cabinet Controller
* Nostalgia/BeatStream HappyCAB Cabinet Controller

## Note
Please dont just flash code and not try to see what its doing otherwise your gonna be sad. Only really NuCtrl is a drop in solution

## HappyCAB Features
* Virtual 3 position power switch (Off / Standby / Active)
* Full power controls over all IO boards and lighting
* Toggleable Marquee Lighting
* HTTP API setting of LED pixel colors
* Transistion of LED pixel supported
* I2C Absoute Volume controls
* PWM Chassis Fan controls
* NuCtrl Game SSD Selection and Power OK pin tripping for rebooting
* vALLSCtrl Game Selection and Runtime controls
  * For manageing the runtime of a Unoffical ALLS
* Interaction with "Savior Of Song" Keychip Hardware to select what game keychip is in use
* HDMI Source selection
* Digital Physical Ethernet A-B Switch for Network Selection
* WACCA and Chunithm LED Driver
* Start/Stop/Warning Melody Beeper
* Communication with Sequenzia Kiosk Bridge to control internal general purpose PC
* Looping OLED Display to show status of features

## Note
If you actually want to use it please make a issuse so i can create a updated wiring schematic becuase a lot of wiring is required. And also if your scared of cutting cables then stay away LOL
