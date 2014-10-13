#ifndef _LCD_H
#define _LCD_H

#include "Arduino.h"
#include "Print.h"

#define	BLACK   0x0000
#define	RED     0xF800
#define	GREEN   0x07E0
#define	BLUE    0x001F
#define YELLOW  0xFFE0
#define MAGENTA 0xF81F
#define CYAN    0x07FF  
#define WHITE   0xFFFF
#define GREY    0x632C

#define TEXTWIDTH   6
#define TEXTHEIGHT  8

class LCD : 
public Print {

public:
  LCD(int16_t w, int16_t h);

  void init();

  void fillRect (int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour);
  void drawChar (int16_t x, int16_t y, unsigned char c, uint16_t colour, uint16_t bg);

  virtual size_t write (uint8_t);

  void setCursor (int16_t x, int16_t y);
  void setTextColour (uint16_t c);
  void setBGColour (uint16_t bg);
  int16_t getTextColour ();
  int16_t getBGColour ();
  void setTextWrap (boolean w);
  int16_t getCursorX();
  int16_t getCursorY();

  const int16_t WIDTH, HEIGHT;
  const int16_t CURSOR_WIDTH, CURSOR_HEIGHT;

protected:
  int16_t cursorX, cursorY;
  uint16_t textColour, bgColour;
  boolean wrap; // If set, 'wrap' text at right edge of display
};

#endif // _LCD_H
