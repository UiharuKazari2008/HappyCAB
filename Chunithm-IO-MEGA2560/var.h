
// ETHERNET PHY DIGITAL SWITCH
// WACCA "0" - PURPLE RED ZIP TIE (Sensor) WHITE (SELECT)
// CHUNI "1" - PURPLE BLUE ZIP TIE (Sensor) WHITE (SELECT)
const int numOfethPorts = 2;
const int ethSensors[2] = { 36, 38 };
const int ethButtons[2] = { 37, 39 };
// Nu CONTROL PORT (KEYCHIP REPLACEMENT)
// UART Serial to Arduino Nano
String nuResponse = "";
// Power Controls
// 0 - PSU and Monitor Enable
// 1 - Nu Power Enable
// 2 - Marquee Enable
// 3 - LED Select
// 4 - Slider RS232
const int numberRelays = 5;
const int controlRelays[5] = { 22, 23, 24, 25, 26 };
// Fan Controller
#define FAN_PWM_PIN_1 9
#define FAN_PWM_PIN_2 10
// Card Reader Communication
bool coinEnable = false;
bool allsOK = false;
bool has_cr_talked = false;
// Display Switch
// 23 - Select
// 15 - State
#define DISPLAY_SELECT_PIN 41
#define DISPLAY_SENSE_PIN 40
bool displayMainState = false;

// Chunithm LED Driver
#define LED_PIN_1 2  // Billboard Left Channel
#define LED_PIN_2 3  // Billboard Right Channel
#define LED_PIN_3 4  // Sidebar Left Channel
#define LED_PIN_4 5  // Sidebar Right Channel

#define NUM_LEDS_1 53
#define NUM_LEDS_2 63
#define NUM_LEDS_3 45

CRGB leds1[NUM_LEDS_1];
CRGB leds2[NUM_LEDS_2];
CRGB leds1_source[NUM_LEDS_1]; 
CRGB leds2_source[NUM_LEDS_2];
CRGB leds1_target[NUM_LEDS_1]; 
CRGB leds2_target[NUM_LEDS_2];
CRGB leds1_backup[NUM_LEDS_1]; 
CRGB leds2_backup[NUM_LEDS_2];
CRGB leds3[NUM_LEDS_3]; 
CRGB leds3_source[NUM_LEDS_3]; 
CRGB leds3_target[NUM_LEDS_3]; 
CRGB leds3_backup[NUM_LEDS_3]; 

Adafruit_NeoPixel BillboardL(NUM_LEDS_1, LED_PIN_1, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel BillboardR(NUM_LEDS_2, LED_PIN_2, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel SidebarL(NUM_LEDS_3, LED_PIN_3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel SidebarR(NUM_LEDS_3, LED_PIN_4, NEO_GRB + NEO_KHZ800);

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
#define BEEPER_PIN 11
unsigned long previousMelodyMillis = 0;
int currentNote = 0;
int melodyPlay = -1;
bool startMelody = false;
int loopMelody = -1;
int pauseBetweenNotes = 0;

// IR Recever
#define IR_REC_PIN 8
// Occupancy and Timer
int requestedPowerState0 = -1;
int defaultInactivityMinTimeout = 45;
int inactivityMinTimeout = 45;
const int shutdownDelayMinTimeout = 5;
unsigned long previousInactivityMillis = 0;
unsigned long previousShutdownMillis = 0;
bool inactivityTimeout = false;

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