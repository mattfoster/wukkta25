// Macropad with BLE Keyboard for XIAO ESP32-C3
// Display shows images on button press, LEDs light up
// Connect via Bluetooth "MacroPad" to use as a 4-key media controller

#include <TFT_eSPI.h>
#include <EasyButton.h>

#undef local
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#define local static

#include <PNGdec.h>
#include "NotoSansBold15.h"
#include "screwdriver.h"

// HID Report Descriptor for keyboard with media keys
static const uint8_t hidReportDescriptor[] = {
  // Keyboard
  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x06,  // Usage (Keyboard)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x01,  //   Report ID (1)
  0x05, 0x07,  //   Usage Page (Key Codes)
  0x19, 0xE0,  //   Usage Minimum (224)
  0x29, 0xE7,  //   Usage Maximum (231)
  0x15, 0x00,  //   Logical Minimum (0)
  0x25, 0x01,  //   Logical Maximum (1)
  0x75, 0x01,  //   Report Size (1)
  0x95, 0x08,  //   Report Count (8)
  0x81, 0x02,  //   Input (Data, Variable, Absolute)
  0x95, 0x01,  //   Report Count (1)
  0x75, 0x08,  //   Report Size (8)
  0x81, 0x01,  //   Input (Constant)
  0x95, 0x06,  //   Report Count (6)
  0x75, 0x08,  //   Report Size (8)
  0x15, 0x00,  //   Logical Minimum (0)
  0x25, 0x65,  //   Logical Maximum (101)
  0x05, 0x07,  //   Usage Page (Key Codes)
  0x19, 0x00,  //   Usage Minimum (0)
  0x29, 0x65,  //   Usage Maximum (101)
  0x81, 0x00,  //   Input (Data, Array)
  0xC0,        // End Collection

  // Consumer Control (Media Keys)
  0x05, 0x0C,  // Usage Page (Consumer)
  0x09, 0x01,  // Usage (Consumer Control)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x02,  //   Report ID (2)
  0x15, 0x00,  //   Logical Minimum (0)
  0x25, 0x01,  //   Logical Maximum (1)
  0x75, 0x01,  //   Report Size (1)
  0x95, 0x10,  //   Report Count (16)
  0x0A, 0xB5, 0x00,  // Usage (Scan Next Track)
  0x0A, 0xB6, 0x00,  // Usage (Scan Previous Track)
  0x0A, 0xB7, 0x00,  // Usage (Stop)
  0x0A, 0xCD, 0x00,  // Usage (Play/Pause)
  0x0A, 0xE2, 0x00,  // Usage (Mute)
  0x0A, 0xE9, 0x00,  // Usage (Volume Up)
  0x0A, 0xEA, 0x00,  // Usage (Volume Down)
  0x0A, 0x23, 0x02,  // Usage (Browser Home)
  0x0A, 0x94, 0x01,  // Usage (My Computer)
  0x0A, 0x92, 0x01,  // Usage (Calculator)
  0x0A, 0x2A, 0x02,  // Usage (Browser Favorites)
  0x0A, 0x21, 0x02,  // Usage (Browser Search)
  0x0A, 0x26, 0x02,  // Usage (Browser Stop)
  0x0A, 0x24, 0x02,  // Usage (Browser Back)
  0x0A, 0x83, 0x01,  // Usage (Media Select)
  0x0A, 0x8A, 0x01,  // Usage (Email)
  0x81, 0x02,  //   Input (Data, Variable, Absolute)
  0xC0         // End Collection
};

// Media key bit positions in the 16-bit consumer report
#define MEDIA_NEXT       0x0001
#define MEDIA_PREV       0x0002
#define MEDIA_STOP       0x0004
#define MEDIA_PLAY_PAUSE 0x0008
#define MEDIA_MUTE       0x0010
#define MEDIA_VOL_UP     0x0020
#define MEDIA_VOL_DOWN   0x0040

NimBLEHIDDevice* hid;
NimBLECharacteristic* inputKeyboard;
NimBLECharacteristic* inputMedia;
bool deviceConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    Serial.println("BLE Connected");
  }
  
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.println("BLE Disconnected");
    NimBLEDevice::startAdvertising();
  }
};

uint8_t debug = 1;
uint8_t displayMode = 0;
volatile bool playingAnimation = false;
volatile int animationStep = 0;

bool displayDirty = true;
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 50

int activeLED = -1;
unsigned long ledStartTime = 0;
#define LED_DURATION_MS 300

bool lightDisplayMode = false;
int lightDisplayStep = 0;
unsigned long lastLightDisplayUpdate = 0;
#define LIGHT_DISPLAY_INTERVAL 100

volatile bool buttonActions[4] = {false, false, false, false};
int currentDisplayImage = -1;
unsigned long imageDisplayStart = 0;
#define IMAGE_DISPLAY_DURATION 2000

hw_timer_t* animationTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

PNG png;
#define MAX_IMAGE_WIDTH 240
int16_t xpos = 0;
int16_t ypos = 0;

const byte* const IMAGES[] = {screwdriver, coin, camera, puffa, sw, plant};
const int IMAGE_SIZES[] = {sizeof(screwdriver), sizeof(coin), sizeof(camera), sizeof(puffa), sizeof(sw), sizeof(plant)};
const char* const IMAGE_NAMES[] = {"screwdriver", "coin", "camera", "puffa", "sw", "plant"};
#define NUM_IMAGES 6

const char* BUTTON_LABELS[] = {"Play/Pause", "Next", "Vol Up", "Vol Down"};
const uint16_t BUTTON_MEDIA_KEYS[] = {MEDIA_PLAY_PAUSE, MEDIA_NEXT, MEDIA_VOL_UP, MEDIA_VOL_DOWN};

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite face = TFT_eSprite(&tft);

#define CLOCK_R       127.0f / 2.0f
#define FACE_W        CLOCK_R * 2 + 1
#define FACE_H        CLOCK_R * 2 + 1
#define CX tft.width()  / 2.0f
#define CY tft.height() / 2.0f

#define LED_PIN_A D3
#define LED_PIN_B D4
#define LED_PIN_C D5
 
#define SW_PIN_A D7
#define SW_PIN_B D6
#define SW_PIN_C D9
#define SW_PIN_D D2

const uint8_t SW_PINS[] = {SW_PIN_A, SW_PIN_B, SW_PIN_C, SW_PIN_D};
#define NUM_BUTTONS 4

EasyButton button1(SW_PIN_A);
EasyButton button2(SW_PIN_B);
EasyButton button3(SW_PIN_C);
EasyButton button4(SW_PIN_D);
EasyButton* buttons[] = {&button1, &button2, &button3, &button4};

void button1ISR() { button1.read(); }
void button2ISR() { button2.read(); }
void button3ISR() { button3.read(); }
void button4ISR() { button4.read(); }

void IRAM_ATTR lightLED(int ledNum) {
  switch(ledNum) {
    case 0:
      pinMode(LED_PIN_A, OUTPUT);
      pinMode(LED_PIN_B, OUTPUT);
      pinMode(LED_PIN_C, INPUT);
      digitalWrite(LED_PIN_A, HIGH);
      digitalWrite(LED_PIN_B, LOW);
      digitalWrite(LED_PIN_C, LOW);
      break;
    case 1:
      pinMode(LED_PIN_A, INPUT);
      pinMode(LED_PIN_B, OUTPUT);
      pinMode(LED_PIN_C, OUTPUT);
      digitalWrite(LED_PIN_A, LOW);
      digitalWrite(LED_PIN_B, LOW);
      digitalWrite(LED_PIN_C, HIGH);
      break;
    case 2:
      pinMode(LED_PIN_A, OUTPUT);
      pinMode(LED_PIN_B, OUTPUT);
      pinMode(LED_PIN_C, INPUT);
      digitalWrite(LED_PIN_A, LOW);
      digitalWrite(LED_PIN_B, HIGH);
      digitalWrite(LED_PIN_C, LOW);
      break;
    case 3:
      pinMode(LED_PIN_A, INPUT);
      pinMode(LED_PIN_B, OUTPUT);
      pinMode(LED_PIN_C, OUTPUT);
      digitalWrite(LED_PIN_A, LOW);
      digitalWrite(LED_PIN_B, HIGH);
      digitalWrite(LED_PIN_C, LOW);
      break;
  }
}

void IRAM_ATTR turnOffAllLEDs() {
  pinMode(LED_PIN_A, INPUT);
  pinMode(LED_PIN_B, INPUT);
  pinMode(LED_PIN_C, INPUT);
}

void IRAM_ATTR onAnimationTimer() {
  if (playingAnimation) {
    int led = animationStep % 4;
    lightLED(led);
    animationStep++;
    if (animationStep >= 12) {
      playingAnimation = false;
      turnOffAllLEDs();
    }
  }
}

void sendMediaKey(uint16_t key) {
  if (!deviceConnected || !inputMedia) return;
  
  uint8_t report[2] = {(uint8_t)(key & 0xFF), (uint8_t)(key >> 8)};
  inputMedia->setValue(report, 2);
  inputMedia->notify();
  
  delay(10);
  
  uint8_t release[2] = {0, 0};
  inputMedia->setValue(release, 2);
  inputMedia->notify();
}

void handleButtonPress(int buttonIndex) {
  Serial.printf("Button %d pressed\n", buttonIndex + 1);
  
  if (!playingAnimation) {
    activeLED = buttonIndex;
    ledStartTime = millis();
    lightLED(buttonIndex);
  }
  
  currentDisplayImage = buttonIndex % NUM_IMAGES;
  imageDisplayStart = millis();
  displayDirty = true;
  
  if (deviceConnected) {
    sendMediaKey(BUTTON_MEDIA_KEYS[buttonIndex]);
    Serial.printf("Sent: %s\n", BUTTON_LABELS[buttonIndex]);
  } else {
    Serial.println("BLE not connected");
  }
}

void onButton1Pressed() { buttonActions[0] = true; }
void onButton2Pressed() { buttonActions[1] = true; }
void onButton3Pressed() { buttonActions[2] = true; }
void onButton4Pressed() { buttonActions[3] = true; }

void setupBLE() {
  NimBLEDevice::init("MacroPad");
  NimBLEDevice::setSecurityAuth(true, false, true);
  
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  
  hid = new NimBLEHIDDevice(pServer);
  
  inputKeyboard = hid->getInputReport(1);
  inputMedia = hid->getInputReport(2);
  
  hid->setReportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
  
  static uint8_t hidInfo[] = {0x01, 0x11, 0x00, 0x02};
  static uint8_t pnpInfo[] = {0x02, 0xe5, 0x05, 0x0a, 0x01, 0x10, 0x01};
  hid->getHidInfo()->setValue(hidInfo, sizeof(hidInfo));
  hid->getPnp()->setValue(pnpInfo, sizeof(pnpInfo));
  
  hid->setBatteryLevel(100);
  hid->startServices();
  
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->getHidService()->getUUID());
  pAdvertising->start();
  
  Serial.println("BLE HID Keyboard ready - connect to 'MacroPad'");
}

void setup(void) {
  Serial.begin(115200);
  tft.init();

  randomSeed(analogRead(0));
  
  animationTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(animationTimer, &onAnimationTimer, true);
  timerAlarmWrite(animationTimer, 80000, true);
  timerAlarmEnable(animationTimer);

  setupBLE();

  void (*buttonCallbacks[])() = {onButton1Pressed, onButton2Pressed, onButton3Pressed, onButton4Pressed};
  void (*buttonISRs[])() = {button1ISR, button2ISR, button3ISR, button4ISR};
  
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i]->begin();
    buttons[i]->onPressed(buttonCallbacks[i]);
    if (buttons[i]->supportsInterrupt()) {
      buttons[i]->enableInterrupt(buttonISRs[i]);
    }
  }

  face.createSprite(tft.width(), tft.height());
  face.loadFont(NotoSansBold15);

  drawSplash();
  delay(2000);
  displayDirty = true;
}

int pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
  return 1;
}

void displayImage(int imgIdx) {
  if (imgIdx < 0 || imgIdx >= NUM_IMAGES) return;
  int16_t rc = png.openFLASH((uint8_t *)IMAGES[imgIdx], IMAGE_SIZES[imgIdx], pngDraw);
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    png.decode(NULL, 0);
    tft.endWrite();
    Serial.printf("Displaying: %s\n", IMAGE_NAMES[imgIdx]);
  }
}

void renderIdle() {
  face.fillSprite(TFT_BLACK);
  
  face.setTextColor(TFT_CYAN, TFT_BLACK);
  face.setTextSize(1);
  face.drawString("MACROPAD", 75, CY - 40);
  
  if (deviceConnected) {
    face.setTextColor(TFT_GREEN, TFT_BLACK);
    face.drawString("BLE Connected", 65, CY);
  } else {
    face.setTextColor(TFT_YELLOW, TFT_BLACK);
    face.drawString("Waiting for BLE...", 55, CY);
  }
  
  face.setTextColor(TFT_WHITE, TFT_BLACK);
  face.drawString("Press any button", 60, CY + 30);
  
  face.pushSprite(0, 0);
}

void renderDisplay() {
  unsigned long now = millis();
  
  if (currentDisplayImage >= 0 && (now - imageDisplayStart < IMAGE_DISPLAY_DURATION)) {
    displayImage(currentDisplayImage);
    displayMode = 1;
  } else {
    if (displayMode == 1) {
      currentDisplayImage = -1;
      displayDirty = true;
    }
    displayMode = 0;
    renderIdle();
  }
}

void loop() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i]->read();
  }
  
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonActions[i]) {
      buttonActions[i] = false;
      handleButtonPress(i);
    }
  }
  
  unsigned long currentMillis = millis();
  
  if (activeLED >= 0 && !playingAnimation && !lightDisplayMode && 
      (currentMillis - ledStartTime >= LED_DURATION_MS)) {
    turnOffAllLEDs();
    activeLED = -1;
  }
  
  if (lightDisplayMode && !playingAnimation) {
    if (currentMillis - lastLightDisplayUpdate >= LIGHT_DISPLAY_INTERVAL) {
      lastLightDisplayUpdate = currentMillis;
      lightLED(lightDisplayStep % 4);
      lightDisplayStep++;
      activeLED = -1;
    }
  }
  
  if (displayDirty && (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
    renderDisplay();
    displayDirty = false;
    lastDisplayUpdate = currentMillis;
  }
  
  static unsigned long lastIdleUpdate = 0;
  if (currentDisplayImage < 0 && (currentMillis - lastIdleUpdate >= 1000)) {
    displayDirty = true;
    lastIdleUpdate = currentMillis;
  }
}

void drawSplash() {
  displayImage(0);
}
