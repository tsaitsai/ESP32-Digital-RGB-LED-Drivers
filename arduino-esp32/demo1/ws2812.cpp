/* 
 * A driver for digital RGB LEDs using the RMT peripheral on the ESP32
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

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ARDUINO)
  #include "esp32-hal.h"
  #include "esp_intr.h"
  #include "driver/gpio.h"
  #include "driver/rmt.h"
  #include "driver/periph_ctrl.h"
  #include "freertos/semphr.h"
  #include "soc/rmt_struct.h"
#elif defined(ESP_PLATFORM)
  #include <esp_intr.h>
  #include <driver/gpio.h>
  #include <driver/rmt.h>
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
  #include <soc/dport_reg.h>
  #include <soc/gpio_sig_map.h>
  #include <soc/rmt_struct.h>
  #include <stdio.h>
  #include <string.h>  // memset, memcpy, etc. live here!
#endif

#ifdef __cplusplus
}
#endif

#if DEBUG_WS2812_DRIVER
extern char * ws2812_debugBuffer;
extern int ws2812_debugBufferSz;
#endif

const uint16_t MAX_PULSES = 32;  // A channel has a 64 "pulse" buffer - we use half per pass
const uint16_t DIVIDER    =  4;  // 8 still seems to work, but timings become marginal
const double   RMT_DURATION_NS = 12.5;  // Minimum time of a single RMT duration based on clock ns

// LUT for mapping bits in RMT.int_<op>.ch<n>_tx_thr_event
const uint32_t tx_thr_event_offsets [] = {
  static_cast<uint32_t>(1) << (24 + 0),
  static_cast<uint32_t>(1) << (24 + 1),
  static_cast<uint32_t>(1) << (24 + 2),
  static_cast<uint32_t>(1) << (24 + 3),
  static_cast<uint32_t>(1) << (24 + 4),
  static_cast<uint32_t>(1) << (24 + 5),
  static_cast<uint32_t>(1) << (24 + 6),
  static_cast<uint32_t>(1) << (24 + 7),
};

// LUT for mapping bits in RMT.int_<op>.ch<n>_tx_end
const uint32_t tx_end_offsets [] = {
  static_cast<uint32_t>(1) << (0 + 0) * 3,
  static_cast<uint32_t>(1) << (0 + 1) * 3,
  static_cast<uint32_t>(1) << (0 + 2) * 3,
  static_cast<uint32_t>(1) << (0 + 3) * 3,
  static_cast<uint32_t>(1) << (0 + 4) * 3,
  static_cast<uint32_t>(1) << (0 + 5) * 3,
  static_cast<uint32_t>(1) << (0 + 6) * 3,
  static_cast<uint32_t>(1) << (0 + 7) * 3,
};

typedef union {
  struct {
    uint32_t duration0:15;
    uint32_t level0:1;
    uint32_t duration1:15;
    uint32_t level1:1;
  };
  uint32_t val;
} rmtPulsePair;

typedef struct {
  uint8_t * buf_data;
  uint16_t buf_pos, buf_len, buf_half, buf_isDirty;
  xSemaphoreHandle sem;
  rmtPulsePair pulsePairMap[2];
} ws2812_stateData;

static strand_t * localStrands;
static int localStrandCnt = 0;

static intr_handle_t rmt_intr_handle = nullptr;

// Forward declarations of local functions
void copyToRmtBlock_half(strand_t * pStrand);
void handleInterrupt(void *arg);

int ws2812_init(strand_t strands [], int numStrands)
{
  #if DEBUG_WS2812_DRIVER
    snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%sws2812_init numStrands = %d\n", ws2812_debugBuffer, numStrands);
  #endif

  localStrands = strands;
  localStrandCnt = numStrands;
  if (localStrandCnt < 1 || localStrandCnt > 8) {
    return -1;
  }

  DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
  DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

  RMT.apb_conf.fifo_mask = 1;  // Enable memory access, instead of FIFO mode
  RMT.apb_conf.mem_tx_wrap_en = 1;  // Wrap around when hitting end of buffer

  for (int i = 0; i < localStrandCnt; i++) {
    strand_t * pStrand = &localStrands[i];
    ledParams_t ledParams = ledParamsAll[pStrand->ledType];
// TODO: find a better way to walk / index ledParamsAll - use a state var?

    pStrand->pixels = static_cast<pixelColor_t*>(malloc(pStrand->numPixels * sizeof(pixelColor_t)));
    if (pStrand->pixels == nullptr) {
      return -1;
    }

    pStrand->_stateVars = static_cast<ws2812_stateData*>(malloc(sizeof(ws2812_stateData)));
    if (pStrand->_stateVars == nullptr) {
      return -1;
    }
    ws2812_stateData * pState = static_cast<ws2812_stateData*>(pStrand->_stateVars);

    pState->buf_len = (pStrand->numPixels * ledParams.bytesPerPixel);
    pState->buf_data = static_cast<uint8_t*>(malloc(pState->buf_len));
    if (pState->buf_data == nullptr) {
      return -1;
    }

    rmt_set_pin(
      static_cast<rmt_channel_t>(pStrand->rmtChannel),
      RMT_MODE_TX,
      static_cast<gpio_num_t>(pStrand->gpioNum));
  
    RMT.conf_ch[pStrand->rmtChannel].conf0.div_cnt = DIVIDER;
    RMT.conf_ch[pStrand->rmtChannel].conf0.mem_size = 1;
    RMT.conf_ch[pStrand->rmtChannel].conf0.carrier_en = 0;
    RMT.conf_ch[pStrand->rmtChannel].conf0.carrier_out_lv = 1;
    RMT.conf_ch[pStrand->rmtChannel].conf0.mem_pd = 0;
  
    RMT.conf_ch[pStrand->rmtChannel].conf1.rx_en = 0;
    RMT.conf_ch[pStrand->rmtChannel].conf1.mem_owner = 0;
    RMT.conf_ch[pStrand->rmtChannel].conf1.tx_conti_mode = 0;  //loop back mode
    RMT.conf_ch[pStrand->rmtChannel].conf1.ref_always_on = 1;  // use apb clock: 80M
    RMT.conf_ch[pStrand->rmtChannel].conf1.idle_out_en = 1;
    RMT.conf_ch[pStrand->rmtChannel].conf1.idle_out_lv = 0;
  
    RMT.tx_lim_ch[pStrand->rmtChannel].limit = MAX_PULSES;
  
    // RMT config for transmitting a '0' bit val to this LED strand
    pState->pulsePairMap[0].level0 = 1;
    pState->pulsePairMap[0].level1 = 0;
    pState->pulsePairMap[0].duration0 = ledParams.T0H / (RMT_DURATION_NS * DIVIDER);
    pState->pulsePairMap[0].duration1 = ledParams.T0L / (RMT_DURATION_NS * DIVIDER);
    
    // RMT config for transmitting a '0' bit val to this LED strand
    pState->pulsePairMap[1].level0 = 1;
    pState->pulsePairMap[1].level1 = 0;
    pState->pulsePairMap[1].duration0 = ledParams.T1H / (RMT_DURATION_NS * DIVIDER);
    pState->pulsePairMap[1].duration1 = ledParams.T1L / (RMT_DURATION_NS * DIVIDER);

    RMT.int_ena.val |= tx_thr_event_offsets[pStrand->rmtChannel];  // RMT.int_ena.ch<n>_tx_thr_event = 1;
    RMT.int_ena.val |= tx_end_offsets[pStrand->rmtChannel];  // RMT.int_ena.ch<n>_tx_end = 1;
  }
  
  esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, handleInterrupt, nullptr, &rmt_intr_handle);

  for (int i = 0; i < localStrandCnt; i++) {
    strand_t * pStrand = &localStrands[i];
    ws2812_resetPixels(pStrand);
  }

  return 0;
}

void ws2812_resetPixels(strand_t * pStrand)
{
//  pixelColor_t offColor = pixelFromRGBW(0, 0, 0, 0);
//  for (int i = 0; i < pStrand->numPixels; i++) {
//    pStrand->pixels[i] = offColor;
//  }
  memset(pStrand->pixels, 0, pStrand->numPixels * sizeof(pixelColor_t));
  ws2812_setColors(pStrand);
}

int ws2812_setColors(strand_t * pStrand)
{
  ws2812_stateData * pState = static_cast<ws2812_stateData*>(pStrand->_stateVars);
  ledParams_t ledParams = ledParamsAll[pStrand->ledType];
// TODO: find a better way to walk / index ledParamsAll - use a state var?

  // Pack pixels into transmission buffer
  if (ledParams.bytesPerPixel == 3) {
    for (uint16_t i = 0; i < pStrand->numPixels; i++) {
      // Color order is translated from RGB to GRB
      pState->buf_data[0 + i * 3] = pStrand->pixels[i].g;
      pState->buf_data[1 + i * 3] = pStrand->pixels[i].r;
      pState->buf_data[2 + i * 3] = pStrand->pixels[i].b;
    }
  }
  else if (ledParams.bytesPerPixel == 4) {
    for (uint16_t i = 0; i < pStrand->numPixels; i++) {
      // Color order is translated from RGBW to GRBW
      pState->buf_data[0 + i * 4] = pStrand->pixels[i].g;
      pState->buf_data[1 + i * 4] = pStrand->pixels[i].r;
      pState->buf_data[2 + i * 4] = pStrand->pixels[i].b;
      pState->buf_data[3 + i * 4] = pStrand->pixels[i].w;
    }    
  }
  else {
    return -1;
  }

  pState->buf_pos = 0;
  pState->buf_half = 0;

  copyToRmtBlock_half(pStrand);

  if (pState->buf_pos < pState->buf_len) {
    // Fill the other half of the buffer block
    #if DEBUG_WS2812_DRIVER
      snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%s# ", ws2812_debugBuffer);
    #endif
    copyToRmtBlock_half(pStrand);
  }

  pState->sem = xSemaphoreCreateBinary();

  RMT.conf_ch[pStrand->rmtChannel].conf1.mem_rd_rst = 1;
  RMT.conf_ch[pStrand->rmtChannel].conf1.tx_start = 1;

  xSemaphoreTake(pState->sem, portMAX_DELAY);
  vSemaphoreDelete(pState->sem);
  pState->sem = nullptr;

  return 0;
}

void copyToRmtBlock_half(strand_t * pStrand)
{
  // This fills half an RMT block
  // When wraparound is happening, we want to keep the inactive half of the RMT block filled

  ws2812_stateData * pState = static_cast<ws2812_stateData*>(pStrand->_stateVars);
  ledParams_t ledParams = ledParamsAll[pStrand->ledType];
// TODO: find a better way to walk / index ledParamsAll - use a state var?

  uint16_t i, j, offset, len, byteval;

  offset = pState->buf_half * MAX_PULSES;
  pState->buf_half = !pState->buf_half;

  len = pState->buf_len - pState->buf_pos;
  if (len > (MAX_PULSES / 8))
    len = (MAX_PULSES / 8);

  if (!len) {
    if (!pState->buf_isDirty) {
      return;
    }
    // Clear the channel's data block and return
    for (i = 0; i < MAX_PULSES; i++) {
      RMTMEM.chan[pStrand->rmtChannel].data32[i + offset].val = 0;
    }
    pState->buf_isDirty = 0;
    return;
  }
  pState->buf_isDirty = 1;

  for (i = 0; i < len; i++) {
    byteval = pState->buf_data[i + pState->buf_pos];

    #if DEBUG_WS2812_DRIVER
      snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%s%d(", ws2812_debugBuffer, byteval);
    #endif

    // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to
    // the rmtPulsePair value corresponding to the buffered bit value
    for (j = 0; j < 8; j++, byteval <<= 1) {
      int bitval = (byteval >> 7) & 0x01;
      int data32_idx = i * 8 + offset + j;
      RMTMEM.chan[pStrand->rmtChannel].data32[data32_idx].val = pState->pulsePairMap[bitval].val;
      #if DEBUG_WS2812_DRIVER
        snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%s%d", ws2812_debugBuffer, bitval);
      #endif
    }
    #if DEBUG_WS2812_DRIVER
      snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%s) ", ws2812_debugBuffer);
    #endif

    // Handle the reset bit by stretching duration1 for the final bit in the stream
    if (i + pState->buf_pos == pState->buf_len - 1) {
      RMTMEM.chan[pStrand->rmtChannel].data32[i * 8 + offset + 7].duration1 =
        ledParams.TRS / (RMT_DURATION_NS * DIVIDER);
      #if DEBUG_WS2812_DRIVER
        snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%sRESET ", ws2812_debugBuffer);
      #endif
    }
  }

  // Clear the remainder of the channel's data not set above
  for (i *= 8; i < MAX_PULSES; i++) {
    RMTMEM.chan[pStrand->rmtChannel].data32[i + offset].val = 0;
  }
  
  pState->buf_pos += len;

  #if DEBUG_WS2812_DRIVER
    snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%s ", ws2812_debugBuffer);
  #endif

  return;
}

void handleInterrupt(void *arg)
{
  portBASE_TYPE taskAwoken = 42;  // TODO: does this value actually matter?

  #if DEBUG_WS2812_DRIVER
    snprintf(ws2812_debugBuffer, ws2812_debugBufferSz, "%sRMT.int_st.val = %08x\n", ws2812_debugBuffer, RMT.int_st.val);
  #endif

  for (int i = 0; i < localStrandCnt; i++) {
    strand_t * pStrand = &localStrands[i];
    ws2812_stateData * pState = static_cast<ws2812_stateData*>(pStrand->_stateVars);

    if (RMT.int_st.val & tx_thr_event_offsets[pStrand->rmtChannel])
    {  // tests RMT.int_st.ch<n>_tx_thr_event
      copyToRmtBlock_half(pStrand);
      RMT.int_clr.val |= tx_thr_event_offsets[pStrand->rmtChannel];  // set RMT.int_clr.ch<n>_tx_thr_event
    }
    else if (RMT.int_st.val & tx_end_offsets[pStrand->rmtChannel] && pState->sem)
    {  // tests RMT.int_st.ch<n>_tx_end and semaphore
      xSemaphoreGiveFromISR(pState->sem, &taskAwoken);
      RMT.int_clr.val |= tx_end_offsets[pStrand->rmtChannel];  // set RMT.int_clr.ch<n>_tx_end 
    }
  }

  return;
}

