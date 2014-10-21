/*================================================
 ================================================= 
 ####  ###  ####
 #  #   #   #  #
 #  #   #   #  #
 ####   #   ####
 #      #   #
 #      #   #
 #     ###  #
 
 PIP Micro Web Browser v0.7
 Suitable for 32KB Arduino/ATmega use.
 
 (c) Chris Anderson 2014.
 
 Creative Commons license:
 Creative Commons Attribution 4.0 International License
 http://creativecommons.org/licenses/by/4.0/
 
 PIP is a functional but incomplete web browser for Arduino and runs 
 well on an Uno. It can  download and render plain HTML (no images, CSS 
 or Javascript) and follow  embedded links. It's joystick controlled 
 and uses a 320x240 LCD screen  for output. 
 
 The Ethernet and SD card libraries use about 20KB of code, so the LCD 
 driver, HTML parser and renderer squeeze in under 12KB. 
 
 Requires:
 
 * Arduino Uno or similiar ATmega 328 powered board
 * Wiz5100 powered Ethernet board with SD card slot (SPI CS = 10 & 4)
 * 320x240 TFT LCD screen (SPI CS = 9)  Found here: http://tinyurl.com/tftlcd
 * Analogue joystick with button (PS2 style)
 
 Installation:

 * Plug an SD card (MAXIMUM 2GB) into your computer and copy the file 
  "CACHE.HTM" from this enclosing folder into the root level of the card.
 * Download the file "HOME.HTM" from the URL below and put it in the enclosing
   folder. Then copy the file to the root level of the SD card.
   https://www.dropbox.com/s/v8h2d94uxxcw1oj/HOME.HTM?dl=0
 * Put the SD card into your Ethernet shield and upload PIP to your Arduino.
 * Rename the enclosing folder of this file to "PIP_07.ino". 
 * Copy the enclosing folder into you local Arduino sketch folder, as per 
   usual for new programs.
 
 Wiring for LCD:
 Name  Pin
 SCLK  13  Clock
 MOSI  11  Master Out - Don't change or you loose hardware assisted speed
 CS    9   Chip Select
 DC    8   Data/Comand
 RST   7   Reset
 LED   Arduino 3.3v
 VCC   Arduino 5v
 GND   Ground

 Wiring for joystick:
 VRx   A0 - Analogue input 0
 VRy   A1
 SW    A2
 VCC   Arduino 5v
 GND   Ground
 
 Notes:
 
 Handles a sub-set of HTML tags. See tags.h file for list.
 
 HTML pages over 20KB or so can stall and fail somewhat randomly. The Wiz5100 
 just doesn't seem to like dripping data to an Arduino much. Over 1KB/sec 
 download speed can be cosidered 'good'.
 
 Occationally when the browser crashes during a render, the HOME.HTM can get 
 corrupted. Just copy the file in the enclosing folder back onto the SD card.
 
 Due to poor Ethernet and SD card interaction on the SPI bus, the cace file
 on the SD card is held open as long as possible and only closed and re-opened
 during HTML download and parsing. That seems to make things ore reliable.
 
 HTML files are parsed and made partially binary before being stored on the CD card.
 File are generally around 50% smaller when unused junk is removed. The stored 
 file will be truncated to 64KB as 16 bit pointers are being used for page and 
 URL indexing.
 May fix this by going to relative pointers.
 
 Currently, URLs over 90 characters in size are truncated. This will undoubtably
 break them, or results in 404 errors. Which aren't handled well yet.
 
 The LCD screen is used via a custom library (very similar to the Adafruit GFX 
 library) which only implements rectangle and character drawing to save space.
 As only rectangle drawing and the print command are used, it should be easy to 
 adapt PIP to use a different LCD. 
 
 Doesn't do images. Come on, there's only 2KB of memory and a tiny screen 
 to play with! JPEG parsing might be problematic as there's only 2KB of 
 program space free.
 
 Forms are not even attempted. There's no keyboard, so good luck with that.
 
 To do:
 Handle state case when a page break the middle of a tag.
 
 v0.7 14/10/2014
 Fixed exception for <script> and <style> tags. Improved resiliance
 to buffer under-runs with slow ethernet connections.
 
 v0.6 1/10/2014
 Indexes in-HTML links, shows current link, steps through links
 activates links and handles ethernet buffer under-runs
 
 v0.5 17/9/2014
 Parser re-written as purely state-based, fixed SD card/Ethernet 
 conflicts - somwhat
 
 v0.4 3/9/2014
 Rebuilt and renamed browser as PIP, indexes pages, custom LCD
 libray re-written, optimise parser
 
 v0.3 18/4/1014
 Custom LCD driver library added, buggy
 
 v0.2 27/3/2014
 Downloads and parses basic HTML pages
 
 v0.1 13/3/2014
 Download and display concept, Muzayik
 
 ================================================ 
 ================================================*/

#define DEBUG0  false   // Main loop / Setup - Turns of a world of serial output - 
#define DEBUG1  true   // URLloading - You really don't want to turn these on 
#define DEBUG2  false   // Page display - Really. Just don't 

#include <Fat16.h>
#include <Ethernet.h>
#include <SPI.h>
#include "LCD.h"                                 // Custom LCD driver
#include "tags.h"                               // Support HTML tag transcodes

#define CACHEFILE      "CACHE.HTM"              // Cached HTML filename
#define HOMEPAGE       "HOME.HTM"               // First load homepage name
#define PAGEINDEXSIZE  10                       // Must small - each page uses 3 bytes
#define LINKINDEXSIZE  20                       // Must small - each link uses 2 bytes 
#define TIMEOUT        5000                     // Timeout on ethernet reads

//================================================
// Global variables - gotta love 'em
//================================================
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };          // Random MAC address - may need to be more random
IPAddress ip(172,20,30,225);                     // Fallback IP address if DNS doesn't work
EthernetClient client;

char server[30] = "*";                         // THERE HAS TO BE A BETTER WAY OF SPLITTING A URL
char path[60] = "";                            // INTO PARTS USING VARIABLES - JUST TO PASS THE HTTP REQUEST
char url[90] = "";                           // What IS a reasonable maximum URL length?

SdCard card;
Fat16 file;
struct cacheStruct {
  uint8_t    pagePtr, lastPage;                 // Pointers to current and last page indexed
  uint16_t  index[PAGEINDEXSIZE];               // File page index pointers
  uint8_t   pageState[PAGEINDEXSIZE];           // Initial tag state for page e.g. in a <strong>
};
cacheStruct textContent = { 
  0, 0, false };

struct linkStruct {
  uint8_t    linkPtr, lastLink;                  // Pointers to current and last link indexed
  uint16_t   index[LINKINDEXSIZE];               // Link 
};
linkStruct pageLinks = {
  0, 0 };

LCD tftLCD = LCD (320, 240);                      // LCD object instance

uint16_t lowestRAM = 2000;                        // Keeps tabs for low memoery alerts

uint8_t command = 6;                              // Initial browser command to display the homepage
uint16_t jX = 0, jY = 0;                          // Input joystick positions
uint8_t generalBuffer[55];

//================================================
// CPU hard reset
//================================================
void (* resetFunc) (void) = 0;                    //declare reset function at address 0


//================================================
// Setup
//================================================
void setup () {
  pinMode (10, OUTPUT);                           // Force Ethernet pin as output
  pinMode (A2, INPUT_PULLUP);                     // Ground activated switch

  Serial.begin (115200);
  Serial.println (F("Starting..."));
  delay (1000);

  //================================================
  // LCD initialisation stuff
  //================================================
  tftLCD.init();
  tftLCD.setTextColour (GREEN);
  tftLCD.fillRect (0, 0, tftLCD.WIDTH, 8, GREY);
  tftLCD.setCursor (0, 1);
  tftLCD.println (F("PIP Web Browser, v0.7, (c) Chris Anderson 2014"));

  freeRam();

  delay(100);

  //================================================
  // SD card initialisation stuff
  //================================================
  tftLCD.print (F("\nSD card init... "));
  digitalWrite (10, HIGH);                        // Force Ethernet off the SPI bus
  if (!card.init()) {
    tftLCD.println (F("failed, card.init"));
    command = 11;
    return;
  }
  if (!Fat16::init(&card)) {
    tftLCD.println (F("failed, Fat16::init\nResetting"));
    command = 11;
    return;
  }
  tftLCD.println();
  if (!openCacheFile(true)) {
    command = 11;
    return;
  }

  //================================================
  // Get IP address
  //================================================
  tftLCD.print (F("\n\nGetting IP address... "));
  if (Ethernet.begin(mac) == 0) {
    tftLCD.print (F("\nTrying fixed IP"));
#if (DEBUG0)
    Serial.println (F("\nTrying fixed IP"));
#endif
    Ethernet.begin(mac, ip);
  }
  tftLCD.println (Ethernet.localIP());

  delay (1000);
}


//================================================
// Main Loop - Reads joystick and passes on commands
//================================================
void loop () {
  jX = analogRead(A0);                           // Joystick X coord on analogue pin A0
  jY = analogRead(A1);                           // Joystick Y coord on analogue pin A1

    switch (command) {
    //================================================
    // Null command - check the joystick for new commands
    //================================================
  case 0:
    if (digitalRead (A2) == 0) {  // Joystick button on pin A2
#if (DEBUG0)
      Serial.print (F("\n>> Button Clicked: "));
#endif
      buildURL (pageLinks.index[pageLinks.linkPtr]);  // Get URL from page index
#if (DEBUG0)
      Serial.print (F("Built url: "));
      Serial.println (url);
      splitURL (url);
      Serial.print (F("Split url: "));
      Serial.print (server);
      Serial.print (F(" + "));
      Serial.println (path);
#endif

      if (url[0] != '*') {                        // Load default home page
        pageLinks.linkPtr = 0;
        pageLinks.lastLink = 0;
        textContent.pagePtr = 0;
        textContent.lastPage = 0;
        command = 5;
#if (DEBUG0)
        Serial.print (F("Loading URL: "));
        Serial.print (server);
        Serial.println (path);
#endif
      } 
      else {                                      // Something stuffed up
        command = 10; //      command = 5;
        file.sync();
#if (DEBUG0)
        Serial.println ("No URL");
#endif
      }
    }
    else if (jY < 20) command = 1;   // up
    else if (jX > 1000) command = 3; // right
    else if (jY > 1000) command = 2; // down
    else if (jX < 20) command = 4;   // left
    break;

    //================================================
    // Page up - then re-display
    //================================================
  case 1:
#if (DEBUG0)
    Serial.print (F("\n>> Command 1: "));
#endif
    if (textContent.pagePtr > 0) {
      textContent.pagePtr--;
      command = 6;
      pageLinks.lastLink = 0;
#if (DEBUG0)
      Serial.print (F("Page up to page "));
      Serial.println (textContent.pagePtr);
#endif      pageLinks.linkPtr = 0;
    } 
    else { // Can't got up any further
      command = 10;
#if (DEBUG0)
      Serial.println(F("Cancelled"));
#endif
    } 
    break;

    //================================================
    // Page down - then re-display
    //================================================
  case 2:
#if (DEBUG0)
    Serial.print (F("\n>> Command 2: "));
#endif
    if (textContent.pagePtr < textContent.lastPage) 
      textContent.pagePtr++;
    else
      textContent.pagePtr = 0;  // Reset to top

    command = 6;
    pageLinks.linkPtr = 0;
    pageLinks.lastLink = 0;
#if (DEBUG0)
    Serial.print (F("Page down to page "));
    Serial.println (textContent.pagePtr);
#endif
    break;

    //================================================
    // Next link - then re-display to update link highlight
    //================================================
  case 3:
#if (DEBUG0)
    Serial.print (F("\n>> Command 3: Next link: "));
#endif
    if (pageLinks.linkPtr < pageLinks.lastLink) {
      pageLinks.linkPtr++;
      command = 6;
#if (DEBUG0)
      Serial.print (F("Increased link to "));
      Serial.println (pageLinks.linkPtr);
#endif
    } 
    else 
    {
      pageLinks.linkPtr = 0;
      command = 6;
#if (DEBUG0)
      Serial.print (F("Reset link to "));
      Serial.println (pageLinks.linkPtr);
#endif
    }
    break;

    //================================================
    // Prev link - then re-display to update link highlight
    //================================================
  case 4:
#if (DEBUG0)
    Serial.print (F("\n>> Command 4: Previous link: "));
#endif
    if (pageLinks.linkPtr > 0) {
      pageLinks.linkPtr--;
      command = 6;
#if (DEBUG0)
      Serial.print (F("Decreased link to "));
      Serial.println (pageLinks.linkPtr);
#endif
    } 
    else  {
      pageLinks.linkPtr = pageLinks.lastLink;
      command = 6;
#if (DEBUG0)
      Serial.print (F("Reset link to "));
      Serial.println (pageLinks.linkPtr);
#endif
    }
    break;

    //================================================
    // Download, parse and cache url
    //================================================
  case 5:
#if (DEBUG0)
    Serial.println (F("\n>> Command 5: Load & cache URL"));
#endif
    file.sync();
    file.close ();                  // Close read only cache file
    splitURL (url);
    if (cacheURL (server, path)) {  // Download and cache URL
      pageLinks.lastLink = 0;
      pageLinks.linkPtr = 0;
      command = 6;
      //      printCache();
    } 
    else {
      file.close ();
      client.stop();
      tftLCD.println(F("\nDownload & caching failed"));
      memcpy (server, "*\0", 2);
      memcpy (path, '\0', 1);
      memcpy (url, '\0', 1);
      pageLinks.lastLink = 0;
      openCacheFile (true);
      command = 6;
#if (DEBUG0)
      Serial.println("Download & caching failed");
#endif
      //      tftLCD.println (F("Press button to continue"));
      //     while (digitalRead (A2) != 0) ;
      //     command = 11;
      break;
    }
    if (!openCacheFile(true)) {             // Re-open read only cache file
      tftLCD.println (F("Cache open failed"));
      memcpy (server, "*\0", 2);
    } 
    else {
      pageLinks.lastLink = 0;
      command = 6;
    }
    break;

    //================================================
    // Display current page from cached file
    //================================================
  case 6:
#if (DEBUG0)
    Serial.print (F("\n>> Command 6: Display cached page: "));
    Serial.println (textContent.pagePtr);
#endif
    if (!displayPage()) {
      tftLCD.println(F("\nDisplay cache failed"));
#if (DEBUG0)
      Serial.println(F("\nDisplay cache failed"));

#endif
    }
    //    printCache();
    command = 10;
    file.sync();
    break;

#if (DEBUG0)
    //================================================
    // Print cache file to serial - debug only
    //================================================
  case 7:
    Serial.print (F("\n>> Command 7: Print raw cached page: "));
    //    printCache();
    command = 10;
    break;
#endif

    //================================================
    // After work state - joystick awaits reset state
    //================================================
  case 10:
    if ((jX > 400) && (jX < 600) && (jY > 400) && (jY < 600) && digitalRead (A2) != 0)
      command = 0;
    break;

    //================================================
    // Reset Arduino on failure 
    //================================================
  case 11:
    tftLCD.println (F("Press button to reset"));
    while (digitalRead (A2) != 0) ;
    resetFunc();

  default:
    ;
  } 
}


//================================================
//================================================
// Cache URL to SD card file - long routine
//================================================
// Connects to the remote server and downloads the
// data, parses the HTML before storing to the 
// cache file on the SD card
//================================================
byte cacheURL (char *URLserver, char *URLpath) {
  // HTML parser states
#define sLEADIN 0
#define sHEADER 1
#define sTEXT	2
#define sPRE	3
#define sTAG	4
#define sAMP	5
#define sANCHOR 6
#define sSTUFF	7
#define sHREF   8
#define sURL	9
#define sENDTAG 10
#define sQUIET  11
#define sSCRIPT 12
#define sFOOTER 99

#define URL_SIZE = 130

  uint16_t  count = 0;                // Output char count
  uint32_t  downloadCount = 0;        // Download char count
  uint8_t   localState = sTEXT;       // HTML stream state
  uint8_t   metaState = sTEXT;        // HTML output state
  uint8_t   nextState = sTEXT;
  uint16_t  tagHash = 0;              // Hash for tag name
  uint16_t  ampHash = 0;              // Hash for escape characters
  char      url[130];                 // Buffer for building parsed URLs
  uint8_t   i = 0;
  uint8_t   c = 0;                    // Input char
  uint8_t   lastChar = TAG_CR;        // Set to supress leading junk
  uint16_t  outputChar = 0;           // Char to build output
  uint32_t  fileLength = 0;
  uint8_t   cleanChar = 0;
  uint32_t  startTime = 0;

  //================================================
  // Open URL
  //================================================
  tftLCD.setTextColour (GREEN);
  tftLCD.setBGColour (BLACK);
  tftLCD.fillRect (0, 8, tftLCD.WIDTH, 88, BLACK);
  tftLCD.fillRect (0, 96, tftLCD.WIDTH, 1, GREEN);
  tftLCD.fillRect (0, 0, tftLCD.WIDTH, 8, GREY);
  tftLCD.setCursor (0, 1);

  // tftLCD.print (F("\nOpening cache... "));
  //  tftLCD.print (CACHEFILE);
#if (DEBUG1)
  Serial.print (F("\nOpening cache... "));
  Serial.print (CACHEFILE);
#endif
  if (!openCacheFile(false)) {                   // Re-open read only cache file
    tftLCD.println (F("Cache open failed"));
    return 0;
  }
  delay (100);

  tftLCD.println (F("\nConnecting..."));
  tftLCD.print (URLserver);
  tftLCD.println (URLpath);
  if (URLserver[0] == '*') {                      // Should never get an * here
    tftLCD.println (F("Invalid URL"));
    return 0;
  } 
  else {
#if (DEBUG1)
    freeRam();
    Serial.print ("URL: ");
    Serial.print (URLserver);
    Serial.print (F(" + "));
    Serial.println (URLpath);
#endif
  }

  if (client.connect(URLserver, 80)) {            // Connect to server and request HTML
    client.print (F("GET "));
    client.print (URLpath);
    client.print (F(" HTTP/1.1\nHost: "));
    client.println (URLserver);
    client.println (F("User-Agent: Mozilla/4.0 (Mobile; PIP/7.0) PIP/7.0\nConnection: close\n\n"));
  } 
  else {
    tftLCD.println(F(" failed"));
#if (DEBUG1)
    Serial.println (F("BOOM!\nConnect failed"));
#endif
    command = 11;
    return 0;
  }

  delay(500);                                    // Give the Ethernet shield a mo to get going
  if (!inputWait()) {
    Serial.println (F("\nEthernet timeout"));
    return 0;
  }
  tftLCD.print (F("\n\nParsing..."));
#if (DEBUG1)
  Serial.println (F("\nParsing..."));
#endif
  startTime = millis();

  {
    memcpy (generalBuffer, "HTTP/1.1 \0", 10);
    outputChar = findUntil (generalBuffer, true);
    if (outputChar == 2) {
      fileLength = client.parseInt();
#if (DEBUG1)
      Serial.print (F("Result: ")); 
      Serial.println (fileLength);
#endif
      if (fileLength >= 300) {
        command = 6;
        return 0;
      }
    }
    memcpy (generalBuffer, "-Length: \0", 10);
    outputChar = findUntil (generalBuffer, true);
#if (DEBUG1)
    Serial.print ("\n");
    Serial.println (outputChar);
#endif

    if (outputChar == 2) {
      fileLength = client.parseInt();
      if (fileLength < 10) fileLength = 0;
#if (DEBUG1)
      if (fileLength > 0) {
        Serial.print (F("File length: "));
        Serial.println (fileLength);
      }
#endif
    } 
    else if (outputChar == 1) {
#if (DEBUG1)
      Serial.println (F("\nStopped by <, no file length found"));
#endif
      fileLength = 0;
    } 
    else {    
#if (DEBUG1)
      Serial.println (F("\nTimeout finding file length"));
#endif
      fileLength = 0;
    }

    memcpy (generalBuffer, "<body\0", 7); 
    outputChar = findUntil (generalBuffer, false);
    if (outputChar = 0) {
      Serial.println (F("\nTimeout finding <body>\nShould probably stop now")); 
    } 
    else localState = sENDTAG;
  }
  c = client.read();

  //================================================
  // Loading loop
  //================================================
  while (metaState < sFOOTER) {
    c = 0;

    if (client.available()) {
      //      freeRam();
      c = client.read();
      downloadCount++;
    } 
    else if (!inputWait()) {
      client.stop();
      Serial.println(F(" DONE"));
      file.sync();
      return 0;
    }

    nextState = localState;
    outputChar = c;

#if (DEBUG1)
    if (metaState > sLEADIN) {
      Serial.print (F("|\n"));
      Serial.print (client.available());
      Serial.print (F("\t"));
      Serial.print (count);
      Serial.print (F("\tmS "));
      Serial.print (metaState);
      Serial.print (F("\tlS "));
      Serial.print (localState);
      Serial.print (F("\t"));
      if (lastChar > 31) Serial.write (lastChar);
      else Serial.print (lastChar, DEC);
      Serial.print (F("\t"));
      if (c > 31) Serial.write (c);
      else Serial.print (c, DEC);
      Serial.print (F("\t|"));
    }
#endif


    //================================================
    // State is sAMP &amp;                           4
    //================================================
    if ((c == '&') && ((localState == sTEXT) || (localState == sURL))) {  // Enter state condition
      if (metaState == sANCHOR) url[i++] = c;
      localState = sAMP;
      ampHash = 0;
      continue; 
    }
    if (localState == sAMP) {
      if (c == ';') {                                 // End condition
#if (DEBUG2)
        Serial.print (" OutputChar = ");
        Serial.println (hashOut (ampHash), DEC);
#endif

        if (metaState != sANCHOR) {
          outputChar = hashOut (ampHash);

          if (outputChar == 0) {
            outputChar = 129;  // Unkown character
            nextState = sTEXT;
          }
          else nextState = sTEXT;
        }         
        else outputChar = 0;
        if (metaState == sANCHOR) nextState = sURL;
      } 
      else { 
        ampHash = (ampHash << 2) + ((int) c) - 32;
        outputChar = 0;
        continue; 
      }
    }


    //================================================
    // State is sTAG                                 3
    //================================================
    if (c == '<') {                                    // Enter state condition
      localState = sTAG;
      nextState = sTAG;
      if (metaState != sQUIET) metaState = sTEXT;
      tagHash = 0;
      continue; 
    }

    if (localState == sTAG) {
      nextState = sTAG;
      if ((c == '>') || (c == ' ') || (c == '\t')) {    // End condition
#if (DEBUG1)
        Serial.print (" end tag ");
        Serial.print (tagHash);
#endif
        nextState = sTEXT;

        if (tagHash == BODYTAGEND) {
          metaState = sFOOTER;
          continue; 
        }

        if ((tagHash == SCRIPTTAGEND) || (tagHash == STYLETAGEND)) {
#if (DEBUG1)
          Serial.print (F(" SCRIPT/STYLE TAG end"));
#endif
          nextState = sTEXT;
          localState = sTEXT; 
          metaState = sTEXT; 
          continue;
        }
        if ((tagHash == SCRIPTTAG) || (tagHash == STYLETAG)) {
#if (DEBUG1)
          Serial.print (F(" SCRIPT/STYLE TAG start "));
#endif
          if (c == '>') localState = sTEXT;
          else localState = sENDTAG;
          metaState = sQUIET;
          tagHash = 0;
          continue;
        }

        if (tagHash == PRETAG) { 
          nextState = sTEXT;
          metaState = sPRE;
        }
        if (tagHash == PRETAGEND) {
          nextState = sTEXT; 
          metaState = sTEXT;
        }
        outputChar = hashOut (tagHash);
        if (c != '>') 
          if (tagHash == 65) nextState = sSTUFF;         // Is actually an <a tag
          else nextState = sENDTAG;                      // Tag not supported
        tagHash = 0;

#if (DEBUG1)
        Serial.print (" Next state: ");
        Serial.print (nextState);
#endif
      }
      else {
        tagHash = (tagHash << 1) + ((int) lowerCase (c)) - 32;
        outputChar = 0;
      }
    }

    //================================================
    // State is sSTUFF in an <a  tag before href="   7
    //================================================
    if (localState == sSTUFF) {
      if ((c != ' ') && (c != '\t')) {                // End condition
        tagHash = lowerCase (c) - 32;
        nextState = sHREF;
      }
      else if (c == '>') {                            // Error in tag termination, wasn't an anchor
        nextState = sTEXT;
        tagHash = 0;
      }
      outputChar = 0;
    }

    //================================================
    // State is sHREF href="                         8
    //================================================
    if (localState == sHREF) {
      if (tagHash == HREFTAG) {                        // End condition
        nextState = sURL;
        metaState = sANCHOR;
        i = 0;
      } 
      else if ((c == '"') || (c == '\'') || (c == '>')) {  // No href match
        nextState = sSTUFF;
        tagHash = 0;
      }
      else
        tagHash = (tagHash << 1) + ((int) toLowerCase (c)) - 32;
      outputChar = 0;
    }

    //================================================
    // State is sURL in href="url"                   9
    //================================================
    if (localState == sURL) {
      if ((c == '"') || (c == '\'')) {                 // End condition
        url[i++] = 0;
        nextState = sENDTAG;
        storeURL(url);
        file.write (TAG_LINK2);
#if (DEBUG1)
        Serial.print (TAG_LINK2);
#endif
      }
      else {
        url[i++] = c;
        url[i] = 0;
        if (i > 130) {
          nextState = sENDTAG;
          file.write (TAG_LINK2);
#if (DEBUG1)
          Serial.print (TAG_LINK2);
#endif
        }
      }
      outputChar = 0;
    }


    //================================================
    // State is sENDTAG after href="url"             10
    //================================================
    if (localState == sENDTAG) {
      if ((c == '>') && (metaState != sQUIET)) {      // End condition
        metaState = sTEXT;
        nextState = sTEXT; 
      } 
      else
        continue;
      outputChar = 0;
    }

    //================================================
    // State is sTEXT - Output stage                10
    //================================================
    cleanChar = outputChar & ATTR_MASK;

    if ((cleanChar == 10) || (cleanChar == '\t')) {
      uint32_t speed = 0;

      tftLCD.setCursor (0, 7);
      tftLCD.print (downloadCount);
      tftLCD.print (F(" bytes downloaded @ "));
      speed = downloadCount / ((millis() - startTime) / 1000);
      tftLCD.print (speed);
      tftLCD.println (F(" bytes/sec       "));

      if (fileLength > 0) {
        uint32_t percent = 0;
        tftLCD.print (fileLength);
        tftLCD.println (F(" bytes file size   "));
        percent = downloadCount * 100 / fileLength;
        //        tftLCD.print (percent);
        //         tftLCD.println (F("% downloaded"));
        tftLCD.fillRect (0, 0, percent * tftLCD.WIDTH / 100, 8, GREEN);
      }    

      tftLCD.print (count);
      tftLCD.println (F(" bytes stored   "));
    }

    // Handle tags with pre breaks
    if ((outputChar & PRE_BREAK)) {
      file.write (10);
      count++;
#if (DEBUG1)
      Serial.write ('%');
#endif
    }

    // Write \n and \r verbatim inside PRE tag
    if (metaState == sPRE) {
      if (cleanChar == 13) cleanChar = 0;
      if (cleanChar == 10) cleanChar = TAG_CR;
    }

    // Convert any \n \r \t to spaces for possible supression
    else {
      if ((cleanChar == 10) || (cleanChar == 13) || (cleanChar == '\t')) {
        if (lastChar != TAG_CR) {
          cleanChar = ' ';
#if (DEBUG1)
          Serial.print (cleanChar);
          Serial.print (F(" Set ' '"));
#endif
        } 
        else {
          cleanChar = 0;
        }
      }
      if ((cleanChar == ' ') || (cleanChar == TAG_CR))
        if ((lastChar == ' ') || (lastChar == '>') || (lastChar == TAG_CR) || (cleanChar < 32)) {
          cleanChar = 0;
          /*#if (DEBUG1)
           Serial.print (F(" set 0\t"));
           Serial.print (lastChar);
           Serial.print (F("\t"));
           Serial.print (outputChar, DEC);
           #endif*/
        }
    }

    // Write out the charater
    if (cleanChar > 0) {
      count++;

      if (cleanChar > 129) cleanChar = 129;

      if ((cleanChar == TAG_CR) && !(outputChar & PRE_BREAK)) {
#if (DEBUG1)
        Serial.print ('@');
#endif
        file.write (10);
      }
      else if (cleanChar > 31) {
        if (metaState != sQUIET) {
          file.write (cleanChar);
#if (DEBUG1)
          Serial.write (cleanChar);
#endif
        }
      } 
      else {
        file.write (cleanChar);
        count++;
#if (DEBUG1)
        Serial.print (cleanChar, DEC);
#endif
      }
      lastChar = cleanChar;
    } 
    else if (outputChar & PRE_BREAK) lastChar = TAG_CR;

    // Handle tags with pre breaks
    if ((outputChar & POST_BREAK) && (lastChar != TAG_CR)) {
      file.write (10);
      count++;
#if (DEBUG1)
      Serial.print ('#');
#endif
      lastChar = TAG_CR;
    }

    localState = nextState;
  } // Loading loop

  file.print ('\0');
  client.flush();
  client.stop();

  tftLCD.println ();
  tftLCD.print (count);
  tftLCD.print (F(" bytes"));

  file.close ();
#if (DEBUG1)
  Serial.print (F("\nCount: "));
  Serial.println (count, DEC);
  freeRam();
#endif
  return count;
}


//================================================
//================================================
// Display page - long routine
//================================================
//================================================
byte displayPage () {
  uint16_t filePtr = textContent.index[textContent.pagePtr];  // Pointer into cached file
  uint8_t c = 1;                                      // Input character
  uint16_t count = 0;                                 // Character count for page index building
  uint16_t colourStack = WHITE;                       // One level stack to remember colour state
  boolean invisiblePrint = false;                     // Turn off output (for URLs, etc)
  uint8_t currentLink = 0;                            // Curretly highlightd link of the page
  boolean buildingIndex = (pageLinks.lastLink == 0);  // True if this page's URL haven't been index yet

  // Draw header
  tftLCD.fillRect (0, 0, tftLCD.WIDTH, 8, GREY);
  tftLCD.setCursor (0, 0);
  tftLCD.setBGColour(GREY);
  tftLCD.setTextColour(BLACK);
  if (server[0] == '*') tftLCD.println (F("Home page"));
  else tftLCD.println (url);

  if (!file.seekSet (filePtr)) {
    tftLCD.println (F("Seek failture"));
    return 0;
  }

#if (DEBUG2)
  freeRam();
  Serial.print (F("SD File position is: "));
  Serial.println (file.curPosition());
  if (buildingIndex) Serial.println (F("Building link index"));
#endif

  tftLCD.setCursor (0, 1);
  tftLCD.setBGColour(BLACK);
  tftLCD.setTextColour(WHITE);

  // Loop through cache file
  while ((filePtr <= file.fileSize()) && (tftLCD.getCursorY() < tftLCD.HEIGHT - 1) && (c != 0)) {
    c = file.read();
    filePtr++;
    count++;

    // Print only visible ASCII characters
    if (c > 31) {
      if (!invisiblePrint) {  // Print only if in visible mode (supresses URLs)
        tftLCD.write (c);
#if (DEBUG2)
        Serial.write(c);
#endif
      } 
      else {
#if (DEBUG2)
        //  Serial.print (F("_"));
#endif
        ;
      }
    }

    // Handle display codes
    else {
      switch (c) {
      case TAG_CR:
      case 10:
        // Fill the rest of the current line with black to erase the previous contents
        tftLCD.fillRect (tftLCD.getCursorX(), tftLCD.getCursorY(), tftLCD.WIDTH - tftLCD.getCursorX(), 8, BLACK);
        tftLCD.write(10);
#if (DEBUG2)
        Serial.println();
#endif
        invisiblePrint = false;
        break;
      case 13:
        // Only doing a CR/LF on ASCII 10
        break;

        // Start of a header tag
      case TAG_HEADING1:
        colourStack = tftLCD.getTextColour();      // Store the colour state
        tftLCD.setTextColour (COL_HEADING);
        break;

        // Start of a bold tag
      case TAG_BOLD1:
        colourStack = tftLCD.getTextColour();
        tftLCD.setTextColour (COL_BOLD);
        break;

        // Start of an anchor tag. The actual URL follows
      case TAG_LINK1:
        invisiblePrint = true;                      // Turn off printing while the URL is loading
        if ((buildingIndex) && (currentLink < LINKINDEXSIZE)) {
          pageLinks.index[currentLink] = filePtr;
          pageLinks.lastLink++;
        }
        break;

        // End of the URL part. Start of the anchor texyt
      case TAG_LINK2:
        colourStack = tftLCD.getTextColour();
        if (currentLink == pageLinks.linkPtr) {  // Print inverse colours for the active link
          tftLCD.setTextColour (BLACK);
          tftLCD.setBGColour (COL_LINK);
        } 
        else 
          tftLCD.setTextColour (COL_LINK);
        invisiblePrint = false;                  // Turn on printing again
        break;

        // End of the anchor tag
      case TAG_LINK3:
        if (currentLink < LINKINDEXSIZE) currentLink++; // Removed && buildingLink
        tftLCD.setTextColour (colourStack);
        tftLCD.setBGColour (BLACK);
        colourStack = WHITE;
        break;

        // End of header or bold tags
      case TAG_HEADING2:
      case TAG_BOLD2:
        tftLCD.setTextColour (colourStack);
        break;

        // List entry tag
      case TAG_LIST:
        tftLCD.write (127);
        break;

        // Horizontal rule tag
      case TAG_HR:
        tftLCD.fillRect (8, tftLCD.getCursorY(), tftLCD.WIDTH - 16, 1, GREY);
#if (DEBUG2)
        Serial.println (F("------"));
        Serial.println (tftLCD.getCursorX());
        Serial.println (tftLCD.getCursorY());
#endif
      default:
        ;
      }
    }
  }
  tftLCD.println();

  // Fill the rest of the screen under the last line with black
  tftLCD.fillRect (0, tftLCD.getCursorY(), tftLCD.WIDTH, tftLCD.HEIGHT  - tftLCD.getCursorY() - 8, BLACK);

#if (DEBUG2)
  Serial.print (F("\nCurrent page: "));
  Serial.println (textContent.pagePtr);

  Serial.print (F("Character count: "));
  Serial.print (count);


  if (textContent.pagePtr == textContent.lastPage)
    Serial.println (F("pagePtr == lastPage")); 
  else 
    Serial.println (F("pagePtr < lastPage")); 
  if (textContent.pagePtr < PAGEINDEXSIZE)
    Serial.println (F("pagePtr < PAGEINDEXSIZE"));
  if (filePtr++ < file.fileSize())
    Serial.println (F("filePtr++ < SDFile.size"));
#endif

  // Update page index
  if ((textContent.pagePtr == textContent.lastPage) && (textContent.pagePtr < PAGEINDEXSIZE) && (filePtr++ < file.fileSize())) {
    textContent.lastPage++;
    textContent.index[textContent.lastPage] = filePtr - 2; //textContent.index[textContent.lastPage] + count;
  }

  // Update last link pointer
  if ((buildingIndex) && (pageLinks.lastLink > 0)) pageLinks.lastLink--;

#if (DEBUG2)
  displayPageIndex();
  displayLinkIndex ();

  Serial.print (F("Char count:\t"));
  Serial.println (count);
  Serial.print (F("filePtr:\t"));
  Serial.println (filePtr);
  Serial.print (F("SD size:\t"));
  Serial.println (file.fileSize());
  Serial.print (F("======"));
#endif

  // Draw RAM use and page position in header
  tftLCD.setCursor (43, 0);
  tftLCD.setBGColour(GREY);
  tftLCD.setTextColour(BLACK);
  tftLCD.print (lowestRAM);
  tftLCD.setCursor (46, 0);
  tftLCD.print (F(" p"));
  tftLCD.print (textContent.pagePtr + 1);
  tftLCD.print (F("/"));
  tftLCD.println (textContent.lastPage + 1);
  return 1;
}


//================================================
// Convert ASCII upper to lower case for a-z
//================================================
char lowerCase (char c) {
  if ((c > 64) && (c < 91)) return c += 32;
  else return c;
}


//================================================
// Convert hash number to tags & flags
//================================================
uint16_t hashOut (uint16_t hash) {
  uint16_t output = 0;

  for (byte i = 0; i < NUMTAGS; i++) {
    if (hash == pgm_read_word (&tag_codes[i++])) {
      output = pgm_read_word (&tag_codes[i]);              
      i = NUMTAGS;
    }
  }
  return output;
}

//================================================
// Store an extracted URL
//================================================
void storeURL (char *local_url) {
  byte i = 0;
  byte j = 0;

  if ((local_url[5] == '/') && (local_url[6] == '/')) {
    j = 6;     // Strip leading http://
    local_url[i++] = TAG_HTTP;
  }
  else if ((local_url[6] == '/') && (local_url[7] == '/')) {
    j = 7;  // Strip leading https://
    local_url[i++] = TAG_HTTP;
  }

  while (local_url[i] > 0)
    local_url [i++] = local_url [i + j];

#if (DEBUG1)
  Serial.print (local_url);
  Serial.print (F("|"));
#endif

  file.print (local_url);
  freeRam();
}


//================================================
// Split a URL into server / path
//================================================
// This routine should be simpler but there are a
// surprising number of different URL formatting
// cases that need to be handled.
//================================================
void splitURL (char *localURL) {
  byte i = 0;
  byte urlIndex = 0;
  byte hasDot = 0;
  byte hasSlash = 0;

#if (DEBUG2)
  Serial.print (F("\nURL to Split: "));
  Serial.println (localURL);
#endif

  if (localURL[urlIndex] == TAG_HTTP) {
    urlIndex++;
    while ((localURL[urlIndex] > 0) && (localURL[urlIndex] != '/'))
      server[i++] = localURL[urlIndex++];
    server[urlIndex] = 0;
    server[i] = 0;
    if (localURL[urlIndex] > 0) {
      i = 0;
      while ((localURL[urlIndex] > 0) && (i < 60))
        path[i++] = localURL[urlIndex++];
      path[i] = 0;
    } 
    else {
      memcpy(path, "/\0", 2);
    }
  }
  else if (localURL[urlIndex] == '/') {  // URL starts with a slash
    while (localURL[urlIndex] > 0)
      path[i++] = localURL[urlIndex++];
    localURL[urlIndex] = 0;
  } 
  else {  // URL is plain
    while (localURL[urlIndex] > 0) {
      if (localURL[urlIndex] == '.') hasDot++;
      if (localURL[urlIndex++] == '/') hasSlash++;
    }
#if (DEBUG2)
    Serial.print ("Dots:");
    Serial.println (hasDot);
    Serial.print ("Slashes:");
    Serial.println(hasSlash);
#endif

    if (hasDot == 1) {
      path[0] = '/';
      urlIndex = 0;
      i = 1;
      do {
        path[i++] = localURL[urlIndex];
      } 
      while (localURL[urlIndex++] > 0);

    } 
    else if ((hasSlash > 0) && (hasDot > 1)) {
      urlIndex = 0;
      i = 0;
      while ((localURL[urlIndex] > 0) && (localURL[urlIndex] != '/'))
        server[i++] = localURL[urlIndex++];
      server[urlIndex] = 0;
      i = 0;
      while (localURL[urlIndex] > 0)
        path[i++] = localURL[urlIndex++];
      path[i] = 0;
    } 
    else {
      urlIndex = 0;
      i = 0;
      while (localURL[urlIndex] > 0)
        server[i++] = localURL[urlIndex++];
      server[urlIndex] = 0;
      memcpy (path, "/\0", 2);
    }
  }
#if (DEBUG2)
  Serial.print (F("URL: "));
  Serial.println (server);
  Serial.println (path);
#endif
  freeRam();
}


//================================================
// Builds a URL from data in the cache
//================================================
void buildURL (uint16_t pointer) {
  byte i = 0;
  char c = 0;
  uint16_t oldPos = file.curPosition();

  if (!file.seekSet (pointer)) {
    tftLCD.println (F("Seek failure"));
#if (DEBUG1)
    Serial.println (F("Seek failure"));
#endif
    return;
  }

  c = file.read();
  while (c != TAG_LINK2) {
    url [i++] = c;
    c = file.read();
  }

  url[i] = 0;
  file.seekSet (oldPos);
  freeRam();
}

#if (DEBUG2)
//================================================
// Display pageindex
//================================================
void displayPageIndex () {
  Serial.print (F("\nPage index:"));
  Serial.print (F("\npagePtr:\t"));
  Serial.println (textContent.pagePtr);
  Serial.print (F("lastPage:\t"));
  Serial.println (textContent.lastPage);
  Serial.println(F("Page Index"));

  for (byte i = 0; i <= textContent.lastPage; i++) {
    Serial.print (i);
    Serial.print (F("\t"));
    Serial.println (textContent.index[i]);
  }
  Serial.println();
  freeRam();
}


//================================================
// Display linkindex
//================================================
void displayLinkIndex () {
  Serial.print (F("\nLink index:"));
  Serial.print (F("\nlinkPtr:\t"));
  Serial.println (pageLinks.linkPtr);
  Serial.print (F("lastLink:\t"));
  Serial.println (pageLinks.lastLink);

  if (pageLinks.lastLink == 0) return;

  Serial.println(F("Link Index"));
  for (byte i = 0; i <= pageLinks.lastLink; i++) {
    Serial.print (i);
    Serial.print (F("\t"));
    Serial.print (pageLinks.index[i]);
    Serial.print (F("\t"));
    file.seekSet (pageLinks.index[i]);
    byte c = 0, j = 0;
    c = file.read();
    while ((c != TAG_LINK2) && (j++ < 100)) {
      Serial.write (c);
      c = file.read();
    }
    Serial.println();
  }
  Serial.println();
  freeRam();
}
#endif


#if (DEBUG0)
//================================================
// Print cache to serial - debug only
//================================================
uint8_t printCache () {
  int16_t c;
  uint16_t count = 0;

  if (!file.isOpen()) {
    Serial.println (F(" failed, not open"));
    return 0;
  }
  file.seekSet (0);

  Serial.print (F("\nFile size: "));
  Serial.println (file.fileSize());

  while ((c = file.read()) > 0) {
    Serial.print (count++);
    Serial.print (F("\t"));
    if (c > 31) {
      Serial.write((char)c);
      Serial.println();
    }
    else {
      Serial.print ("*");
      Serial.println (c);
    }
  }

  Serial.println (F("\nDone"));
  freeRam();
}
#endif


//================================================
// Open cache file with read or write option
//================================================
uint8_t openCacheFile (boolean readOnly) {
  char cacheFile[10];
  uint8_t openMask = O_RDWR | O_TRUNC | O_CREAT;

  if (readOnly) openMask = O_READ;

  if (server[0] == '*') memcpy (cacheFile, HOMEPAGE, sizeof(HOMEPAGE));
  else memcpy (cacheFile, CACHEFILE, sizeof(CACHEFILE));

  //  tftLCD.setCursor (0, 1);
  tftLCD.print (F("\nOpening cache... "));
  tftLCD.print (cacheFile);

#if (DEBUG2)
  Serial.print (F("Opening cache... "));
  Serial.println (cacheFile);
#endif

  digitalWrite (10, HIGH);

  if (!file.open (cacheFile, openMask)) {
    tftLCD.println (F(" Failed on open"));
    return 0;
  }
  if (!file.isOpen()) {
    tftLCD.println (F(" Failed, not open"));
    return 0;
  }
#if (DEBUG2)
  Serial.println();
#endif
  return 1;
}


//================================================
// Memory free tool
//================================================
void freeRam () {
  extern int __heap_start, *__brkval; 
  uint16_t v; 
  v = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
#if (DEBUG1)
  Serial.print (F("Free RAM: "));
  Serial.print (v);
#endif
  if (v < lowestRAM) {
    lowestRAM = v;
#if (DEBUG1)
    Serial.print (F(" KB (Lowest: "));
    Serial.print (lowestRAM);
    Serial.println (F(")"));
#endif
  } 
#if (DEBUG1)
  else Serial.println();
#endif

}


//================================================
// Consumes data from Ethernet until the given 
// string is found, or time runs out
//================================================
byte findUntil (uint8_t *string, boolean terminate) {
  char currentChar = 0;
  long timeOut = millis() + 5000;
  char c = 0;

#if (DEBUG1)
  Serial.print (F("\nLooking for: "));

  for (byte i = 0; string[i] != 0; i ++)
    Serial.write (string[i]);
  Serial.println('\n');
#endif

  while (millis() < timeOut) {
    if (client.available()) {
      timeOut = millis() + 5000;
      c = client.read();
      if (terminate && (c == '<')) return 1;  // Pre-empted match
#if (DEBUG1)
      Serial.write (c);
#endif
      if (c == string[currentChar]) {
        if (string[++currentChar] == 0) {
#if (DEBUG1)
          Serial.println(F("[FOUND]"));
#endif
          return 2;                          // Found
        }
      } 
      else currentChar = 0;
    } 
  }
  return 0;                                  // Timeout
}


//================================================
// Delays until Ethernet data is available
//================================================
boolean inputWait() {
  byte wait = 0;
  long timeOut = millis() + 9000;             // Allow 5 seconds of no data
#if (DEBUG1)
  Serial.println ();
#endif
  while ((millis() < timeOut) && (!client.available())) {
#if (DEBUG1)
    Serial.print (" ");
    Serial.print (wait * 100);
#endif
    delay(++wait * 100);
  }
  if ((!client.available()) && (!client.connected())) return 0;
  else return 1;
}
