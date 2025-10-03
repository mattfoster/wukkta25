#include <TFT_eSPI.h>
#include "NotoSansBold15.h"

TFT_eSPI tft = TFT_eSPI();  // Create object "tft"
TFT_eSprite face = TFT_eSprite(&tft);
uint16_t radar_fg = 0x07C0;

#define CLOCK_R       127.0f / 2.0f
#define FACE_W        CLOCK_R * 2 + 1
#define FACE_H    CLOCK_R * 2 + 1
#define CX tft.width()  / 2.0f
#define CY tft.height() / 2.0f

#define LED_PIN_A D3
#define LED_PIN_B D4
 

// TODO: how to scan and store all these.
struct btle_readings {
  char name[10]; // my name - can we select this from a list on startup? 
  uint16_t str1;
  uint16_t str2;
  uint16_t str3;
  uint16_t str4;
};

struct btle_readings badges;

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {

  Serial.begin(115200);
  tft.init();

  randomSeed(analogRead(0));

  face.createSprite(tft.width(), tft.height());

  // face.loadFont(NotoSansBold15);
  badges.str1 = 100;
  badges.str2 = 30;
  badges.str3 = 60;
  badges.str4 = 80;
  

  renderRadar();
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

void led_1(){
  // charlieplexing
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  digitalWrite(LED_PIN_A, HIGH);
  digitalWrite(LED_PIN_B, LOW);
}
 
void led_2(){
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  digitalWrite(LED_PIN_A, LOW);
  digitalWrite(LED_PIN_B, HIGH);
}


// On startup, ask for user name, pick from list. 
// Save name, work out how to store other user details 

// TODO: scan for bluetooth devices (badges) and plot strengths
// ideally with the smae device in each sector, and the top sector empty, for other info.
// TODO: work out what signal strength looks like, then map from 0 to 120
void renderRadar()
{

  face.fillSprite(TFT_BLACK);

  uint32_t radius = 0;
  float xp = 0.0, yp = 0.0; // Use float pixel position for smooth AA

  getCoord(CX, CY, &xp, &yp, 35, 0);
  face.drawSmoothArc(CX, CY, badges.str1, 0, 0, 72, TFT_RED, TFT_BLACK);

  getCoord(CX, CY, &xp, &yp, 35, 0);
  face.drawSmoothArc(CX, CY, badges.str2, 0, 72, 144, TFT_BLUE, TFT_BLACK);

  // Skip middle slice

  getCoord(CX, CY, &xp, &yp, 35, 0);
  face.drawSmoothArc(CX, CY, badges.str3, 0, 216, 288, TFT_RED, TFT_BLACK);

  getCoord(CX, CY, &xp, &yp, 35, 0);
  face.drawSmoothArc(CX, CY, badges.str4, 0, 288, 360, TFT_BLUE, TFT_BLACK);

  radius = 0;
  while(radius < CX) {
      face.drawSmoothCircle(CX, CY, radius, radar_fg, TFT_BLACK);
      radius += 20;
  }

  for (uint16_t angle = 0; angle < 360; angle += 72) {
    getCoord(CX, CY, &xp, &yp, FACE_W, angle + 36);
    face.drawWideLine(CX, CY, xp, yp, 3, radar_fg, TFT_BLACK);
  }

  // Sweep
  // getCoord(CX, CY, &xp, &yp, FACE_W, trace);
  // face.drawWideLine(CX, CY, xp, yp, 3, radar_fg, TFT_BLACK);
  // trace += 2;
 
  
  face.setTextColor(TFT_WHITE, TFT_BLACK);
  face.drawString("Wukkta25", CLOCK_R + 32, CLOCK_R * 0.75);

  face.pushSprite(0, 0, TFT_TRANSPARENT);
}
// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop()
{

  // Get signal strengths here
  badges.str1 = random(0, 120);
  badges.str2 = random(0, 120);
  badges.str3 = random(0, 120);
  badges.str4 = random(0, 120);
  
  renderRadar();
  delay(100);
  led_1();
  delay(100);
  led_2();

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

