// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg 
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2_4G

// Supporting Arduino Forum Topics (closed, read only):
// Good Display ePaper for Arduino: https://forum.arduino.cc/t/good-display-epaper-for-arduino/419657
// Waveshare e-paper displays with SPI: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865
//
// Add new topics in https://forum.arduino.cc/c/using-arduino/displays/23 for new questions and issues

//#####################################################################################################
// Example pf Jean-Marc Zingg is extended by Mikhail Lavrenov to show anti aliased fonts on 4-grey EPD.
// The extension starts here 
//#####################################################################################################
//
// The code below is based on the code by Peter Vullings (projectitis),
// https://github.com/projectitis/ILI9341_t3  which was based on Adafruit GFX (with adaptations)
// 
// Software License Agreement (BSD License)
// Copyright (c) 2012 Adafruit Industries.  All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 

#include "PackedBDF.h"

template<typename GxEPD2_Type, const uint16_t page_height>
class GxEPD2_4G_ext : public GxEPD2_4G_4G_R<GxEPD2_Type, page_height>
{
  private:
    GxEPD2_Type epd2_copy;
  protected:
  //  int16_t _width, _height; // Display w/h as modified by current rotation
  //  int16_t  cursor_x, cursor_y;
  //  uint16_t textcolor, textbgcolor;
  //  uint8_t textsize, rotation;
  //  boolean wrap; // If set, 'wrap' text at right edge of display
  // Variable above are defined in Adafruit_GFX class, and are used by other functions, so don't redefine them here locally to avoid confusion/incompatibility
    uint32_t textcolorPrexpanded, textbgcolorPrexpanded;
    const packedbdf_t *font;
    uint8_t fontbpp = 1;
    uint8_t fontbppindex = 0;
    uint8_t fontbppmask = 1;
    uint8_t fontppb = 8;
    uint8_t* fontalphalut;
    float fontalphamx = 1;

  public:
    GxEPD2_4G_ext(GxEPD2_Type epd2_instance) : GxEPD2_4G_4G_R<GxEPD2_Type, page_height>(epd2_copy), epd2_copy(epd2_instance)
    {
    }
    //#include "glcdfont.c"
    //extern "C" const unsigned char glcdfont[];  // from ILI9341_t3

    void setAAFont(const packedbdf_t &f) {
      font = &f;
      fontbpp = 1;  // sets a default value, meaning 1-bit font, not anti-aliased
      // Calculate additional metrics for Anti-Aliased font support (BDF extn v2.3)
      if (font && font->version==23){ // in case the font is anti-aliased
        fontbpp = (font->reserved & 0b000011)+1;
        Serial.print("setAAFont --> font version == 23, and fontbpp="); // testing debugging
        Serial.println(fontbpp);                                        // testing debugging
        fontbppindex = (fontbpp >> 2)+1;
        fontbppmask = (1 << (fontbppindex+1))-1;
        fontppb = 8/fontbpp;
        fontalphamx = 31/((1<<fontbpp)-1);
        // Ensure text and bg color are different. Note: use setTextColor to set actual bg color
        //if (Adafruit_GFX::textcolor == Adafruit_GFX::textbgcolor) Adafruit_GFX::textbgcolor = (Adafruit_GFX::textcolor==0x0000)?0xFFFF:0x0000;
      }
    } // end of setFont()

    static uint32_t fetchbit(const uint8_t *p, uint32_t index)
    {
      if (p[index >> 3] & (1 << (7 - (index & 7)))) return 1;
      return 0;
    }

    static uint32_t fetchbits_unsigned(const uint8_t *p, uint32_t index, uint32_t required)
    {
      uint32_t val = 0;
      do {
        uint8_t b = p[index >> 3];
        uint32_t avail = 8 - (index & 7);
        if (avail <= required) {
          val <<= avail;
          val |= b & ((1 << avail) - 1);
          index += avail;
          required -= avail;
        } else {
          b >>= avail - required;
          val <<= required;
          val |= b & ((1 << required) - 1);
          break;
        }
      } while (required);
      return val;
    } // end of fetchbits_unsigned

    static uint32_t fetchbits_signed(const uint8_t *p, uint32_t index, uint32_t required)
    {
      uint32_t val = fetchbits_unsigned(p, index, required);
      if (val & (1 << (required - 1))) {
        return (int32_t)val - (1 << required);
      }
      return (int32_t)val;
    } // end of fetchbits_signed

    uint32_t fetchpixel(const uint8_t *p, uint32_t index, uint32_t x){
      // The byte
      uint8_t b = p[index >> 3];
      // Shift to LSB position and mask to get value
      uint8_t s = ((fontppb-(x % fontppb)-1)*fontbpp);
      // Mask and return
      return (b >> s) & fontbppmask;
    }

    void setAACursor(int16_t x, int16_t y) {
	    if (x < 0) x = 0;
	    else if (x >= Adafruit_GFX::_width) x = Adafruit_GFX::_width - 1;
	    Adafruit_GFX::cursor_x = x;
	    if (y < 0) y = 0;
	    else if (y >= Adafruit_GFX::_height) y = Adafruit_GFX::_height - 1;
	    Adafruit_GFX::cursor_y = y;
    }

    void drawAAFontChar(unsigned int c){
      uint32_t bitoffset;
      const uint8_t *data;

      //Serial.printf("drawFontChar %d\n", c);

      if (c >= font->index1_first && c <= font->index1_last) {
        Serial.printf("\ndrawAAFontChar \'%c\'- OK - char within index1 boundaries\n",c);
        bitoffset = c - font->index1_first;
        bitoffset *= font->bits_index;
      } else if (c >= font->index2_first && c <= font->index2_last) {
        Serial.println("\ndrawAAFontChar - OK - char within index2 boundaries");
        bitoffset = c - font->index2_first + font->index1_last - font->index1_first + 1;
        bitoffset *= font->bits_index;
      } else if (font->unicode) {
        Serial.println("\ndrawAAFontChar - NOK - unicode font not suppoted");
        return; // TODO: implement sparse unicode
      } else {
        Serial.println("\ndrawAAFontChar - NOK - font not recognized");
        return;
      }
      Serial.printf("  index =  %d\n", fetchbits_unsigned(font->index, bitoffset, font->bits_index));
      data = font->data + fetchbits_unsigned(font->index, bitoffset, font->bits_index);

      uint32_t encoding = fetchbits_unsigned(data, 0, 3);
      if (encoding != 0) return;
      uint32_t width = fetchbits_unsigned(data, 3, font->bits_width);
      bitoffset = font->bits_width + 3;
      uint32_t height = fetchbits_unsigned(data, bitoffset, font->bits_height);
      bitoffset += font->bits_height;
      
      Serial.printf("  size =   %d,%d\n", width, height);

      int32_t xoffset = fetchbits_signed(data, bitoffset, font->bits_xoffset);
      bitoffset += font->bits_xoffset;
      int32_t yoffset = fetchbits_signed(data, bitoffset, font->bits_yoffset);
      bitoffset += font->bits_yoffset;
      Serial.printf("  offset = %d,%d\n", xoffset, yoffset);

      uint32_t delta = fetchbits_unsigned(data, bitoffset, font->bits_delta);
      bitoffset += font->bits_delta;
      //Serial.printf("  delta =  %d\n", delta);

      Serial.printf("  cursor before starting = %d,%d\n", Adafruit_GFX::cursor_x, Adafruit_GFX::cursor_y);

      // horizontally, we draw every pixel, or none at all
      if (Adafruit_GFX::cursor_x < 0) Adafruit_GFX::cursor_x = 0;
      int32_t origin_x = Adafruit_GFX::cursor_x + xoffset;
      if (origin_x < 0) {
        Adafruit_GFX::cursor_x -= xoffset;
        origin_x = 0;
      }
      if (origin_x + (int)width > Adafruit_GFX::_width) {
        if (!Adafruit_GFX::wrap) return;
        origin_x = 0;
        if (xoffset >= 0) {
          Adafruit_GFX::cursor_x = 0;
        } else {
          Adafruit_GFX::cursor_x = -xoffset;
        }
        Adafruit_GFX::cursor_y += font->line_space;
      }
      if (Adafruit_GFX::cursor_y >= Adafruit_GFX::_height) return;
      Adafruit_GFX::cursor_x += delta;

      // vertically, the top and/or bottom can be clipped
      int32_t origin_y = Adafruit_GFX::cursor_y + font->cap_height - height - yoffset;
      Serial.printf("  origin = %d,%d\n", origin_x, origin_y);
      Serial.printf("  cursor at start = %d,%d  xoffset=%d\n", Adafruit_GFX::cursor_x, Adafruit_GFX::cursor_y,xoffset);
      
      //// TODO: compute top skip and number of lines
      int32_t linecount = height;
      //uint32_t loopcount = 0;
      uint32_t y = origin_y;
      
      // BDF v2.3 Anti-aliased font handled differently
      if (fontbpp>1){
        //Serial.println("drawAAFontChar - pixel values one by one");
        bitoffset = ((bitoffset + 7) & (-8)); // byte-boundary
        uint32_t xp = 0;
        while (linecount) {
          uint32_t x = 0;
          while(x<width) {
            // One pixel at a time
            uint8_t alpha = fetchpixel(data, bitoffset, xp);
            //Serial.printf("[%00dx%00d,%d]",Adafruit_GFX::cursor_x+x,y,alpha);
            switch( alpha ){
              case 3:
                GxEPD2_4G_4G_R<GxEPD2_Type, page_height>::drawGreyPixel(origin_x+x, y, 0x00); // TBC about y vs xp vs origin_y
                break;
              case 2:
                GxEPD2_4G_4G_R<GxEPD2_Type, page_height>::drawGreyPixel(origin_x+x, y, 0x40); 
                break;
              case 1:
                GxEPD2_4G_4G_R<GxEPD2_Type, page_height>::drawGreyPixel(origin_x+x, y, 0x80); 
                break;
              default:
                GxEPD2_4G_4G_R<GxEPD2_Type, page_height>::drawGreyPixel(origin_x+x, y, 0xF0); 
                break;
            }
            bitoffset += fontbpp;
            x++;
            xp++;
          }
          y++;
          linecount--;
        }
      }
      // No anti-alias
      else{ //### DrawFontBits is commented out, so nothing is drawn
        Serial.println("drawAAFontChar - something went wrong");
        //SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));

        int32_t linecount = height;
        uint32_t y = origin_y;
        while (linecount) {
          uint32_t b = fetchbit(data, bitoffset++);
          if (b == 0) {
            uint32_t x = 0;
            do {
              uint32_t xsize = width - x;
              if (xsize > 32) xsize = 32;
              uint32_t bits = fetchbits_unsigned(data, bitoffset, xsize);
              //drawFontBits(bits, xsize, origin_x + x, y);
              bitoffset += xsize;
              x += xsize;
            } while (x < width);
            y++;
            linecount--;
          } else {
            uint32_t n = fetchbits_unsigned(data, bitoffset, 3) + 2;
            bitoffset += 3;
            uint32_t x = 0;
            do {
              uint32_t xsize = width - x;
              if (xsize > 32) xsize = 32;
              //Serial.printf("    multi line %d\n", n);
              uint32_t bits = fetchbits_unsigned(data, bitoffset, xsize);
              //drawFontBits(bits, xsize, origin_x + x, y, n);
              bitoffset += xsize;
              x += xsize;
            } while (x < width);
            y += n;
            linecount -= n;
          }
        }
      }
      //Adafruit_GFX::cursor_x += 1; // leave 1 pixel space after each character.
    } // end of drawFontChar()

    //strPixelLen			- gets pixel length of given ASCII string
    int16_t strPixelLen(char * str)
    {
    //	Serial.printf("strPixelLen %s\n", str);
      if (!str) return(0);
      uint16_t len=0, maxlen=0;
      while (*str)
      {
        if (*str=='\n')
        {
          if ( len > maxlen )
          {
            maxlen=len;
            len=0;
          }
        }
        else
        {
          if (!font)
          {
            Serial.printf("strPixelLen error - font not defined");
            return( 0 );
          }
          else
          {
            uint32_t bitoffset;
            const uint8_t *data;
            uint16_t c = *str;

    //				Serial.printf("char %c(%d)\n", c,c);

            if (c >= font->index1_first && c <= font->index1_last) {
              bitoffset = c - font->index1_first;
              bitoffset *= font->bits_index;
            } else if (c >= font->index2_first && c <= font->index2_last) {
              bitoffset = c - font->index2_first + font->index1_last - font->index1_first + 1;
              bitoffset *= font->bits_index;
            } else if (font->unicode) {
              continue;
            } else {
              continue;
            }
            //Serial.printf("  index =  %d\n", fetchbits_unsigned(font->index, bitoffset, font->bits_index));
            data = font->data + fetchbits_unsigned(font->index, bitoffset, font->bits_index);

            uint32_t encoding = fetchbits_unsigned(data, 0, 3);
            if (encoding != 0) continue;
    //				uint32_t width = fetchbits_unsigned(data, 3, font->bits_width);
    //				Serial.printf("  width =  %d\n", width);
            bitoffset = font->bits_width + 3;
            bitoffset += font->bits_height;

    //				int32_t xoffset = fetchbits_signed(data, bitoffset, font->bits_xoffset);
    //				Serial.printf("  xoffset =  %d\n", xoffset);
            bitoffset += font->bits_xoffset;
            bitoffset += font->bits_yoffset;

            uint32_t delta = fetchbits_unsigned(data, bitoffset, font->bits_delta);
            bitoffset += font->bits_delta;
    //				Serial.printf("  delta =  %d\n", delta);

            len += delta;//+width-xoffset;
    //				Serial.printf("  len =  %d\n", len);
            if ( len > maxlen )
            {
              maxlen=len;
    //					Serial.printf("  maxlen =  %d\n", maxlen);
            }
          
          }
        }
        str++;
      }
    //	Serial.printf("Return  maxlen =  %d\n", maxlen);
      return( maxlen );
    }    // end of strpixellen

    size_t printAA(const String &s)
    {
      size_t n = 0;
      size_t size = s.length();
      while (size--) {
        if (s[n]){ 
          drawAAFontChar(s[n]);
          n++; 
          }
        else break;
      }
      return n;
    }
};

//#########################################################################################################################
// Below is the original code from Jean-Marc Zingg, only change is function  GxEPD2_4G_ext is replaced with GxEPD2_4G_4G
//#########################################################################################################################

// select the display driver class (only one) for your  panel
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_GDEY0154D67 // GDEY0154D67 200x200, SSD1681, (FPC-B001 20.05.21)
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_flex // GDEW0213I5F 104x212, UC8151 (IL0373), (WFT0213CZ16)
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_GDEY0213B74 // GDEY0213B74 122x250, SSD1680, (FPC-A002 20.04.08)
//#define GxEPD2_DRIVER_CLASS GxEPD2_270     // GDEW027W3   176x264, EK79652 (IL91874), (WFI0190CZ22)
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_T5  // GDEW029T5   128x296, UC8151 (IL0373), (WFT0290CZ10)
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_T5D // GDEW029T5D  128x296, UC8151D, (WFT0290CZ10)
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_I6FD // GDEW029I6FD  128x296, UC8151D, (WFT0290CZ10)
//#define GxEPD2_DRIVER_CLASS GxEPD2_371     // GDEW0371W7  240x416, UC8171 (IL0324), (missing)
//#define GxEPD2_DRIVER_CLASS GxEPD2_420     // GDEW042T2   400x300, UC8176 (IL0398), (WFT042CZ15)
#define GxEPD2_DRIVER_CLASS GxEPD2_420_GDEY042T81 // GDEY042T81 400x300, SSD1683 (no inking)
//#define GxEPD2_DRIVER_CLASS GxEPD2_426_GDEQ0426T82 // GDEQ0426T82 480x800, SSD1677 (P426010-MF1-A)
//#define GxEPD2_DRIVER_CLASS GxEPD2_750_T7  // GDEW075T7   800x480, EK79655 (GD7965), (WFT0583CZ61)

// these panels don't support mixed content because of missing partial window update support
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_T94 // GDEM029T94  128x296, SSD1680

// these panels don't support mixed content because clearing the background before changing refresh mode
//#define GxEPD2_DRIVER_CLASS GxEPD2_750_GDEY075T7  // GDEY075T7  800x480, UC8179 (GD7965), (FPC-C001 20.08.20)

// SS is usually used for CS. define here for easy change
#ifndef EPD_CS
#define EPD_CS SS
#endif

#if defined(GxEPD2_DRIVER_CLASS)

  #if defined (ESP8266)
    #define MAX_DISPLAY_BUFFER_SIZE (81920ul-34000ul-5000ul) // ~34000 base use, change 5000 to your application use
    #define MAX_HEIGHT_BW(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
    #define MAX_HEIGHT_4G(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4))
    // adapt the constructor parameters to your wiring
    GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=D8*/ EPD_CS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4));
    // mapping of Waveshare e-Paper ESP8266 Driver Board, new version
    //GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=15*/ EPD_CS, /*DC=4*/ 4, /*RST=2*/ 2, /*BUSY=5*/ 5));
    // mapping of Waveshare e-Paper ESP8266 Driver Board, old version
    //GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=15*/ EPD_CS, /*DC=4*/ 4, /*RST=5*/ 5, /*BUSY=16*/ 16));
    GxEPD2_4G_BW_R<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_BW(GxEPD2_DRIVER_CLASS)> display_bw(display.epd2);
    #undef MAX_DISPLAY_BUFFER_SIZE
    #undef MAX_HEIGHT_BW
    #undef MAX_HEIGHT_4G
  #endif

  #if defined(ESP32)
    #define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.
    #define MAX_HEIGHT_BW(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
    #define MAX_HEIGHT_4G(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4))
    // adapt the constructor parameters to your wiring
    #if defined(ARDUINO_LOLIN_D32_PRO)
      GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=5*/ EPD_CS, /*DC=*/ 0, /*RST=*/ 2, /*BUSY=*/ 15));
      #warning ARDUINO_LOLIN_D32_PRO in display selection .h file
    #elif defined(ARDUINO_ESP32_DEV) // e.g. TTGO T8 ESP32-WROVER
      GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=5*/ EPD_CS, /*DC=*/ 2, /*RST=*/ 0, /*BUSY=*/ 4));
      #warning ARDUINO_ESP32_DEV in display selection .h file
    #else
      //GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=5*/ EPD_CS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));
      // original line above, below setup by MikLav for ESP32 S3 Nano
      GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=5*/ 10, /*DC=*/ 2, /*RST=*/ 3, /*BUSY=*/ 4));
      #warning other ESP32 platform selected in display selection .h file
    #endif
    GxEPD2_4G_BW_R<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_BW(GxEPD2_DRIVER_CLASS)> display_bw(display.epd2);
    #undef MAX_DISPLAY_BUFFER_SIZE
    #undef MAX_HEIGHT_BW
    #undef MAX_HEIGHT_4G
  #endif

  // can't use package "STMF1 Boards (STM32Duino.com)" (Roger Clark) anymore with Adafruit_GFX, use "STM32 Boards (selected from submenu)" (STMicroelectronics)
  #if defined(ARDUINO_ARCH_STM32)
    #define MAX_DISPLAY_BUFFER_SIZE 15000ul // ~15k is a good compromise
    #define MAX_HEIGHT_BW(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
    #define MAX_HEIGHT_4G(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4))
    // adapt the constructor parameters to your wiring
    GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=PA4*/ EPD_CS, /*DC=*/ PA3, /*RST=*/ PA2, /*BUSY=*/ PA1));
    GxEPD2_4G_BW_R<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_BW(GxEPD2_DRIVER_CLASS)> display_bw(display.epd2);
    #undef MAX_DISPLAY_BUFFER_SIZE
    #undef MAX_HEIGHT_BW
    #undef MAX_HEIGHT_4G
  #endif

  #if defined(__AVR)
    #if defined (ARDUINO_AVR_MEGA2560) // Note: SS is on 53 on MEGA
      #define MAX_DISPLAY_BUFFER_SIZE 5000 // e.g. full height for 200x200
    #else // Note: SS is on 10 on UNO, NANO
      #define MAX_DISPLAY_BUFFER_SIZE 800 // 
    #endif
    #define MAX_HEIGHT_BW(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
    #define MAX_HEIGHT_4G(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4))
    // adapt the constructor parameters to your wiring
    GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ EPD_CS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7));
    GxEPD2_4G_BW_R<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_BW(GxEPD2_DRIVER_CLASS)> display_bw(display.epd2);
    #undef MAX_HEIGHT_BW
    #undef MAX_HEIGHT_4G
  #endif

  #if defined(ARDUINO_ARCH_SAM)
    #define MAX_DISPLAY_BUFFER_SIZE 32768ul // e.g., up to 96k
    #define MAX_HEIGHT_BW(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
    #define MAX_HEIGHT_4G(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4))
    // adapt the constructor parameters to your wiring
    GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=10*/ EPD_CS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7));
    GxEPD2_4G_BW_R<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_BW(GxEPD2_DRIVER_CLASS)> display_bw(display.epd2);
    #undef MAX_DISPLAY_BUFFER_SIZE
    #undef MAX_HEIGHT_BW
    #undef MAX_HEIGHT_4G
  #endif

  #if defined(ARDUINO_ARCH_SAMD)
    #define MAX_DISPLAY_BUFFER_SIZE 15000ul // ~15k is a good compromise
    #define MAX_HEIGHT_BW(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
    #define MAX_HEIGHT_4G(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4))
    // adapt the constructor parameters to your wiring
    GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=4*/ 4, /*DC=*/ 7, /*RST=*/ 6, /*BUSY=*/ 5));
    //GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=4*/ 4, /*DC=*/ 3, /*RST=*/ 2, /*BUSY=*/ 1)); // my Seed XIOA0
    //GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=4*/ 3, /*DC=*/ 2, /*RST=*/ 1, /*BUSY=*/ 0)); // my other Seed XIOA0
    GxEPD2_4G_BW_R<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_BW(GxEPD2_DRIVER_CLASS)> display_bw(display.epd2);
    #undef MAX_DISPLAY_BUFFER_SIZE
    #undef MAX_HEIGHT_BW
    #undef MAX_HEIGHT_4G
  #endif

  #if defined(ARDUINO_ARCH_RP2040)
    #define MAX_DISPLAY_BUFFER_SIZE 131072ul // e.g. half of available ram
    #define MAX_HEIGHT_BW(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
    #define MAX_HEIGHT_4G(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 4))
    #if defined(ARDUINO_NANO_RP2040_CONNECT)
      // adapt the constructor parameters to your wiring
      GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ EPD_CS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7));
    #endif
    #if defined(ARDUINO_RASPBERRY_PI_PICO)
      // adapt the constructor parameters to your wiring
      //GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ EPD_CS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7)); // my proto board
      // mapping of GoodDisplay DESPI-PICO. NOTE: uses alternate HW SPI pins!
      GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ 3, /*DC=*/ 2, /*RST=*/ 1, /*BUSY=*/ 0)); // DESPI-PICO
      //GxEPD2_4G_ext<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_4G(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/ 3, /*DC=*/ 2, /*RST=*/ 11, /*BUSY=*/ 10)); // DESPI-PICO modified
    #endif
    GxEPD2_4G_BW_R<GxEPD2_DRIVER_CLASS, MAX_HEIGHT_BW(GxEPD2_DRIVER_CLASS)> display_bw(display.epd2);
    #undef MAX_DISPLAY_BUFFER_SIZE
    #undef MAX_HEIGHT_BW
    #undef MAX_HEIGHT_4G
  #endif

#endif
