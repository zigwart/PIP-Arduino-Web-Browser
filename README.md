PIP
===
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
 * VRx   A0 - Analogue input 0
 * VRy   A1
 * SW    A2
 * VCC   Arduino 5v
 * GND   Ground
 
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
 Fixed exception for &lt;script> and &lt;style> tags. Improved resiliance
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
