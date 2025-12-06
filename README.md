# Wukkta 25 Badge

Features:

* BLE scanning for other badges
* User-specific splash images
* Interrupt driven MX switches with LEDs
* Charlieplexed LEDs
* Battery powered with USB-C charging
* 1.8" Round display (GC9A01)

Based on the SEEED XIAO ESP32-C3

## Hacking

Use Arduino.

Requires the esp32 Espressif board library, use version 2.0.14.

Requires a few libraries, via the library manager:

* TFT_eSPI (2.5.34, see compatibility note below)
* EasyButton (2.0.3)
* NimBLE Ardiono (2.3.6)
* PNGdec (1.1.6)

Some of these might have dependencies, and if they do you'll see errors.  

Select XIAO_ESP32C3 as the board, and it will appear as as `usbmodem` device on
a mac. 

### Suggestion

* Clean up my vibe coded mess.
* Write a macropad mode.
* Allow communications between badges.
* Add a decent AGENT.md with instructions on things like not using timers
inside of ISRs, and maybe including links to NimBLE's really good docs. 


## Display

The display is supported by TFT_eSPI, but there's a compatibility issue with
esp32 library versions above 2.0.14, where the board will hit a reboot loop.

On a mac, you'll need to populate `User_Setup.h` with board setup information,
and drop that in `~/Documents/Arduino/libraries/TFT_eSPI`. It's probably in
similar places on other platforms.

I'm the using the following, but I'm not sure where I got it:

```
#define USER_SETUP_ID 66

#define GC9A01_DRIVER  // Full configuration option, define additional parameters below for this display

#define TFT_RGB_ORDER TFT_RGB  // Colour order Blue-Green-Red
#define TFT_HEIGHT 240 // GC9A01 240 x 240

#define TFT_SCLK D8
#define TFT_MISO D9
#define TFT_MOSI D10
#define TFT_CS   D0  // Chip select control pin
#define TFT_DC   D1  // Data Command control pin
#define TFT_BL   D6
#define TFT_RST  -1  // Reset pin (could connect to RST pin)

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY  20000000

```
