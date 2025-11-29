#include <TFT_eSPI.h>
#include <EasyButton.h>

#undef local
#include <NimBLEDevice.h>
#define local static

#include <PNGdec.h>
#include "NotoSansBold15.h"
#include "screwdriver.h"

uint8_t debug = 1;

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
#define SCAN_INTERVAL_MS 2000
#define BADGE_NAME_PREFIX "Wukkta25-Badge"

struct BLEDeviceInfo {
  String address;
  String name;
  int rssi;
  unsigned long lastSeen;
};

BLEDeviceInfo discoveredDevices[MAX_DEVICES];
int deviceCount = 0;
unsigned long lastScanTime = 0;

NimBLEScan* pBLEScan;
NimBLEAdvertising* pAdvertising;

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

void onButton1Pressed() {
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(LED_PIN_C, INPUT);
  digitalWrite(LED_PIN_A, HIGH);
  digitalWrite(LED_PIN_B, LOW);
  digitalWrite(LED_PIN_C, LOW);
}

void onButton2Pressed(){
  pinMode(LED_PIN_A, INPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(LED_PIN_C, OUTPUT);
  digitalWrite(LED_PIN_A, LOW);
  digitalWrite(LED_PIN_B, LOW);
  digitalWrite(LED_PIN_C, HIGH);
}
 
void onButton3Pressed(){
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(LED_PIN_C, INPUT);
  digitalWrite(LED_PIN_A, LOW);
  digitalWrite(LED_PIN_B, HIGH);
  digitalWrite(LED_PIN_C, LOW);
}

void onButton4Pressed(){
  pinMode(LED_PIN_A, INPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(LED_PIN_C, OUTPUT);
  digitalWrite(LED_PIN_A, LOW);
  digitalWrite(LED_PIN_B, HIGH);
  digitalWrite(LED_PIN_C, LOW);
}

void processBLEDevice(const NimBLEAdvertisedDevice* advertisedDevice) {
  String address = advertisedDevice->getAddress().toString().c_str();
  String name = advertisedDevice->haveName() ? advertisedDevice->getName().c_str() : "";
  int rssi = advertisedDevice->getRSSI();
  unsigned long now = millis();
  
  if (!name.startsWith(BADGE_NAME_PREFIX)) {
    return;
  }
  
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
    deviceCount++;
    
    if (debug) {
      Serial.printf("New badge found: %s (%s) RSSI: %d\n", 
                    address.c_str(), 
                    name.c_str(), 
                    rssi);
    }
  }
}

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {

  Serial.begin(115200);
  tft.init();

  randomSeed(analogRead(0));

  NimBLEDevice::init("Wukkta25-Badge");
  
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->start();
  
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setDuplicateFilter(false);
  
  Serial.println("BLE Advertising and Scanning initialized");

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

char *names[] = { 
  "feroz",
  "matt",
  "niko",
  "paul",
  "will"
};

int name_count = sizeof(names) / sizeof(names[0]);

// Find the index for a given name
int find_name(char *name) {
  for (uint8_t ii = 0; ii < name_count; ii++) {
    if (!strcmp(names[ii], name)) {
      return ii;
    }
  }

  return -1;
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
    uint16_t color = (i % 2 == 0) ? TFT_RED : TFT_BLUE;
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
  face.drawString("Wukkta25", CLOCK_R + 32, CLOCK_R * 0.75);

  face.pushSprite(0, 0, TFT_TRANSPARENT);
}
// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop()
{
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastScanTime >= SCAN_INTERVAL_MS) {
    lastScanTime = currentMillis;
    
    deviceCount = 0;
    
    Serial.println("Starting BLE scan...");
   
    NimBLEScan *pScan = NimBLEDevice::getScan();
    NimBLEScanResults results = pScan->getResults(10 * 1000);
    
    Serial.printf("Scan complete. Found %d advertisements\n", results.getCount());
    
    for (int i = 0; i < results.getCount(); i++) {
      const NimBLEAdvertisedDevice* device = results.getDevice(i);
      processBLEDevice(device);
    }
    
    pBLEScan->clearResults();
    
    Serial.printf("Tracking %d unique devices\n", deviceCount);
  }
  
  renderRadar();
  delay(50);
}

void drawSplash() {
  int16_t rc = png.openFLASH((uint8_t *)screwdriver, sizeof(screwdriver), pngDraw);
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

