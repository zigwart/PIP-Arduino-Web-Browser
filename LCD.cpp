#include "LCD.h"
#include "LCDfont.c"
#include <limits.h>
#include "pins_arduino.h"
#include "wiring_private.h"
//#include <avr/pgmspace.h>
#include <SPI.h>

#define SCLK 13  // Don't change
#define MOSI 11  // Don't change
#define CS   9
#define DC   8
#define RST  7  // you can also connect this to the Arduino reset

#define DELAY 0x80

#define TFT_CASET   0x2A
#define TFT_RASET   0x2B
#define TFT_RAMWR   0x2C

// SPI/TFT setup
volatile uint8_t *csport, *rsport;
uint8_t  datapinmask, clkpinmask, cspinmask, rspinmask;


//================================================
// LCD object init
//================================================
LCD::LCD (int16_t w, int16_t h): 
WIDTH(w), HEIGHT(h), CURSOR_WIDTH(w / TEXTWIDTH), CURSOR_HEIGHT (h / TEXTHEIGHT) {
  cursorX =  0;
  cursorY = 0;
  textColour = 0xFFFF;
  bgColour = 0x0000;
  wrap = true;
}

//================================================
// SPI write commands
//================================================
void spiwrite (uint8_t c) {
  SPDR = c;
  while (!(SPSR & _BV(SPIF)));
}

void writecommand (uint8_t c) {
  *rsport &= ~rspinmask;
  *csport &= ~cspinmask;
  spiwrite (c);
  *csport |= cspinmask;
}

void writedata (uint8_t c) {
  *rsport |= rspinmask;
  *csport &= ~cspinmask;
  spiwrite (c);
  *csport |= cspinmask;
}

void setAddrWindow (uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  writecommand (TFT_CASET); // Column addr set
  writedata (y0 >> 8);
  writedata (y0 & 0xff);     // YSTART
  writedata (y1 >> 8);
  writedata (y1 & 0xff);     // YEND  
  writecommand (TFT_RASET); // Row addr set
  writedata (x0 >> 8);
  writedata (x0 & 0xff);     // XSTART 
  writedata (x1 >> 8);
  writedata (x1 & 0xff);     // XEND
  writecommand (TFT_RAMWR); // write to RAM
}


//================================================
// LCD init code store
//================================================
static const uint8_t PROGMEM LCDinit[] = {
  21,
  0xcb,	5,  0x39, 0x2c, 0x00, 0x34, 0x02,
  0xcf,	3,  0x00, 0xc1, 0x30,
  0xe8,	3,  0x85, 0x00, 0x78,
  0xea,	2,  0x00, 0x00,
  0xed,	4,  0x64, 0x03,	0x12, 0x81,
  0xf7,	1,  0x20,
  0xc0,	1,  0x23,
  0xc1,	1,  0x10,
  0xc5,	2,  0x3e, 0x28,
  0xc7,	1,  0x86,
  0x36,	1,  0x08,
  0x3a,	1,  0x55,
  0xb1,	2,  0x00, 0x18,
  0xb6,	3,  0x08, 0x82, 0x27,
  0xf2,	1,  0x00,
  0x26,	1,  0x01,
  0xe0,	15, 0x0f, 0x31, 0x2b, 0x0c, 0x0e, 0x08, 0x4e, 0xf1, 0x37, 0x07, 0x10, 0x03, 0x0e, 0x09, 0x00,
  0xe1, 15, 0x00, 0x0e, 0x14, 0x03, 0x11, 0x07, 0x031, 0xc1, 0x48, 0x08, 0x0f, 0x0c, 0x31, 0x36, 0x0f,
  0x11, 0+DELAY, 120,
  0x29, 0,
  0x2c, 0 
};
//0x08,

//================================================
// LCD init code handler
//================================================
void commandList (const uint8_t *addr) {
  uint8_t numCommands, numArgs;
  uint16_t ms;

  numCommands = pgm_read_byte (addr++);   // Number of commands to follow

  while (numCommands--) {                 // For each command...
    writecommand (pgm_read_byte (addr++)); //   Read, issue command
    numArgs = pgm_read_byte (addr++);    //   Number of args to follow
    ms = numArgs & DELAY;          //   If hibit set, delay follows args
    numArgs &= ~DELAY;                   //   Mask out delay bit
    while (numArgs--) {                   //   For each argument...
      writedata (pgm_read_byte (addr++));  //     Read, issue argument
    }
    if (ms) {
      ms = pgm_read_byte (addr++); // Read post-command delay time (ms)
      if (ms == 255) ms = 500;     // If 255, delay for 500 ms
      delay (ms);
    }
  }
}


//================================================
// Init LCD object and LCD panel
//================================================
void LCD::init() {
  pinMode (DC, OUTPUT);
  pinMode (CS, OUTPUT);
  pinMode (RST, OUTPUT);
  csport    = portOutputRegister(digitalPinToPort(CS));
  rsport    = portOutputRegister(digitalPinToPort(DC));
  cspinmask = digitalPinToBitMask(CS);
  rspinmask = digitalPinToBitMask(DC);

  SPI.begin();
  //SPI.setClockDivider (SPI_CLOCK_DIV4); // 4 MHz (half speed)
  //Due defaults to 4mHz (clock divider setting of 21)
  SPI.setBitOrder (MSBFIRST);
  SPI.setDataMode (SPI_MODE0);

  // toggle RST low to reset; CS low so it'll listen to us
  *csport &= ~cspinmask;
  digitalWrite (RST, HIGH);
  delay (10);
  digitalWrite (RST, LOW);
  delay (10);
  digitalWrite (RST, HIGH);
  delay (10);

  commandList (LCDinit);
  digitalWrite (CS, HIGH);

  fillRect (0 ,0, WIDTH, HEIGHT, BLACK);
}


//================================================
// LCD write
//================================================
size_t LCD::write (uint8_t c) {
  if (c == '\n') {
//    fillRect (cursorX, cursorY, WIDTH - cursorX, TEXTHEIGHT, bgColour);  // Blank rest of line
    cursorX  = 0;
    cursorY += TEXTHEIGHT;
    if (cursorY == CURSOR_HEIGHT) return 1;
  } 
  else if (c != '\r') { // Skip '\r'
    drawChar(cursorX, cursorY, c, textColour, bgColour);
    cursorX += TEXTWIDTH;
    if (wrap && (cursorX > (WIDTH - TEXTWIDTH))) {
      cursorX = 0;
      cursorY += TEXTHEIGHT;
      if (cursorY > HEIGHT) cursorY = 1;
    }
  }
  return 1;
}

//================================================
// LCD draw character
//================================================
void LCD::drawChar (int16_t x, int16_t y, unsigned char c, uint16_t colour, uint16_t bg) {
  unsigned char charData;

  if((x >= WIDTH) || (y >= HEIGHT) || ((x + TEXTWIDTH - 1) < 0) || ((y + TEXTHEIGHT - 1) < 0))
    return;

  c -= 32; // Shift ASCII to font array values
  if ((c < 0) || (c > 97)) c = 97;

  uint8_t fhi = colour >> 8, flo = colour & 0xff;
  uint8_t bhi = bg >> 8, blo = bg & 0xff;

  setAddrWindow (x, y, x + TEXTWIDTH - 1, y + TEXTHEIGHT - 1);

  *rsport |= rspinmask;
  *csport &= ~cspinmask;

  for (int8_t i = 0; i < TEXTWIDTH; i++ ) {
    charData = pgm_read_byte (font + (c * (TEXTWIDTH - 1)) + i);

    for (int8_t j = 0; j < TEXTHEIGHT; j++) {

      if ((i < (TEXTWIDTH -1)) && ((charData >> j) & 0x01)) {
        spiwrite (fhi);
        spiwrite (flo);
      } 
      else {         
        spiwrite (bhi);
        spiwrite (blo);
      } 

    }
  }

  *csport |= cspinmask;

}


//================================================
// Filled rectangle
//================================================
void LCD::fillRect (int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour) {

  if ((x >= WIDTH) || (y >= HEIGHT)) return;
  if ((x + --w) >= WIDTH) w = WIDTH  - x - 1;
  if ((y + --h) >= WIDTH) h = WIDTH - y - 1;

  setAddrWindow (x, y, x + w, y + h);

  uint8_t hi = colour >> 8;
  uint8_t lo = colour & 0xff;
  *rsport |=  rspinmask;
  *csport &= ~cspinmask;

  for (y = h; y >= 0; y--) {
    for (x = w; x >= 0; x--) {
      spiwrite (hi);
      spiwrite (lo);
    }
  }
  *csport |= cspinmask;
}


//================================================
// Utility functions
//================================================
void LCD::setCursor (int16_t x, int16_t y) {
  cursorX = x * TEXTWIDTH;
  cursorY = y * TEXTHEIGHT;
}

void LCD::setTextColour (uint16_t c) {
  textColour = c;
}

void LCD::setBGColour (uint16_t b) {
  bgColour = b;
}

int16_t LCD::getTextColour () {
  return textColour;
}

int16_t LCD::getBGColour () {
  return bgColour;
}

void LCD::setTextWrap (boolean w) {
  wrap = w;
}

int16_t LCD::getCursorX() {
  return cursorX;
}

int16_t LCD::getCursorY() {
  return cursorY;
}
