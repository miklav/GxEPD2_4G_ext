# GxEPD2_4G_ext
### An example which adds Anti-Aliased Font use to GxEPD2_4G library

Based on "GxEPD2_4G_MixedExample" from library [GxEPD2_4G_ext by ZinggJM](https://github.com/ZinggJM/GxEPD2_4G)

Tested with Waveshare ESP32 S3 Nano board and with WeAct Studio 4.2" epaper module.
*********************************

### Usage:

Download and copy all files to folder "EPD2_4G_MixedExample_ext" on your computer, under your Arduino folder. In Windows it will typically be ...Documents\Arduino\EPD2_4G_MixedExample_ext\
Then you can open EPD2_4G_MixedExample_ext.ino in Arduino IDE, compile and upload the example to your ESP32 board. 

Wiring - see the picture below (VCC can be connected to 3V3 next to D13, GND to GND next to D2)  
_Remark: Diagram shows a different EPD module from WeAct Studio, connection is the same for 4.2" 400x300 EPD_

There are a few differences compared to the original EPD2_4G_MixedExample_ext example:
- Class "GxEPD2_4G_ext" ---> object "display" defined in the project's H file
- Class adds a new method (function) setAAFont, while original method setFont() of GxEPD2_4G remains intact
- Class adds a new function drawAAFontChar()  and printAA()

*********************************
Example is provided "as is", please do not expect any further documentation or any support.

![wiring diagram](ESP32-S3-Nano-SPI_EPD_wiring_ML.jpg)  
Pictures below show anti-aliased text next to the original (not anti-aliased) text
![screen photo 1](IMG20250605112455.jpg)
![screen photo 2](IMG20250605112422.jpg)
