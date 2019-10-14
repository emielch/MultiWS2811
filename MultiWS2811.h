/*  MultiWS2811 - High performance library to control 128 channels of WS2811 LEDs
    simultaniously through shift registers and multiplexers.
    Copyright (c) 2018 Emiel Harmsen
    Based on OctoWS2811 by Paul Stoffregen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#ifndef MultiWS2811_h
#define MultiWS2811_h

//#define _DEBUG

#ifndef __MK66FX1M0__
#error "Sorry, MultiWS2811 only works on Teensy 3.6"
#endif

#include <Arduino.h>
#include "DMAChannel.h"

#if TEENSYDUINO < 121
#error "Teensyduino version 1.21 or later is required to compile this library."
#endif

#define WS2811_RGB	0	// The WS2811 datasheet documents this way
#define WS2811_RBG	1
#define WS2811_GRB	2	// Most LED strips are wired this way
#define WS2811_GBR	3
#define WS2811_BRG	4
#define WS2811_BGR	5

#define WS2811_800kHz 0x00	// Nearly all WS2811 are 800 kHz
#define WS2811_400kHz 0x10	// Adafruit's Flora Pixels
#define WS2813_800kHz 0x20	// WS2813 are close to 800 kHz but has 300 us frame set delay

#define MAX_TRANSFER_LEN	170  // limit imposed by DMA hardware, discussed here: https://forum.pjrc.com/threads/50839-need-help-with-large-screen?p=176730&viewfull=1#post176730

struct COL_RGB {
	uint8_t b, g, r;
};

class MultiWS2811 {
public:
	MultiWS2811(uint32_t numPerStrip, void *frameBuf, COL_RGB *copyBuf, COL_RGB *drawBuf, uint8_t config = WS2811_GRB);
	void begin(void);
	void begin(uint32_t numPerStrip, void *frameBuf, COL_RGB *copyBuf, COL_RGB *drawBuf, uint8_t config = WS2811_GRB);

	void setPixel(uint32_t num, int color) {
		drawBuffer[num].b = color & 255;
		drawBuffer[num].g = color >> 8 & 255;
		drawBuffer[num].r = color >> 16 & 255;
	}
	void setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue) {
		drawBuffer[num].r = red;
		drawBuffer[num].g = green;
		drawBuffer[num].b = blue;
	}
	void setPixel(uint32_t num, COL_RGB color) {
		drawBuffer[num] = color;
	}

	int getPixel(uint32_t num);
	COL_RGB getPixelRGB(uint32_t num) { return drawBuffer[num]; }

	void show(void);
	int busy(void);

	int numPixels(void) {
		return stripLen * 8;
	}
	int color(uint8_t red, uint8_t green, uint8_t blue) {
		return (red << 16) | (green << 8) | blue;
	}


private:
	static MultiWS2811* multiWS2811;
	static uint16_t stripLen;
	static uint8_t ditherCycle;
	static void *frameBuffer;
	static COL_RGB *copyBuffer;
	static COL_RGB *drawBuffer;
	static uint8_t params;
	static DMAChannel dma1;
	static uint16_t currentTransferEndLed;

	static void isr(void);
	static void fillFrameBuffer();
	static void transfer(uint16_t fromLed);
};

#endif
