# ESP32 Digital RGB LED Drivers

Digital RGB LED (WS2812/SK6812/NeoPixel/WS2813/etc.) drivers for the ESP32

Based upon the [ESP32 WS2812 driver work by Chris Osborn](https://github.com/FozzTexx/ws2812-demo)

<hr>

### Notes

This currently works well with WS2812/NeoPixel LEDs - SK6812 LEDs should work equally well. This should also work fine with WS2813 (no hardware to test this yet).

The RMT peripheral of the ESP32 is used for controlling up to 8 LED "strands" (in whatever form factor the serially-chained LEDs are placed). These strands are independently controlled and buffered. So far I've tested 4 strands successfully.

There are working demos for Espressif's IoT Development Framework (esp-idf) and Arduino-ESP32 core

<hr>

### TODO

  - Arduino-ESP32 and classes - very weird if not static or inst'd with `new`??
  - Add strand num to each strand for introspection
  - print strand num from pStrand for each demo
  - Add more interleaved demos
  - Mirror changes to the ESP-IDF side

  - More demos!
  - Make Arduino side a true Arduino library?
  - APA102/DotStar support?
