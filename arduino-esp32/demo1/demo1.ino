/* 
 * Demo code for driving digital RGB(W) LEDs using the ESP32's RMT peripheral
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

#include "esp32_digital_led_lib.h"

#if defined(ARDUINO) && ARDUINO >= 100
  // No extras
#elif defined(ARDUINO) // pre-1.0
  // No extras
#elif defined(ESP_PLATFORM)
  #include "arduinoish.hpp"
#endif

// **Required** if debugging is enabled in library header
// TODO: remove requirement?
#if DEBUG_ESP32_DIGITAL_LED_LIB
  int digitalLeds_debugBufferSz = 1024;
  char * digitalLeds_debugBuffer = static_cast<char*>(calloc(digitalLeds_debugBufferSz, sizeof(char)));
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
//  {.rmtChannel = 0, .gpioNum = 16, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels = 256,
//   .pixels = nullptr, ._stateVars = nullptr},
  {.rmtChannel = 1, .gpioNum = 17, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels =  93,
   .pixels = nullptr, ._stateVars = nullptr},
  {.rmtChannel = 2, .gpioNum = 18, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels =  93,
   .pixels = nullptr, ._stateVars = nullptr},
  {.rmtChannel = 3, .gpioNum = 19, .ledType = LED_WS2812B, .brightLimit = 32, .numPixels =  64,
   .pixels = nullptr, ._stateVars = nullptr},
  {.rmtChannel = 0, .gpioNum = 16, .ledType = LED_SK6812W, .brightLimit = 32, .numPixels = 300,
   .pixels = nullptr, ._stateVars = nullptr},
};
int STRANDCNT = sizeof(STRANDS)/sizeof(STRANDS[0]);

// Forward declarations
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

void scanner_for_two(strand_t * pStrand1, strand_t * pStrand2, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.print("DEMO: scanner_for_two(");
  Serial.print(pStrand1->rmtChannel);
  Serial.print(", ");
  Serial.print(pStrand2->rmtChannel);
  Serial.println(")");
  bool runForever = (timeout_ms == 0 ? true : false);
  int currIdx1 = 0;
  int currIdx2 = 0;
  int prevIdx1 = 0;
  int prevIdx2 = 0;
  unsigned long start_ms = millis();
  pixelColor_t offColor = pixelFromRGB(0, 0, 0);
  while (runForever || (millis() - start_ms < timeout_ms)) {
    pixelColor_t newColor1 = pixelFromRGB(0, 0, pStrand1->brightLimit);
    pixelColor_t newColor2 = pixelFromRGB(pStrand2->brightLimit, 0 ,0);
    pStrand1->pixels[prevIdx1] = offColor;
    pStrand2->pixels[prevIdx2] = offColor;
    pStrand1->pixels[currIdx1] = newColor1;
    pStrand2->pixels[currIdx2] = newColor2;
    digitalLeds_update(pStrand1);
    digitalLeds_update(pStrand2);
    prevIdx1 = currIdx1;
    prevIdx2 = currIdx2;
    currIdx1 = (currIdx1 + 1) % pStrand1->numPixels;
    currIdx2 = (currIdx2 + 1) % pStrand2->numPixels;
    delay(delay_ms);
  }
  digitalLeds_reset(pStrand1);
  digitalLeds_reset(pStrand2);
}

void scanner(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.print("DEMO: scanner(");
  Serial.print(pStrand->rmtChannel);
  Serial.println(")");
  int currIdx = 0;
  int prevIdx = 0;
  bool runForever = (timeout_ms == 0 ? true : false);
  unsigned long start_ms = millis();
  while (runForever || !(millis() - start_ms >= timeout_ms && currIdx == 0)) {
    pStrand->pixels[prevIdx] = pixelFromRGBW(0, 0, 0, 0);
    pStrand->pixels[currIdx] = pixelFromRGBW(pStrand->brightLimit, pStrand->brightLimit, pStrand->brightLimit, pStrand->brightLimit);
    // Serial.println(currIdx);
    digitalLeds_update(pStrand);
    prevIdx = currIdx;
    currIdx++;
    if (currIdx >= pStrand->numPixels) {
      currIdx = 0;
    }
    delay(delay_ms);
  }
  digitalLeds_reset(pStrand);
}

class Rainbower {
    strand_t * pStrand;
    const uint8_t color_div = 4;
    const uint8_t anim_step = 1;
    uint8_t anim_max;
    uint8_t stepVal1;
    uint8_t stepVal2;
    pixelColor_t color1;
    pixelColor_t color2;
  public:
    Rainbower(strand_t *);
    void drawNext();
};

Rainbower::Rainbower(strand_t * pStrandIn)
{
  pStrand = pStrandIn;
  anim_max = pStrand->brightLimit - anim_step;
  stepVal1 = 0;
  stepVal2 = 0;
  color1 = pixelFromRGB(anim_max, 0, 0);
  color2 = pixelFromRGB(anim_max, 0, 0);
}

void Rainbower::drawNext()
{
  color1 = color2;
  stepVal1 = stepVal2;
  for (uint16_t i = 0; i < pStrand->numPixels; i++) {
    pStrand->pixels[i] = pixelFromRGB(color1.r/color_div, color1.g/color_div, color1.b/color_div);
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
    if (i == 0){
    }
  }
  digitalLeds_update(pStrand);
}

void rainbow_for_three(strand_t * pStrand1, strand_t * pStrand2, strand_t * pStrand3, unsigned long delay_ms, unsigned long timeout_ms)
{
  Rainbower rbow1(pStrand1); Rainbower * pRbow1 = &rbow1;
  Rainbower rbow2(pStrand2); Rainbower * pRbow2 = &rbow2;
  Rainbower rbow3(pStrand3); Rainbower * pRbow3 = &rbow3;
  Serial.print("DEMO: rainbow_for_three(");
  Serial.print(pStrand1->rmtChannel);
  Serial.print(", ");
  Serial.print(pStrand2->rmtChannel);
  Serial.print(", ");
  Serial.print(pStrand3->rmtChannel);
  Serial.print(") : ");
  Serial.print("pRbow1 = 0x");
  Serial.print((uint32_t)pRbow1, HEX);
  Serial.print(", ");
  Serial.print("pRbow2 = 0x");
  Serial.print((uint32_t)pRbow2, HEX);
  Serial.print(", ");
  Serial.print("pRbow3 = 0x");
  Serial.print((uint32_t)pRbow3, HEX);
  Serial.println();
  unsigned long start_ms = millis();
  while (timeout_ms == 0 || (millis() - start_ms < timeout_ms)) {
    pRbow1->drawNext();
    pRbow2->drawNext();
    pRbow3->drawNext();
    delay(delay_ms);
  }
  digitalLeds_reset(pStrand1);
  digitalLeds_reset(pStrand2);
  digitalLeds_reset(pStrand3);
}

void rainbow_for_two(strand_t * pStrand1, strand_t * pStrand2, unsigned long delay_ms, unsigned long timeout_ms)
{
  Serial.print("DEMO: rainbow_for_two(");
  Serial.print(pStrand1->rmtChannel);
  Serial.print(", ");
  Serial.print(pStrand2->rmtChannel);
  Serial.println(")");
  unsigned long start_ms = millis();
  Rainbower rbow1(pStrand1); Rainbower * pRbow1 = &rbow1;
  Rainbower rbow2(pStrand2); Rainbower * pRbow2 = &rbow2;
  while (timeout_ms == 0 || (millis() - start_ms < timeout_ms)) {
    pRbow1->drawNext();
    pRbow2->drawNext();
    delay(delay_ms);
  }
  digitalLeds_reset(pStrand1);
  digitalLeds_reset(pStrand2);
}

void rainbow(strand_t * pStrand, unsigned long delay_ms, unsigned long timeout_ms)
{
  //Rainbower * pRbow = new Rainbower(pStrand); // riskier?
  Rainbower rbow(pStrand); Rainbower * pRbow = &rbow;
  Serial.print("DEMO: rainbow(");
  Serial.print(pStrand->rmtChannel);
  Serial.print("): ");
  Serial.print("pRbow addr = 0x");
  Serial.print((uint32_t)pRbow, HEX);
  Serial.println();
  unsigned long start_ms = millis();
  while (timeout_ms == 0 || (millis() - start_ms < timeout_ms)) {
    pRbow->drawNext();
    delay(delay_ms);
  }
  //delete pRbow; // risky if you forget this! Or free memory out of sequence...
  digitalLeds_reset(pStrand);
}

void test_loop()
{
  int test_max_passes = 30;
  for (int test_passes = 0; test_passes < test_max_passes; test_passes++) {
    for (int i = 0; i < STRANDCNT; i++) {
      strand_t * pStrand = &STRANDS[i];
      for(uint16_t i = 0; i < pStrand->numPixels; i++) {
        pStrand->pixels[i] = pixelFromRGB(1, 1, 1);
      }
      pStrand->pixels[0] = pixelFromRGB(2, 1, 3);
      pStrand->pixels[1] = pixelFromRGB(5, 4, 6);
      pStrand->pixels[2] = pixelFromRGB(8, 7, 9);
      digitalLeds_update(pStrand);
    }
    #if DEBUG_ESP32_DIGITAL_LED_LIB
      dumpDebugBuffer(test_passes, digitalLeds_debugBuffer);
    #endif
    delay(1);
  }
  while(1) {}
}

int getMaxMalloc(int min_mem, int max_mem) {
  int prev_size = min_mem;
  int curr_size = min_mem;
  int max_free = 0;
//  Serial.print("checkmem: testing alloc from ");
//  Serial.print(min_mem);
//  Serial.print(" : ");
//  Serial.print(max_mem);
//  Serial.println(" bytes");
  while (1) {
    void * foo1 = malloc(curr_size);
//    Serial.print("checkmem: attempt alloc of ");
//    Serial.print(curr_size);
//    Serial.print(" bytes --> pointer 0x");
//    Serial.println((uintptr_t)foo1, HEX);
    if (foo1 == nullptr) {  // Back off
      max_mem = min(curr_size, max_mem);
//      Serial.print("checkmem: backoff 2 prev = ");
//      Serial.print(prev_size);
//      Serial.print(", curr = ");
//      Serial.print(curr_size);
//      Serial.print(", max_mem = ");
//      Serial.print(max_mem);
//      Serial.println();
      curr_size = (int)(curr_size - (curr_size - prev_size) / 2.0);
//      Serial.print("checkmem: backoff 2 prev = ");
//      Serial.print(prev_size);
//      Serial.print(", curr = ");
//      Serial.print(curr_size);
//      Serial.println();
    }
    else {  // Advance
      free(foo1);
      max_free = curr_size;
      prev_size = curr_size;
      curr_size = min(curr_size * 2, max_mem);
//      Serial.print("checkmem: advance 2 prev = ");
//      Serial.print(prev_size);
//      Serial.print(", curr = ");
//      Serial.print(curr_size);
//      Serial.println();
    }
    if (abs(curr_size - prev_size) == 0) {
      break;
    }
  }
  Serial.print("checkmem: max free heap = ");
  Serial.print(esp_get_free_heap_size());
  Serial.print(" bytes, max allocable = ");
  Serial.print(max_free);
  Serial.println(" bytes");
  return max_free;
}

void dumpSysInfo() {
  esp_chip_info_t sysinfo;
  esp_chip_info(&sysinfo);
  Serial.print("Model: ");
  Serial.print((int)sysinfo.model);
  Serial.print("; Features: 0x");
  Serial.print((int)sysinfo.features, HEX);
  Serial.print("; Cores: ");
  Serial.print((int)sysinfo.cores);
  Serial.print("; Revision: r");
  Serial.println((int)sysinfo.revision);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing...");
  dumpSysInfo();
  getMaxMalloc(1*1024, 1024*1024);

  /****************************************************************************
     If you have multiple strands connected, but not all are in use, the
     GPIO power-on defaults for the unused strand data lines will typically be
     high-impedance. Unless you are pulling the data lines high or low via a
     resistor, this will lead to noise on those unused but connected channels
     and unwanted driving of those unallocated strands.
     This optional gpioSetup() code helps avoid that problem programmatically.
  ****************************************************************************/
  gpioSetup(16, OUTPUT, LOW);
  gpioSetup(17, OUTPUT, LOW);
  gpioSetup(18, OUTPUT, LOW);
  gpioSetup(19, OUTPUT, LOW);

  if (digitalLeds_init(STRANDS, STRANDCNT)) {
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
    #if DEBUG_ESP32_DIGITAL_LED_LIB
      dumpDebugBuffer(-2, digitalLeds_debugBuffer);
    #endif
    digitalLeds_reset(pStrand);
    #if DEBUG_ESP32_DIGITAL_LED_LIB
      dumpDebugBuffer(-1, digitalLeds_debugBuffer);
    #endif
  }
  Serial.println("Init complete");
}

void loop()
{
//  // mem tests
//  rainbow(&STRANDS[2], 0, 1);
//  getMaxMalloc(1*1024, 1024*1024);
//  rainbow_for_three(&STRANDS[0], &STRANDS[1], &STRANDS[2], 0, 1);
//  getMaxMalloc(1*1024, 1024*1024);
//  return;

//  // print tests
//  Serial.println(0xFFFFFFFF, DEC);
//  Serial.println(0xFFFFFFFF, HEX);
//  Serial.println(0xFFFFFFFF, BIN);
//  Serial.println(0x7FFFFFFF, DEC);
//  Serial.println(0x7FFFFFFF, HEX);
//  Serial.println(0x7FFFFFFF, BIN);
//  Serial.println(0x00000000, DEC);
//  Serial.println(0x00000000, HEX);
//  Serial.println(0x00000000, BIN);
//  Serial.println(        -1, DEC);
//  Serial.println(        -1, HEX);
//  Serial.println(        -1, BIN);

  scanner(&STRANDS[2], 0, 2000);
  scanner(&STRANDS[2], 1, 2000); // A tiny delay can smooth things out
  scanner(&STRANDS[2], 5, 2000);
  rainbow_for_three(&STRANDS[0], &STRANDS[1], &STRANDS[2], 0, 5000);
  rainbow_for_two(&STRANDS[0], &STRANDS[1], 0, 5000);
  scanner_for_two(&STRANDS[0], &STRANDS[1], 0, 2000);
  #if DEBUG_ESP32_DIGITAL_LED_LIB
    dumpDebugBuffer(0, digitalLeds_debugBuffer);
  #endif
  for (int i = 0; i < STRANDCNT; i++) {
    strand_t * pStrand = &STRANDS[i];
    rainbow(pStrand, 0, 2000);
    scanner(pStrand, 0, 2000);
    digitalLeds_reset(pStrand);
    #if DEBUG_ESP32_DIGITAL_LED_LIB
      dumpDebugBuffer(test_passes, digitalLeds_debugBuffer);
    #endif
  }
}

