/* 
 * Arduino-like shim code for ESP32 native builds
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

#ifndef ARDUINOISH_HPP
#define ARDUINOISH_HPP

#if defined(ARDUINO) && ARDUINO >= 100
  // No extras
#elif defined(ARDUINO) // pre-1.0
  // No extras
#elif defined(ESP_PLATFORM)
  #include <esp_system.h>
  #include <nvs_flash.h>
  #include <stdio.h>
  #include <driver/gpio.h>
  #include <driver/uart.h>
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <soc/rmt_struct.h>

  #define HIGH 1
  #define LOW 0
  #define OUTPUT GPIO_MODE_OUTPUT
  #define INPUT GPIO_MODE_INPUT

  #define DEC 10
  #define HEX 16
  #define OCT 8
  #define BIN 2

  #define min(a, b)  ((a) < (b) ? (a) : (b))
  #define max(a, b)  ((a) > (b) ? (a) : (b))
  #define floor(a)   ((int)(a))
  #define ceil(a)    ((int)((int)(a) < (a) ? (a+1) : (a)))

  uint32_t IRAM_ATTR millis()
  {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
  }

  void delay(uint32_t ms)
  {
    if (ms > 0) {
      vTaskDelay(ms / portTICK_PERIOD_MS);
    }
  }

  class SerialStub {
    public:
      inline void begin(uint32_t baud_rate)
      {
        delay(500);
        uart_set_baudrate(UART_NUM_0, baud_rate);
      }

      inline void print()
      {
        ets_printf("");
      }
      inline void println()
      {
        ets_printf("\n");
      }

      inline void print(const char * arg)
      {
        ets_printf("%s", arg);
      }
      inline void println(const char * arg)
      {
        ets_printf("%s\n", arg);
      }

      inline void print(const int arg, int argType = DEC)
      {
        switch (argType) {
          case DEC:
            ets_printf("%d", arg);
            break;
          case HEX:
            ets_printf("%x", arg);
            break;
          case OCT:
            ets_printf("%o", arg);
            break;
          case BIN:
            ets_printf("%x", arg);  // TODO: Not implemented yet
            break;
          default:
            ets_printf("%d", arg);
        }
      }
      inline void println(const int arg, int argType = DEC)
      {
        switch (argType) {
          case DEC:
            ets_printf("%d\n", arg);
            break;
          case HEX:
            ets_printf("%x\n", arg);
            break;
          case OCT:
            ets_printf("%o\n", arg);
            break;
          case BIN:
            ets_printf("%x\n", arg);  // TODO: Not implemented yet
            break;
          default:
            ets_printf("%d\n", arg);
        }
      }
  } Serial;

  void setup(void);

  void loop(void);

  void task_main(void *pvParameters)
  {
    setup();
    for (;;) {
      loop();
    }
    return;
  }

  extern "C" int app_main(void)
  {
    nvs_flash_init();
    xTaskCreate(task_main, "task_main", 4096, NULL, 10, NULL);
    return 0;
  }
#endif

#endif /* ARDUINOISH_HPP */
