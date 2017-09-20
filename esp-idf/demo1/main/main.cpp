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
  char * ws2812_debugBuffer = static_cast<char*>(calloc(ws2812_debugBufferSz, sizeof(char)));
#endif

void gpioSetup(int gpioNum, int gpioMode, int gpioVal) {
  #if defined(ARDUINO) && ARDUINO >= 100
    pinMode (gpioNum, gpioMode);
    digitalWrite (gpioNum, gpioVal);
  #elif defined(ESP_PLATFORM)
    gpio_num_t gpioNumNative = static_cast<gpio_num_t>(gpioNum);
    gpio_mode_t gpioModeNative = static_cast<gpio_mode_t>(gpioMode);
    gpio_pad_select_gpio(gpioNumNative);
    gpio_set_direction(gpioNumNative, gpioModeNative);
    gpio_set_level(gpioNumNative, gpioVal);
  #endif
}

strand_t STRANDS[] = { // Avoid using any of the strapping pins on the ESP32
  //{.rmtChannel = 0, .gpioNum = 16, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels = 256,
  // .pixels = NULL, ._stateVars = NULL},
  {.rmtChannel = 1, .gpioNum = 17, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels =  93,
   .pixels = NULL, ._stateVars = NULL},
  {.rmtChannel = 2, .gpioNum = 18, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels =  93,
   .pixels = NULL, ._stateVars = NULL},
  {.rmtChannel = 3, .gpioNum = 19, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels =  64,
   .pixels = NULL, ._stateVars = NULL},
};
int STRANDCNT = sizeof(STRANDS)/sizeof(STRANDS[0]);

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

void displayOff(strand_t * pStrand)
{
  for (int i = 0; i < pStrand->numPixels; i++) {
    pStrand->pixels[i] = makeRGBVal(0, 0, 0);
  }
  ws2812_setColors(pStrand);
}

void scanner_for_two(strand_t * pStrand1, strand_t * pStrand2, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.println("DEMO: scanner_for_two()");
  bool RUN_FOREVER = (timeout_ms == 0 ? true : false);
  int currIdx1 = 0;
  int currIdx2 = 0;
  int prevIdx1 = 0;
  int prevIdx2 = 0;
  unsigned long start_ms = millis();
  rgbVal zeroColor = makeRGBVal(0, 0, 0);
  while (RUN_FOREVER || (millis() - start_ms < timeout_ms)) {
    rgbVal newColor1 = makeRGBVal(0, 0, pStrand1->brightLimit);
    rgbVal newColor2 = makeRGBVal(pStrand2->brightLimit, 0 ,0);
    pStrand1->pixels[prevIdx1] = zeroColor;
    pStrand2->pixels[prevIdx2] = zeroColor;
    pStrand1->pixels[currIdx1] = newColor1;
    pStrand2->pixels[currIdx2] = newColor2;
    ws2812_setColors(pStrand1);
    ws2812_setColors(pStrand2);
    prevIdx1 = currIdx1;
    prevIdx2 = currIdx2;
    currIdx1 = (currIdx1 + 1) % pStrand1->numPixels;
    currIdx2 = (currIdx2 + 1) % pStrand2->numPixels;
    delay(delay_ms);
  }
  displayOff(pStrand1);
  displayOff(pStrand2);
}

void scanner(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.println("DEMO: scanner()");
  int currIdx = 0;
  int prevIdx = 0;
  bool RUN_FOREVER = (timeout_ms == 0 ? true : false);
  unsigned long start_ms = millis();
  while (RUN_FOREVER || (millis() - start_ms < timeout_ms)) {
    pStrand->pixels[prevIdx] = makeRGBVal(0, 0, 0);
    pStrand->pixels[currIdx] = makeRGBVal(pStrand->brightLimit, pStrand->brightLimit, pStrand->brightLimit);;
    ws2812_setColors(pStrand);
    prevIdx = currIdx;
    currIdx++;
    if (currIdx >= pStrand->numPixels) {
      currIdx = 0;
    }
    delay(delay_ms);
  }
}

class Rainbower {
    strand_t * pStrand;
    uint8_t color_div = 4;
    uint8_t anim_step = 1;
    uint8_t anim_max;
    rgbVal color1;
    rgbVal color2;
    uint8_t stepVal1, stepVal2;
  public:
    Rainbower(strand_t *);
    void drawNext();
};

Rainbower::Rainbower(strand_t * pStrandIn)
{
  Serial.println("init: Rainbower::Rainbower()");
  pStrand = pStrandIn;
  anim_max = pStrand->brightLimit - anim_step;
  color1 = makeRGBVal(anim_max, 0, 0);
  color2 = makeRGBVal(anim_max, 0, 0);
}

void Rainbower::drawNext()
{
  color1 = color2;
  stepVal1 = stepVal2;
  for (uint16_t i = 0; i < pStrand->numPixels; i++) {
    pStrand->pixels[i] = makeRGBVal(color1.r/color_div, color1.g/color_div, color1.b/color_div);
    if (i == 1) {
      color2 = color1;
      stepVal2 = stepVal1;
    }
    switch (stepVal1) {
      case 0:
      color1.g += anim_step;
      if (color1.g >= anim_max)
        stepVal1++;
      break;
      case 1:
      color1.r -= anim_step;
      if (color1.r == 0)
        stepVal1++;
      break;
      case 2:
      color1.b += anim_step;
      if (color1.b >= anim_max)
        stepVal1++;
      break;
      case 3:
      color1.g -= anim_step;
      if (color1.g == 0)
        stepVal1++;
      break;
      case 4:
      color1.r += anim_step;
      if (color1.r >= anim_max)
        stepVal1++;
      break;
      case 5:
      color1.b -= anim_step;
      if (color1.b == 0)
        stepVal1 = 0;
      break;
    }
//    if (i == 0){
//      Serial.print("Color : ");
//      Serial.print((uint32_t)(pStrand->pixels), HEX);
//      Serial.print(" ");
//      Serial.print(pStrand->pixels[0].r);
//      Serial.print(", ");
//      Serial.print(pStrand->pixels[0].g);
//      Serial.print(", ");
//      Serial.print(pStrand->pixels[0].b);
//      Serial.println();
//    }
  }
  ws2812_setColors(pStrand);
}

void rainbow_for_three(strand_t * pStrand1, strand_t * pStrand2, strand_t * pStrand3, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.println("DEMO: rainbow_for_three()");
  unsigned long start_ms = millis();
  static Rainbower rbow1(pStrand1);  // fails weirdly if not static - why??
  static Rainbower rbow2(pStrand2);  // fails weirdly if not static - why??
  static Rainbower rbow3(pStrand3);  // fails weirdly if not static - why??
  while (timeout_ms == 0 || (millis() - start_ms < timeout_ms)) {
    rbow1.drawNext();
    rbow2.drawNext();
    rbow3.drawNext();
    delay(delay_ms);
  }
  displayOff(pStrand1);
  displayOff(pStrand2);
  displayOff(pStrand3);
}

void rainbow_for_two(strand_t * pStrand1, strand_t * pStrand2, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.println("DEMO: rainbow_for_two()");
  unsigned long start_ms = millis();
//  Rainbower * rbow1 = new Rainbower(pStrand1);
//  Rainbower * rbow2 = new Rainbower(pStrand2);
  static Rainbower rbow1(pStrand1);  // fails weirdly if not static - why??
  static Rainbower rbow2(pStrand2);  // fails weirdly if not static - why??
  while (timeout_ms == 0 || (millis() - start_ms < timeout_ms)) {
    //rbow1->drawNext();
    rbow1.drawNext();
    //rbow2->drawNext();
    rbow2.drawNext();
    delay(delay_ms);
  }
  //delete rbow1;
  //delete rbow2;
  displayOff(pStrand1);
  displayOff(pStrand2);
}

void rainbow(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.println("DEMO: rainbow()");
  unsigned long start_ms = millis();
  Rainbower * rbow2 = new Rainbower(pStrand);
  Rainbower * rbow = new Rainbower(pStrand);
  while (timeout_ms == 0 || (millis() - start_ms < timeout_ms)) {
    rbow->drawNext();
    delay(delay_ms);
  }
  delete rbow;
  delete rbow2;
  displayOff(pStrand);
}

void rainbow_for_two_OLD(strand_t * pStrand1, strand_t * pStrand2, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.println("DEMO: rainbow_for_two_OLD()");
  const uint8_t color_div = 4;
  const uint8_t anim_step = 1;
  const uint8_t anim_max_1 = pStrand1->brightLimit - anim_step;
  const uint8_t anim_max_2 = pStrand2->brightLimit - anim_step;
  rgbVal color1_1, color2_1 = makeRGBVal(anim_max_1, 0, 0);
  rgbVal color1_2, color2_2 = makeRGBVal(0, 0, anim_max_2);
  uint8_t stepVal1_1 = 0;
  uint8_t stepVal2_1 = 0;
  uint8_t stepVal1_2 = 0;
  uint8_t stepVal2_2 = 0;
  bool RUN_FOREVER = (timeout_ms == 0 ? true : false);
  unsigned long start_ms = millis();
  while (RUN_FOREVER || (millis() - start_ms < timeout_ms)) {
    color1_1 = color2_1;
    stepVal1_1 = stepVal2_1;
    for (uint16_t i = 0; i < pStrand1->numPixels; i++) {
      pStrand1->pixels[i] = makeRGBVal(color1_1.r/color_div, color1_1.g/color_div, color1_1.b/color_div);
      if (i == 1) {
        color2_1 = color1_1;
        stepVal2_1 = stepVal1_1;
      }
      switch (stepVal1_1) {
        case 0:
        color1_1.g += anim_step;
        if (color1_1.g >= anim_max_1)
          stepVal1_1++;
        break;
        case 1:
        color1_1.r -= anim_step;
        if (color1_1.r == 0)
          stepVal1_1++;
        break;
        case 2:
        color1_1.b += anim_step;
        if (color1_1.b >= anim_max_1)
          stepVal1_1++;
        break;
        case 3:
        color1_1.g -= anim_step;
        if (color1_1.g == 0)
          stepVal1_1++;
        break;
        case 4:
        color1_1.r += anim_step;
        if (color1_1.r >= anim_max_1)
          stepVal1_1++;
        break;
        case 5:
        color1_1.b -= anim_step;
        if (color1_1.b == 0)
          stepVal1_1 = 0;
        break;
      }
    }
    color1_2 = color2_2;
    stepVal1_2 = stepVal2_2;
    for (uint16_t i = 0; i < pStrand2->numPixels; i++) {
      pStrand2->pixels[i] = makeRGBVal(color1_2.r/color_div, color1_2.g/color_div, color1_2.b/color_div);
      if (i == 1) {
        color2_2 = color1_2;
        stepVal2_2 = stepVal1_2;
      }
      switch (stepVal1_2) {
        case 0:
        color1_2.g += anim_step;
        if (color1_2.g >= anim_max_2)
          stepVal1_2++;
        break;
        case 1:
        color1_2.r -= anim_step;
        if (color1_2.r == 0)
          stepVal1_2++;
        break;
        case 2:
        color1_2.b += anim_step;
        if (color1_2.b >= anim_max_2)
          stepVal1_2++;
        break;
        case 3:
        color1_2.g -= anim_step;
        if (color1_2.g == 0)
          stepVal1_2++;
        break;
        case 4:
        color1_2.r += anim_step;
        if (color1_2.r >= anim_max_2)
          stepVal1_2++;
        break;
        case 5:
        color1_2.b -= anim_step;
        if (color1_2.b == 0)
          stepVal1_2 = 0;
        break;
      }
    }
    ws2812_setColors(pStrand1);
    ws2812_setColors(pStrand2);
    delay(delay_ms);
  }
  displayOff(pStrand1);
  displayOff(pStrand2);
}

void rainbow_OLD(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.println("DEMO: rainbow_OLD()");
  const uint8_t color_div = 4;
  const uint8_t anim_step = 1;
  const uint8_t anim_max = pStrand->brightLimit - anim_step;
  rgbVal color1 = makeRGBVal(anim_max, 0, 0);
  rgbVal color2 = makeRGBVal(anim_max, 0, 0);
  uint8_t stepVal1 = 0;
  uint8_t stepVal2 = 0;
  bool RUN_FOREVER = (timeout_ms == 0 ? true : false);
  unsigned long start_ms = millis();
  while (RUN_FOREVER || (millis() - start_ms < timeout_ms)) {
    color1 = color2;
    stepVal1 = stepVal2;
    for (uint16_t i = 0; i < pStrand->numPixels; i++) {
      pStrand->pixels[i] = makeRGBVal(color1.r/color_div, color1.g/color_div, color1.b/color_div);
      if (i == 1) {
        color2 = color1;
        stepVal2 = stepVal1;
      }
      switch (stepVal1) {
        case 0:
        color1.g += anim_step;
        if (color1.g >= anim_max)
          stepVal1++;
        break;
        case 1:
        color1.r -= anim_step;
        if (color1.r == 0)
          stepVal1++;
        break;
        case 2:
        color1.b += anim_step;
        if (color1.b >= anim_max)
          stepVal1++;
        break;
        case 3:
        color1.g -= anim_step;
        if (color1.g == 0)
          stepVal1++;
        break;
        case 4:
        color1.r += anim_step;
        if (color1.r >= anim_max)
          stepVal1++;
        break;
        case 5:
        color1.b -= anim_step;
        if (color1.b == 0)
          stepVal1 = 0;
        break;
      }
    }
    ws2812_setColors(pStrand);
    delay(delay_ms);
  }
  displayOff(pStrand);
}

void test_loop()
{
  int test_max_passes = 30;
  for (int test_passes = 0; test_passes < test_max_passes; test_passes++) {
    for (int i = 0; i < STRANDCNT; i++) {
      strand_t * pStrand = &STRANDS[i];
      for(uint16_t i = 0; i < pStrand->numPixels; i++) {
        pStrand->pixels[i] = makeRGBVal(1, 1, 1);
      }
      pStrand->pixels[0] = makeRGBVal(2, 1, 3);
      pStrand->pixels[1] = makeRGBVal(5, 4, 6);
      pStrand->pixels[2] = makeRGBVal(8, 7, 9);
      ws2812_setColors(pStrand);
    }
    #if DEBUG_WS2812_DRIVER
      dumpDebugBuffer(test_passes, ws2812_debugBuffer);
    #endif
    delay(1);
  }
  while(1) {}
}


void setup()
{
  // TODO: this is to avoid crosstalk during testing
  gpioSetup(16, OUTPUT, LOW);
  gpioSetup(17, OUTPUT, LOW);
  gpioSetup(18, OUTPUT, LOW);
  gpioSetup(19, OUTPUT, LOW);
  Serial.begin(115200);
  Serial.println("Initializing...");
  if (ws2812_init(STRANDS, STRANDCNT)) {
    Serial.println("Init FAILURE: halting");
    while (true) {};
  }
  for (int i = 0; i < STRANDCNT; i++) {
    strand_t * pStrand = &STRANDS[i];
    Serial.print("Strand ");
    Serial.print(i);
    Serial.print(" = ");
    Serial.print((uint32_t)(pStrand->pixels), HEX);
    Serial.println();
    #if DEBUG_WS2812_DRIVER
      dumpDebugBuffer(-2, ws2812_debugBuffer);
    #endif
    displayOff(pStrand);
    #if DEBUG_WS2812_DRIVER
      dumpDebugBuffer(-1, ws2812_debugBuffer);
    #endif
  }
  Serial.println("Init complete");
}

void loop()
{
  rainbow_for_three(&STRANDS[0], &STRANDS[1], &STRANDS[2], 0, 5000);
  //rainbow_for_two_OLD(&STRANDS[0], &STRANDS[1], 0, 5000);
  //rainbow_for_two(&STRANDS[0], &STRANDS[1], 0, 5000);
  scanner_for_two(&STRANDS[0], &STRANDS[1], 0, 2000);
  rainbow(&STRANDS[0], 0, 2000);
  rainbow(&STRANDS[1], 0, 2000);
  rainbow_OLD(&STRANDS[0], 0, 2000);
  rainbow_OLD(&STRANDS[1], 0, 2000);
  #if DEBUG_WS2812_DRIVER
    dumpDebugBuffer(0, ws2812_debugBuffer);
  #endif
  for (int i = 0; i < STRANDCNT; i++) {
    strand_t * pStrand = &STRANDS[i];
    rainbow(pStrand, 0, 2000);
    scanner(pStrand, 0, 2000);
    displayOff(pStrand);
    #if DEBUG_WS2812_DRIVER
      dumpDebugBuffer(test_passes, ws2812_debugBuffer);
    #endif
  }
}

