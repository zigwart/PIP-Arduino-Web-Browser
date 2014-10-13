PIP
===

PIP Arduino Web Browser

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
 Attribution-NonCommercial 4.0 International.
 http://creativecommons.org/licenses/by-nc/4.0/
 
 Notes:
 
 Handles a sub-set of HTML tags. See tags.h file for list.
 
 Due to poor Ethernet and SD card interaction on the SPI bus, the cace file
 on the SD card is held open as ong as possible and only closed and re-opened
 during HTML download and parsing. That seems to make things ore reliable.
 
 HTML files are parsed and made partially binary before being stored on the CD card.
 File are generally around 50% smaller when unused junk is removed. The stored 
 file will be truncated to 64KB as 16 bit pointers are being used for page and 
 URL indexing.
 May fix this by going to relative pointers.
 
 Currently, URLs over 130 characters in size are truncated. This will undoubtably
 break them, or results in 404 errors.
 
 Doesn't do images. Come on, there's only 2KB of memory and a tiny screen 
 to play with! JPEG parsing might be problematic as there's only 4KB of 
 program space free.
 
 
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
