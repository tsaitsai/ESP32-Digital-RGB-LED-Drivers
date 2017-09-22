# ESP32 Digital RGB LED Drivers

Digital RGB LED (WS2812/SK6812/NeoPixel/WS2813/etc.) drivers for the ESP32

Based upon the [ESP32 WS2812 driver work by Chris Osborn](https://github.com/FozzTexx/ws2812-demo)

<hr>

### Notes

This currently works well with WS2812/NeoPixel LEDs - SK6812 LEDs should work equally well. This should also work fine with WS2813 (no hardware to test this yet).

The RMT peripheral of the ESP32 is used for controlling up to 8 LED "strands" (in whatever form factor the serially-chained LEDs are placed). These strands are independently controlled and buffered. So far I've tested 4 strands successfully.

There are working demos for Espressif's IoT Development Framework (esp-idf) and Arduino-ESP32 core

<hr>

### ESP-IDF build notes - Important!

There are ESP-IDF SDK settings that need to be changed to equal the Arduino-ESP32 defaults. The Tick Rate and CPU Frequency need to be adjusted, otherwise the ESP-IDF build will run significantly more slowly.

Please see the `sdkconfig.defaults` file for details. If you run `make menuconfig` or `make sdkconfig` this file will be parsed for initial settings ONLY if `sdkconfig` doesn't exist. However, this file will be processed every time you run `make defconfig`.

<hr>

### TODO

  - IN PROGRESS: Need RGBW device and datasheets to test preliminary RGBW handling!
  - Small main.c demo - is it even possible?
  - Arduino-ESP32 and classes - very weird if not static or instantiated with `new`??
  - Add more interleaved demos, and more demos in general
  - Mirror changes to the ESP-IDF side
  - Fix TODOs in code
  - Make Arduino side a true Arduino library?
  - APA102/DotStar support?
