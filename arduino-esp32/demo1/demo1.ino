/* 
 * Demo code for digital RGB LEDs using the RMT peripheral on the ESP32
 *
 * Modifications Copyright (c) 2017 Martin F. Falatic
 *
 * Based on public domain code created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com
 *
 * The RMT peripheral on the ESP32 provides very accurate timing of
 * signals sent to the WS2812 LEDs.
 *
 */
/* 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ws2812.h"

#if defined(ARDUINO) && ARDUINO >= 100
  // No extras
#elif defined(ARDUINO) // pre-1.0
  // No extras
#elif defined(ESP_PLATFORM)
  #include "arduinoish.hpp"
#endif

// Required if debugging is enabled in WS2812 header
#if DEBUG_WS2812_DRIVER
int ws2812_debugBufferSz = 1024;
char * ws2812_debugBuffer = (char*)calloc(ws2812_debugBufferSz, sizeof(char));
#endif

strand_t STRANDS[] = { // Avoid using any of the strapping pins on the ESP32
  {.rmtChannel = 0, .gpioNum = 19, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels = 256, .pixels = NULL},
//  {.rmtChannel = 0, .gpioNum = 16, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels = 256, .pixels = NULL},
//  {.rmtChannel = 1, .gpioNum = 17, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels = 256, .pixels = NULL},
//  {.rmtChannel = 2, .gpioNum = 18, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels = 256, .pixels = NULL},
//  {.rmtChannel = 3, .gpioNum = 19, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels = 256, .pixels = NULL},
};

// Forward declarations
void displayOff(strand_t *);
void rainbow(strand_t *, unsigned long, unsigned long);
void scanner(strand_t *, unsigned long, unsigned long);
void dumpDebugBuffer(int, char *);

void dumpDebugBuffer(int id, char * debugBuffer)
{
  Serial.print("DEBUG: (");
  Serial.print(id);
  Serial.print(") ");
  Serial.println(debugBuffer);
  debugBuffer[0] = 0;
}

void setup()
{
  // TODO this is to avoid crosstalk during testing
  pinMode (16, OUTPUT); digitalWrite (16, LOW);
  pinMode (17, OUTPUT); digitalWrite (17, LOW);
  pinMode (18, OUTPUT); digitalWrite (18, LOW);
  pinMode (19, OUTPUT); digitalWrite (19, LOW);

  Serial.begin(115200);
  Serial.println("Initializing...");
  int numStrands = sizeof(STRANDS)/sizeof(STRANDS[0]);
  if(ws2812_init(STRANDS, numStrands)) {
    Serial.println("Init FAILURE: halting");
    while (true) {};
  }
  strand_t * pStrand = &STRANDS[0];
  #if DEBUG_WS2812_DRIVER
    dumpDebugBuffer(-2, ws2812_debugBuffer);
  #endif
  pStrand->pixels = (rgbVal*)malloc(sizeof(rgbVal) * pStrand->numPixels);
  displayOff(pStrand);
  #if DEBUG_WS2812_DRIVER
    dumpDebugBuffer(-1, ws2812_debugBuffer);
  #endif
  Serial.println("Init complete");
}

// Test code
const int TEST_MAX_PASSES = 10;
int test_passes = 0;
void test_loop()
{
  strand_t * pStrand = &STRANDS[0];
  for(uint16_t i = 0; i < pStrand->numPixels; i++) {
    pStrand->pixels[i] = makeRGBVal(1, 1, 1);
  }
  pStrand->pixels[0] = makeRGBVal(2, 1, 3);
  pStrand->pixels[1] = makeRGBVal(5, 4, 6);
  pStrand->pixels[2] = makeRGBVal(8, 7, 9);
  ws2812_setColors(pStrand);
  #if DEBUG_WS2812_DRIVER
    dumpDebugBuffer(test_passes, ws2812_debugBuffer);
  #endif
  delay(1);
  if (++test_passes >= TEST_MAX_PASSES) {
    while(1) {}
  }
}

void loop()
{
  //test_loop(); return;
  strand_t * pStrand = &STRANDS[0];
  rainbow(pStrand, 0, 5000);
  scanner(pStrand, 0, 5000);
  displayOff(pStrand);
  #if DEBUG_WS2812_DRIVER
    dumpDebugBuffer(test_passes, ws2812_debugBuffer);
  #endif
}

void displayOff(strand_t * pStrand)
{
  for (int i = 0; i < pStrand->numPixels; i++) {
    pStrand->pixels[i] = makeRGBVal(0, 0, 0);
  }
  ws2812_setColors(pStrand);
}

void scanner(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  int currIdx = 0;
  int prevIxd = 0;
  bool RUN_FOREVER = (timeout_ms == 0 ? true : false);
  unsigned long start_ms = millis();
  while (RUN_FOREVER || (millis() - start_ms < timeout_ms)) {
    pStrand->pixels[prevIxd] = makeRGBVal(0, 0, 0);
    pStrand->pixels[currIdx] = makeRGBVal(pStrand->brightLimit, pStrand->brightLimit, pStrand->brightLimit);;
    ws2812_setColors(pStrand);
    prevIxd = currIdx;
    currIdx++;
    if (currIdx >= pStrand->numPixels) {
      currIdx = 0;
    }
    delay(delay_ms);
  }
}

void rainbow(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  const uint8_t color_div = 4;
  const uint8_t anim_step = 1;
  const uint8_t anim_max = pStrand->brightLimit - anim_step;
  rgbVal color = makeRGBVal(anim_max, 0, 0);
  rgbVal color2 = makeRGBVal(anim_max, 0, 0);
  uint8_t stepVal = 0;
  uint8_t stepVal2 = 0;

  bool RUN_FOREVER = (timeout_ms == 0 ? true : false);
  unsigned long start_ms = millis();
  while (RUN_FOREVER || (millis() - start_ms < timeout_ms)) {
    color = color2;
    stepVal = stepVal2;
  
    for (uint16_t i = 0; i < pStrand->numPixels; i++) {
      pStrand->pixels[i] = makeRGBVal(color.r/color_div, color.g/color_div, color.b/color_div);
  
      if (i == 1) {
        color2 = color;
        stepVal2 = stepVal;
      }
  
      switch (stepVal) {
        case 0:
        color.g += anim_step;
        if (color.g >= anim_max)
          stepVal++;
        break;
        case 1:
        color.r -= anim_step;
        if (color.r == 0)
          stepVal++;
        break;
        case 2:
        color.b += anim_step;
        if (color.b >= anim_max)
          stepVal++;
        break;
        case 3:
        color.g -= anim_step;
        if (color.g == 0)
          stepVal++;
        break;
        case 4:
        color.r += anim_step;
        if (color.r >= anim_max)
          stepVal++;
        break;
        case 5:
        color.b -= anim_step;
        if (color.b == 0)
          stepVal = 0;
        break;
      }
    }
  
    ws2812_setColors(pStrand);
  
    delay(delay_ms);
  }
}
