#include <TFT_eSPI.h>
#include <EasyButton.h>

#undef local
#include <NimBLEDevice.h>
#define local static

#include <PNGdec.h>
#include "NotoSansBold15.h"
#include "screwdriver.h"

uint8_t debug = 1;
bool scanningEnabled = true;
uint8_t displayMode = 0; // 0=radar, 1=bars, 2=detailed, 3=credits
bool isPinging = false;
unsigned long pingStartTime = 0;
#define PING_DURATION_MS 3000
bool scanningWasPausedBeforePing = false;
volatile bool playingAnimation = false;
volatile int animationStep = 0;
bool showPingImage = false;
bool showBadgeImage = false;
int badgeImageIndex = 0;
unsigned long badgeImageStartTime = 0;
#define BADGE_IMAGE_DURATION 4000

#define MAX_IMAGE_QUEUE 5
int imageQueue[MAX_IMAGE_QUEUE];
int imageQueueCount = 0;
int currentQueueIndex = 0;
bool displayingQueuedImages = false;

bool displayDirty = true;
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 50  // 20 FPS

int activeLED = -1;
unsigned long ledStartTime = 0;
#define LED_DURATION_MS 150

bool pausedLEDState = false;
unsigned long lastPausedFlashTime = 0;
#define PAUSED_FLASH_INTERVAL 500

bool lightDisplayMode = false;
int lightDisplayStep = 0;
unsigned long lastLightDisplayUpdate = 0;
#define LIGHT_DISPLAY_INTERVAL 100

bool scanInProgress = false;
unsigned long scanStartTime = 0;
#define SCAN_DURATION_MS 10000

volatile bool button1Action = false;
volatile bool button3LongAction = false;

hw_timer_t* animationTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// PNG cruft
PNG png; // PNG decoder instance
#define MAX_IMAGE_WIDTH 240
int16_t xpos = 0;
int16_t ypos = 0;

// Image ideas.....
// show screwdriver spash image, pick user? 
// then what, do we want others too? 
// if so in flash? (for now yes)

// If we don't pick a user???
// Just look for other badges and display strength? 

// function ideas:
// scan for other badges, draw segments for each detected.
// make number of segments variable
// BT:
// advertise self (allow naming??)
// scan every X sections
// flash LEDs? 
// what shall the buttons do? 

// button ideas
// bluetooth macropad??



TFT_eSPI tft = TFT_eSPI();  // Create object "tft"
TFT_eSprite face = TFT_eSprite(&tft);
uint16_t radar_fg = 0x07C0;

#define CLOCK_R       127.0f / 2.0f
#define FACE_W        CLOCK_R * 2 + 1
#define FACE_H    CLOCK_R * 2 + 1
#define CX tft.width()  / 2.0f
#define CY tft.height() / 2.0f

// Pins for charlieplexing LEDS. We have 4 in total, one for each MX switch
#define LED_PIN_A D3
#define LED_PIN_B D4
#define LED_PIN_C D5
 
// Pins for MX switches. No debounce hardware so this will need doing in software
// Use EasyButton
#define SW_PIN_A D7
#define SW_PIN_B D6
#define SW_PIN_C D9 // problem
#define SW_PIN_D D2

EasyButton button1(SW_PIN_A);
EasyButton button2(SW_PIN_B);
EasyButton button3(SW_PIN_C);
EasyButton button4(SW_PIN_D);

#define MAX_DEVICES 5
#define BADGE_NAME_PREFIX "Wukkta25-Badge"
#define BADGE_USER_NAME "Sacha"
#define BADGE_FULL_NAME BADGE_NAME_PREFIX "-" BADGE_USER_NAME

struct BLEDeviceInfo {
  String address;
  String name;
  int rssi;
  unsigned long lastSeen;
  bool isPinging;
};

BLEDeviceInfo discoveredDevices[MAX_DEVICES];
int deviceCount = 0;

NimBLEScan* pBLEScan;
NimBLEAdvertising* pAdvertising;

class MyScanCallbacks: public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
  String address = advertisedDevice->getAddress().toString().c_str();
  String name = advertisedDevice->haveName() ? advertisedDevice->getName().c_str() : "";
  int rssi = advertisedDevice->getRSSI();
  
  if (debug) {
    Serial.printf("Found device: %s (%s) RSSI: %d\n", address.c_str(), name.c_str(), rssi);
  }
  
  if (!name.startsWith(BADGE_NAME_PREFIX)) return;
  
  unsigned long now = millis();
  int existingIndex = -1;
  
  for (int i = 0; i < deviceCount; i++) {
    if (discoveredDevices[i].address == address) {
      existingIndex = i;
      break;
    }
  }
  
  if (existingIndex >= 0) {
    discoveredDevices[existingIndex].rssi = rssi;
    discoveredDevices[existingIndex].lastSeen = now;
    if (name.length() > 0) {
      discoveredDevices[existingIndex].name = name;
    }
  } else if (deviceCount < MAX_DEVICES) {
    discoveredDevices[deviceCount].address = address;
    discoveredDevices[deviceCount].name = name;
    discoveredDevices[deviceCount].rssi = rssi;
    discoveredDevices[deviceCount].lastSeen = now;
    discoveredDevices[deviceCount].isPinging = false;
    deviceCount++;
    
    if (debug) {
      Serial.printf("New badge: %s (%s) RSSI: %d\n", address.c_str(), name.c_str(), rssi);
    }
    
    // Queue image for specific badge names
    int imgIdx = -1;
    if (name.indexOf("feroz") >= 0 || name.indexOf("Feroz") >= 0) {
      imgIdx = 3; // puffa
    } else if (name.indexOf("niko") >= 0 || name.indexOf("Niko") >= 0) {
      imgIdx = 2; // camera
    } else if (name.indexOf("paul") >= 0 || name.indexOf("Paul") >= 0) {
      imgIdx = 1; // coin
    } else if (name.indexOf("will") >= 0 || name.indexOf("Will") >= 0) {
      imgIdx = 4; // switch
    } else if (name.indexOf("matt") >= 0 || name.indexOf("Matt") >= 0) {
      imgIdx = 0; // screwdriver
    }
    
    // Add to queue if we have a match and space
    if (imgIdx >= 0 && imageQueueCount < MAX_IMAGE_QUEUE) {
      imageQueue[imageQueueCount++] = imgIdx;
      Serial.printf("Queued image %d for %s\n", imgIdx, name.c_str());
    }
  }
  displayDirty = true;
  }
  
  void onScanEnd(const NimBLEScanResults& results, int reason) {
    Serial.printf("*** Scan complete. Reason: %d, Found %d total ads, Tracking %d badges ***\n", reason, results.getCount(), deviceCount);
    scanInProgress = false;
    
    // Start displaying queued images if any
    if (imageQueueCount > 0) {
      displayingQueuedImages = true;
      currentQueueIndex = 0;
      Serial.printf("Starting to display %d queued images\n", imageQueueCount);
    } else {
      displayDirty = true;
    }
  }
};

void button1ISR() {
  // See: https://github.com/evert-arias/EasyButton/blob/main/examples/Interrupts/Interrupts.ino#L31
  button1.read();
}

void button2ISR() {
  button2.read();
}

void button3ISR() {
  button3.read();
}

void button4ISR() {
  button4.read();
}

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
    
    // 4 LEDs × 3 cycles = 12 steps
    if (animationStep >= 12) {
      playingAnimation = false;
      turnOffAllLEDs();
    }
  }
}

void startPingAnimation() {
  portENTER_CRITICAL(&timerMux);
  playingAnimation = true;
  animationStep = 0;
  portEXIT_CRITICAL(&timerMux);
}

void updateAdvertising() {
  // For now, just track ping state locally
  // Visual feedback on local display
  Serial.printf("Ping state: %s\n", isPinging ? "ACTIVE" : "INACTIVE");
}

void onButton1Pressed() {
  button1Action = true;
}

void onButton2Pressed(){
  Serial.println("B2");
  displayMode = (displayMode + 1) % 4;
  displayDirty = true;
  
  if (!playingAnimation) {
    activeLED = 1;
    ledStartTime = millis();
    lightLED(1);
  }
  
  const char* modes[] = {"RADAR", "BARS", "DETAILED", "CREDITS"};
  Serial.printf("Display mode: %s\n", modes[displayMode]);
}
 
void onButton3Pressed(){
  lightDisplayMode = !lightDisplayMode;
  displayDirty = true;
  
  if (lightDisplayMode) {
    lightDisplayStep = 0;
    lastLightDisplayUpdate = millis();
    Serial.println("Light display STARTED");
  } else {
    turnOffAllLEDs();
    Serial.println("Light display STOPPED");
    
    if (!playingAnimation) {
      activeLED = 2;
      ledStartTime = millis();
      lightLED(2);
    }
  }
}

int currentImage = 0;

void displayPingImage() {
  const byte* images[] = {screwdriver, coin, camera, puffa, sw};
  const int imageSizes[] = {sizeof(screwdriver), sizeof(coin), sizeof(camera), sizeof(puffa), sizeof(sw)};
  const char* imageNames[] = {"screwdriver", "coin", "camera", "puffa", "sw"};
  
  int16_t rc = png.openFLASH((uint8_t *)images[currentImage], imageSizes[currentImage], pngDraw);
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    rc = png.decode(NULL, 0);
    tft.endWrite();
    Serial.printf("Displaying: %s\n", imageNames[currentImage]);
  }
  
  delay(1000);
  
  currentImage = (currentImage + 1) % 5;
}

void onButton4Pressed(){
  showPingImage = true;
  
  if (!playingAnimation) {
    activeLED = 3;
    ledStartTime = millis();
    lightLED(3);
  }
}

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {

  Serial.begin(115200);
  tft.init();

  randomSeed(analogRead(0));
  
  // Setup timer for LED animation - 80ms interval
  // Prescaler 80: 80MHz / 80 = 1MHz (1 tick = 1 microsecond)
  // 80ms = 80,000 microseconds = 80,000 ticks
  animationTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(animationTimer, &onAnimationTimer, true);
  timerAlarmWrite(animationTimer, 80000, true);
  timerAlarmEnable(animationTimer);

  NimBLEDevice::init(BADGE_FULL_NAME);
  
  // Set up advertising with device name
  NimBLEAdvertisementData advertisementData;
  advertisementData.setName(BADGE_FULL_NAME);
  advertisementData.setCompleteServices(NimBLEUUID("FEAD"));
  
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setAdvertisementData(advertisementData);
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  pAdvertising->start();
  
  Serial.println("========================================");
  Serial.println("BLE Advertising started");
  Serial.printf("Name: %s\n", BADGE_FULL_NAME);
  Serial.printf("Address: %s\n", NimBLEDevice::getAddress().toString().c_str());
  Serial.println("========================================");
  
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setScanCallbacks(new MyScanCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setDuplicateFilter(false);
  
  Serial.println("BLE Scanning initialized");

  button1.begin();
  button2.begin();
  button3.begin();
  button4.begin();

  button1.onPressed(onButton1Pressed);
  button2.onPressed(onButton2Pressed);
  button3.onPressed(onButton3Pressed);
  button4.onPressed(onButton4Pressed);

  if (button1.supportsInterrupt()) {
    button1.enableInterrupt(button1ISR);
  }
  if (button2.supportsInterrupt()) {
    button2.enableInterrupt(button2ISR);
  }
  if (button3.supportsInterrupt()) {
    button3.enableInterrupt(button3ISR);
  }
  if (button4.supportsInterrupt()) {
    button4.enableInterrupt(button4ISR);
  }


  face.createSprite(tft.width(), tft.height());

  drawSplash();
  delay(3000);

}



// On startup, ask for user name, pick from list. 
// Save name, work out how to store other user details 

// TODO: scan for bluetooth devices (badges) and plot strengths
// ideally with the smae device in each sector, and the top sector empty, for other info.
// TODO: work out what signal strength looks like, then map from 0 to 120
int mapRSSI(int rssi) {
  return map(constrain(rssi, -100, -30), -100, -30, 0, 120);
}

void renderRadar()
{
  face.fillSprite(TFT_BLACK);

  uint32_t radius = 0;
  float xp = 0.0, yp = 0.0;

  for (int i = 0; i < min(deviceCount, 4); i++) {
    int arcRadius = mapRSSI(discoveredDevices[i].rssi);
    uint16_t color = discoveredDevices[i].isPinging ? TFT_YELLOW : ((i % 2 == 0) ? TFT_RED : TFT_BLUE);
    int startAngle = i * 72;
    int endAngle = startAngle + 72;
    
    if (i >= 2) {
      startAngle += 72;
      endAngle += 72;
    }
    
    face.drawSmoothArc(CX, CY, arcRadius, 0, startAngle, endAngle, color, TFT_BLACK);
  }

  radius = 0;
  while(radius < CX) {
      face.drawSmoothCircle(CX, CY, radius, radar_fg, TFT_BLACK);
      radius += 20;
  }

  for (uint16_t angle = 0; angle < 360; angle += 72) {
    getCoord(CX, CY, &xp, &yp, FACE_W, angle + 36);
    face.drawWideLine(CX, CY, xp, yp, 3, radar_fg, TFT_BLACK);
  }
  
  face.setTextColor(TFT_WHITE, TFT_BLACK);
  face.drawString("WUKKTA25", 95, CLOCK_R * 0.60);
  
  if (lightDisplayMode) {
    face.setTextColor(TFT_MAGENTA, TFT_BLACK);
    face.drawString("LIGHT SHOW", 85, CLOCK_R * 0.75);
  } else {
    face.drawString(scanningEnabled ? "SCANNING" : "PAUSED", 95, CLOCK_R * 0.75);
  }
  
  face.setTextColor(TFT_BLUE, TFT_BLACK);
  face.drawString(BADGE_USER_NAME, 105, CLOCK_R * 0.90);

  face.pushSprite(0, 0, TFT_TRANSPARENT);
}

void renderBars()
{
  face.fillSprite(TFT_BLACK);
  face.setTextColor(TFT_WHITE, TFT_BLACK);
  face.setTextSize(1);
  
  face.drawString("SIGNAL BARS", 60, 15);
  
  int y = 45;
  int barHeight = 15;
  int spacing = 20;
  
  for (int i = 0; i < min(deviceCount, 5); i++) {
    int barWidth = map(constrain(discoveredDevices[i].rssi, -100, -30), -100, -30, 0, 100);
    uint16_t color = discoveredDevices[i].isPinging ? TFT_YELLOW : TFT_GREEN;
    
    face.fillRect(50, y, barWidth, barHeight, color);
    face.drawRect(50, y, 100, barHeight, TFT_WHITE);
    
    face.drawString(String(discoveredDevices[i].rssi) + "dBm", 165, y + 3);
    
    y += spacing;
  }
  
  face.pushSprite(0, 0);
}

void renderDetailed()
{
  face.fillSprite(TFT_BLACK);
  
  if (deviceCount > 0) {
    static int detailedIndex = 0;
    if (detailedIndex >= deviceCount) detailedIndex = 0;
    
    uint16_t color = discoveredDevices[detailedIndex].isPinging ? TFT_YELLOW : TFT_GREEN;
    
    // Center text for round display - use CX and CY from radar
    int centerY = CY - 20;
    
    face.setTextColor(color, TFT_BLACK);
    face.setTextSize(1);
    face.drawString(discoveredDevices[detailedIndex].name, 20, centerY);
    
    face.setTextColor(TFT_WHITE, TFT_BLACK);
    face.drawString("RSSI: " + String(discoveredDevices[detailedIndex].rssi) + " dBm", 20, centerY + 20);
    
    if (discoveredDevices[detailedIndex].isPinging) {
      face.setTextColor(TFT_YELLOW, TFT_BLACK);
      face.drawString("** PINGING **", 25, centerY + 40);
    }
    
    detailedIndex = (detailedIndex + 1) % deviceCount;
  } else {
    face.setTextColor(TFT_WHITE, TFT_BLACK);
    face.setTextSize(1);
    face.drawString("No devices", 30, CY);
  }
  
  face.pushSprite(0, 0);
}

void renderCredits()
{
  face.fillSprite(TFT_BLACK);
  face.setTextColor(TFT_GREEN, TFT_BLACK);
  face.setTextSize(1);
  
  int y = CY - 45;
  
  face.drawString("WUKKTA25 BADGE", 75, y);
  
  y += 25;
  face.setTextColor(TFT_CYAN, TFT_BLACK);
  face.drawString("by Matt", 95, y);
  
  y += 25;
  face.setTextColor(TFT_YELLOW, TFT_BLACK);
  face.drawString("starring", 90, y);
  
  y += 20;
  face.setTextColor(TFT_WHITE, TFT_BLACK);
  face.drawString("Feroz, Niko,", 80, y);
  
  y += 15;
  face.drawString("Paul, Will", 85, y);
  
  face.pushSprite(0, 0);
}

void renderDisplay() {
  switch(displayMode) {
    case 0: renderRadar(); break;
    case 1: renderBars(); break;
    case 2: renderDetailed(); break;
    case 3: renderCredits(); break;
  }
}
// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop()
{
  button1.read();
  button2.read();
  button3.read();
  button4.read();
  
  // Handle ping image display
  if (showPingImage) {
    showPingImage = false;
    scanningWasPausedBeforePing = !scanningEnabled;
    
    if (scanningEnabled) {
      pBLEScan->stop();
      scanningEnabled = false;
    }
    
    isPinging = true;
    pingStartTime = millis();
    updateAdvertising();
    Serial.println("PING sent!");
    
    displayPingImage();
    startPingAnimation();
  }
  
  unsigned long currentMillis = millis();
  
  // Handle queued image display (sequential after scan)
  if (displayingQueuedImages) {
    if (badgeImageStartTime == 0) {
      // Start showing current image in queue
      const byte* images[] = {screwdriver, coin, camera, puffa, sw};
      const int imageSizes[] = {sizeof(screwdriver), sizeof(coin), sizeof(camera), sizeof(puffa), sizeof(sw)};
      const char* imageNames[] = {"screwdriver", "coin", "camera", "puffa", "sw"};
      
      int imgIdx = imageQueue[currentQueueIndex];
      int16_t rc = png.openFLASH((uint8_t *)images[imgIdx], imageSizes[imgIdx], pngDraw);
      if (rc == PNG_SUCCESS) {
        tft.startWrite();
        rc = png.decode(NULL, 0);
        tft.endWrite();
        Serial.printf("Displaying queued image %d/%d: %s\n", currentQueueIndex + 1, imageQueueCount, imageNames[imgIdx]);
      }
      badgeImageStartTime = currentMillis;
    } else if (currentMillis - badgeImageStartTime >= BADGE_IMAGE_DURATION) {
      // Image duration complete, move to next
      currentQueueIndex++;
      badgeImageStartTime = 0;
      
      if (currentQueueIndex >= imageQueueCount) {
        // All images shown, reset
        displayingQueuedImages = false;
        imageQueueCount = 0;
        currentQueueIndex = 0;
        displayDirty = true;
        Serial.println("All queued images displayed");
      }
    }
  }
  
  // Handle BLE scanning state machine
  if (scanningEnabled && !scanInProgress) {
    // Start new scan (callback will handle completion)
    Serial.println("Starting 10-second BLE scan...");
    scanInProgress = true;
    // Don't clear deviceCount - let devices accumulate
    pBLEScan->start(SCAN_DURATION_MS, false);
  }
  
  // Handle button 1 action (toggle scanning)
  if (button1Action) {
    button1Action = false;
    scanningEnabled = !scanningEnabled;
    displayDirty = true;
    
    if (!playingAnimation) {
      activeLED = 0;
      ledStartTime = millis();
      lightLED(0);
    }
    
    if (!scanningEnabled && scanInProgress) {
      pBLEScan->stop();
      scanInProgress = false;
      Serial.println("Scanning PAUSED");
    } else if (scanningEnabled) {
      Serial.println("Scanning ENABLED");
    }
  }
  
  // Handle LED timeout (turn off after duration)
  if (activeLED >= 0 && !playingAnimation && !lightDisplayMode && (currentMillis - ledStartTime >= LED_DURATION_MS)) {
    turnOffAllLEDs();
    activeLED = -1;
  }
  
  // Light display mode - cycle through LEDs continuously
  if (lightDisplayMode && !playingAnimation) {
    if (currentMillis - lastLightDisplayUpdate >= LIGHT_DISPLAY_INTERVAL) {
      lastLightDisplayUpdate = currentMillis;
      lightLED(lightDisplayStep % 4);
      lightDisplayStep++;
      activeLED = -1;
    }
  }
  
  // Flash LED 0 when scanning is paused (to indicate button 1 will resume)
  if (!scanningEnabled && !playingAnimation && !lightDisplayMode && activeLED < 0) {
    if (currentMillis - lastPausedFlashTime >= PAUSED_FLASH_INTERVAL) {
      pausedLEDState = !pausedLEDState;
      lastPausedFlashTime = currentMillis;
      
      if (pausedLEDState) {
        lightLED(0);
      } else {
        turnOffAllLEDs();
      }
    }
  }
  
  // Handle ping timeout
  if (isPinging && (currentMillis - pingStartTime >= PING_DURATION_MS)) {
    isPinging = false;
    updateAdvertising();
    Serial.println("Ping ended");
    
    // Auto-restart scanning after ping (unless it was paused before)
    if (!scanningWasPausedBeforePing) {
      scanningEnabled = true;
      pBLEScan->start(2, false);
      displayDirty = true;
      Serial.println("Scanning auto-resumed");
    }
  }
  
  // Only update display at max 20 FPS when dirty
  if (displayDirty && (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
    renderDisplay();
    displayDirty = false;
    lastDisplayUpdate = currentMillis;
  }
}

void drawSplash() {
  // Select image based on badge user name
  const byte* images[] = {screwdriver, coin, camera, puffa, sw};
  const int imageSizes[] = {sizeof(screwdriver), sizeof(coin), sizeof(camera), sizeof(puffa), sizeof(sw)};
  
  int imgIdx = 0; // default to screwdriver
  String userName = String(BADGE_USER_NAME);
  userName.toLowerCase();
  
  if (userName.indexOf("feroz") >= 0) {
    imgIdx = 3; // puffa
  } else if (userName.indexOf("niko") >= 0) {
    imgIdx = 2; // camera
  } else if (userName.indexOf("paul") >= 0) {
    imgIdx = 1; // coin
  } else if (userName.indexOf("will") >= 0) {
    imgIdx = 4; // sw
  } else if (userName.indexOf("matt") >= 0) {
    imgIdx = 0; // screwdriver
  }
  
  int16_t rc = png.openFLASH((uint8_t *)images[imgIdx], imageSizes[imgIdx], pngDraw);
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    rc = png.decode(NULL, 0);
    tft.endWrite();
  }
}

// =========================================================================
// Get coordinates of end of a line, pivot at x,y, length r, angle a
// =========================================================================
// Coordinates are returned to caller via the xp and yp pointers
// From Smooth Clock example
#define DEG2RAD 0.0174532925
void getCoord(int16_t x, int16_t y, float *xp, float *yp, int16_t r, float a)
{
  float sx1 = cos( (a - 90) * DEG2RAD);
  float sy1 = sin( (a - 90) * DEG2RAD);
  *xp =  sx1 * r + x;
  *yp =  sy1 * r + y;
}

// From examples, except that this needs and int return type to work.
int pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
  return 1;
}

