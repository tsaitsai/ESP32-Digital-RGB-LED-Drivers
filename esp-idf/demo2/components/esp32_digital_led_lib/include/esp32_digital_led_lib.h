/*
 * Library for driving digital RGB(W) LEDs using the ESP32's RMT peripheral
 *
 * Modifications Copyright (c) 2017 Martin F. Falatic
 *
 * Based on public domain code created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com
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

#ifndef ESP32_DIGITAL_LED_LIB_H
#define ESP32_DIGITAL_LED_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DEBUG_ESP32_DIGITAL_LED_LIB 0

typedef union {
  struct __attribute__ ((packed)) {
    uint8_t r, g, b, w;
  };
  uint32_t num;
} pixelColor_t;

typedef struct {
  int rmtChannel;
  int gpioNum;
  int ledType;
  int brightLimit;
  int numPixels;
  pixelColor_t * pixels;
  void * _stateVars;
} strand_t;

enum led_types {LED_WS2812, LED_WS2812B, LED_WS2813, LED_SK6812, LED_SK6812W};

typedef struct {
  int ledType;
  int bytesPerPixel;
  uint32_t T0H;
  uint32_t T1H;
  uint32_t T0L;
  uint32_t T1L;
  uint32_t TRS;
} ledParams_t;

const ledParams_t ledParamsAll[] = {  // MUST match order of led_types!
  { .ledType = LED_WS2812,  .bytesPerPixel = 3, .T0H = 350, .T1H = 700, .T0L = 800, .T1L = 600, .TRS =  50000},
//{ .ledType = LED_WS2812B, .bytesPerPixel = 3, .T0H = 350, .T1H = 900, .T0L = 900, .T1L = 350, .TRS =  50000}, // Older datasheet
//{ .ledType = LED_WS2812B, .bytesPerPixel = 3, .T0H = 400, .T1H = 850, .T0L = 850, .T1L = 400, .TRS =  50000}, // 2016 datasheet
  { .ledType = LED_WS2812B, .bytesPerPixel = 3, .T0H = 450, .T1H = 850, .T0L = 850, .T1L = 450, .TRS =  50000}, // cplcpu test
  { .ledType = LED_WS2813,  .bytesPerPixel = 3, .T0H = 350, .T1H = 800, .T0L = 350, .T1L = 350, .TRS = 300000},
  { .ledType = LED_SK6812,  .bytesPerPixel = 3, .T0H = 300, .T1H = 600, .T0L = 900, .T1L = 600, .TRS =  80000},
  { .ledType = LED_SK6812W, .bytesPerPixel = 4, .T0H = 300, .T1H = 600, .T0L = 900, .T1L = 600, .TRS =  80000},
};

extern int digitalLeds_init(strand_t strands [], int numStrands);
extern int digitalLeds_update(strand_t * strand);
extern void digitalLeds_reset(strand_t * pStrand);

inline pixelColor_t pixelFromRGB(uint8_t r, uint8_t g, uint8_t b)
{
  pixelColor_t v;
  v.r = r;
  v.g = g;
  v.b = b;
  v.w = 0;
  return v;
}

inline pixelColor_t pixelFromRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  pixelColor_t v;
  v.r = r;
  v.g = g;
  v.b = b;
  v.w = w;
  return v;
}

#ifdef __cplusplus
}
#endif

#endif /* ESP32_DIGITAL_LED_LIB_H */

