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

#include <string.h>
#include "MultiWS2811.h"
#include "gamma.h"

//#define _DEBUG

MultiWS2811* MultiWS2811::multiWS2811;
uint16_t MultiWS2811::stripLen;
uint8_t MultiWS2811::ditherCycle;
void * MultiWS2811::frameBuffer;
COL_RGB * MultiWS2811::copyBuffer;
COL_RGB * MultiWS2811::drawBuffer;
uint8_t MultiWS2811::params;
DMAChannel MultiWS2811::dma1;
uint16_t MultiWS2811::currentTransferEndLed;

static volatile uint8_t update_in_progress = 0;
static volatile uint8_t update_ready = 0;
static uint32_t update_completed_at = 0;


MultiWS2811::MultiWS2811(uint32_t numPerStrip, void *frameBuf, COL_RGB *copyBuf, COL_RGB *drawBuf, uint8_t config)
{
	stripLen = numPerStrip;
	frameBuffer = frameBuf;
	copyBuffer = copyBuf;
	drawBuffer = drawBuf;
	params = config;
}

void MultiWS2811::begin(uint32_t numPerStrip, void *frameBuf, COL_RGB *copyBuf, COL_RGB *drawBuf, uint8_t config)
{
	stripLen = numPerStrip;
	frameBuffer = frameBuf;
	copyBuffer = copyBuf;
	drawBuffer = drawBuf;
	params = config;
	begin();
}


void MultiWS2811::begin(void)
{
	multiWS2811 = this;

	fastDitherMode = false;
	
	uint32_t bufsize;
	bufsize = stripLen * 384;

	ditherCycle = 0;

	// set up the buffers
	memset(frameBuffer, 0, bufsize);
	memset(drawBuffer, 0, bufsize);

	// configure the 16 output pins
	GPIOC_PCOR = 0xFF;
	pinMode(15, OUTPUT);	// strip #1
	pinMode(22, OUTPUT);	// strip #2
	pinMode(23, OUTPUT);	// strip #3
	pinMode(9, OUTPUT);	// strip #4
	pinMode(10, OUTPUT);	// strip #5
	pinMode(13, OUTPUT);	// strip #6
	pinMode(11, OUTPUT);	// strip #7
	pinMode(12, OUTPUT);	// strip #8

	GPIOD_PCOR = 0xFF;
	pinMode(2, OUTPUT);	// strip #9
	pinMode(14, OUTPUT);	// strip #10
	pinMode(7, OUTPUT);	// strip #11
	pinMode(8, OUTPUT);	// strip #12
	pinMode(6, OUTPUT);	// strip #13
	pinMode(20, OUTPUT);	// strip #14
	pinMode(21, OUTPUT);	// strip #15
	pinMode(5, OUTPUT);	// strip #16

#ifdef _DEBUG
	pinMode(0, OUTPUT); // testing: oscilloscope trigger
#endif


	// 6.6 MHz
	FTM1_SC = 0;   // stop the timer // 45.4.3 Status And Control (FTMx_SC) p.1144
	FTM1_CNT = 0;  // Writing any value to COUNT updates the counter with its initial value, CNTIN.  // 45.4.4 Counter (FTMx_CNT) p.1145
	FTM1_MOD = 7;  // count to this value // 45.4.5 Modulo (FTMx_MOD) p.1146 // After the FTM counter reaches the modulo value, the overflow flag (TOF) becomes set at the next clock.
	
	// DMA trigger
	FTM1_C0SC = FTM_CSC_MSB | FTM_CSC_ELSB | FTM_CSC_DMA | FTM_CSC_CHIE;   // 45.4.6 Channel (n) Status And Control (FTMx_CnSC) p.1147  // Edge-Aligned PWM - High-true pulses (clear Output on match) 
	FTM1_C0V = 1;  // switch to LOW when reaching this value // 45.4.7 Channel (n) Value (FTMx_CnV) p.1149
	//CORE_PIN3_CONFIG = PORT_PCR_MUX(3);  // 12.5.1 Pin Control Register n (PORTx_PCRn) p.220
	
	// shift reg clock
	FTM1_C1SC = FTM_CSC_MSB | FTM_CSC_ELSA;   // 45.4.6 Channel (n) Status And Control (FTMx_CnSC) p.1147  // Edge-Aligned PWM - Low-true pulses (set Output on match) 
	FTM1_C1V = FTM1_MOD-1;  // switch to HIGH when reaching this value // 45.4.7 Channel (n) Value (FTMx_CnV) p.1149
	CORE_PIN4_CONFIG = PORT_PCR_MUX(3);  // 12.5.1 Pin Control Register n (PORTx_PCRn) p.220



	// 0.833 MHz
	FTM2_SC = 0;
	FTM2_CNT = 0;
	FTM2_MOD = (FTM1_MOD + 1) * 8 - 1;
	
	// Latch & Mux Switch
	FTM2_C0SC = FTM_CSC_MSB | FTM_CSC_ELSB; // Edge-Aligned PWM - High-true pulses (clear Output on match) 
	FTM2_C0V = (FTM2_MOD+1)*0.5;
	CORE_PIN29_CONFIG = PORT_PCR_MUX(3);


	// 0.833 MHz 
	FTM3_SC = 0;
	FTM3_CNT = 0;
	FTM3_MOD = (FTM1_MOD + 1) * 8 - 1;
	
	// Up/Down
	FTM3_C4SC = FTM_CSC_MSB | FTM_CSC_ELSA;  // Edge-Aligned PWM - Low-true pulses (set Output on match) 
	FTM3_C4V = (FTM3_MOD+1)*0.5;
	CORE_PIN35_CONFIG = PORT_PCR_MUX(3);



	// Use DMA to write pixel data to port C and port D 
	// from: https://github.com/brainsmoke/HexWS2811

	dma1.TCD->SADDR = frameBuffer;
	dma1.TCD->SOFF = 2;
	dma1.TCD->ATTR_SRC = DMA_TCD_ATTR_SIZE_16BIT;
	dma1.TCD->SLAST = -bufsize;

	// Send data to both PORT C and D in the same minor loop (executed after the same trigger)

	#define PORT_DELTA ( (uint32_t)&GPIOD_PDOR - (uint32_t)&GPIOC_PDOR )
	dma1.TCD->DADDR = &GPIOC_PDOR;
	dma1.TCD->DOFF = PORT_DELTA;
	// loop GPIOC_PDOR, GPIOD_PDOR and back
	dma1.TCD->ATTR_DST = ((31 - __builtin_clz(PORT_DELTA * 2)) << 3) | DMA_TCD_ATTR_SIZE_8BIT;
	dma1.TCD->DLASTSGA = 0;

	dma1.TCD->NBYTES = 2;
	dma1.TCD->BITER = bufsize / 2;
	dma1.TCD->CITER = bufsize / 2;


	dma1.disableOnCompletion();
	dma1.interruptAtCompletion();
	dma1.triggerAtHardwareEvent(DMAMUX_SOURCE_FTM1_CH0);
	dma1.attachInterrupt(isr);
}

int MultiWS2811::busy(void)
{
	if (update_in_progress) return 1;
	// busy for 50 (or 300 for ws2813) us after the done interrupt, for WS2811 reset
	if (micros() - update_completed_at < 300) return 1;
	return 0;
}


void MultiWS2811::isr(void)
{
	pinMode(35, OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(29, OUTPUT);

	FTM1_SC = 0;
	FTM2_SC = 0;
	FTM3_SC = 0;

	FTM1_CNT = 0;
	FTM2_CNT = 0;
	FTM3_CNT = 0;

	dma1.clearInterrupt();
	
	if (currentTransferEndLed < stripLen) {
		transfer(currentTransferEndLed);
	}
	else {
		update_completed_at = micros();
		update_in_progress = 0;

		if (fastDitherMode && !update_ready) {
			fillFrameBuffer();
			update_in_progress = 1;
			transfer(0);
		}
	}
}

void MultiWS2811::show(void)
{
	update_ready = 1;
	memcpy(copyBuffer, drawBuffer, stripLen * 384);
	while (update_in_progress);
	update_ready = 0;

	fillFrameBuffer();
	update_in_progress = 1;
	transfer(0);
}

void MultiWS2811::transfer(uint16_t fromLed)
{
	if(fromLed == 0)
		while (micros() - update_completed_at < 300);

	// determine the amount of leds to transfer
	uint16_t remainingLEDs = stripLen - fromLed;
	uint16_t transferLen = remainingLEDs;
	if (remainingLEDs > MAX_TRANSFER_LEN) {
		transferLen = MAX_TRANSFER_LEN;
	}
	currentTransferEndLed = fromLed + transferLen;

	uint32_t bufsize;
	bufsize = transferLen * 384;
	
	// set the DMA channel to start and stop at the correct memory locations
	dma1.TCD->SADDR = static_cast<char*>(frameBuffer) + 384 * fromLed;
	dma1.TCD->SLAST = -bufsize;
	dma1.TCD->BITER = bufsize / 2;
	dma1.TCD->CITER = bufsize / 2;

	int delayCount = FTM1_MOD * 4;

	noInterrupts();
	
	// make sure all timers are stopped
	FTM1_SC = 0;  
	FTM2_SC = 0;
	FTM3_SC = 0;
	
	// reset all timers to their correct start values, so that when they are started one after another, they align the way we want
	FTM1_CNT = 0;  // Writing any value to COUNT updates the counter with its initial value, CNTIN.

	FTM2_CNTIN = 1;
	FTM2_CNT = 0;
	FTM2_CNTIN = 0;

	FTM3_CNTIN = FTM3_MOD-6;
	FTM3_CNT = 0;
	FTM3_CNTIN = 0;


	// clear any prior timer DMA triggers
	uint32_t tmp __attribute__((unused));
	FTM1_C0SC = 0;
	tmp = FTM1_C0SC;
	FTM1_C0SC = FTM_CSC_MSB | FTM_CSC_ELSB | FTM_CSC_DMA | FTM_CSC_CHIE;


	dma1.enable();
#ifdef _DEBUG
	if (fromLed == 0) digitalWriteFast(0, HIGH); // oscilloscope trigger
#endif

	FTM3_SC = FTM2_SC = FTM1_SC = FTM_SC_CLKS(1) | FTM_SC_PS(0);  // start the timers  // FTM_SC_CLKS Clock Source Selection p.1144 // FTM_SC_PS Prescale Factor Selection p.1144

	CORE_PIN4_CONFIG = PORT_PCR_MUX(3);
	CORE_PIN35_CONFIG = PORT_PCR_MUX(3);
	for (int i = 0; i < delayCount; i++) { digitalWriteFast(1, HIGH); }    // wait till switching pin 29 to the timer to filter out the first pulse
	CORE_PIN29_CONFIG = PORT_PCR_MUX(3);

#ifdef _DEBUG
	if (fromLed == 0) digitalWriteFast(0, LOW); // oscilloscope trigger
#endif
	interrupts();
}


void MultiWS2811::fillFrameBuffer()
{
	uint32_t shiftreg, strip_shift, strip, offset, mask;
	uint16_t bit, *p;

	ditherCycle++;
	if (ditherCycle > (1 << DITHER_BITS) - 1) ditherCycle = 0;

	const uint8_t *ditheredLUT = gammaTable + (ditherCycle << 8);

	for (int num = 0; num < stripLen * 128; num++) {
		int color;

		switch (params & 7) {
		case WS2811_RBG:
			color = ditheredLUT[copyBuffer[num].r] << 16 | ditheredLUT[copyBuffer[num].g] | ditheredLUT[copyBuffer[num].b] << 8;
			break;
		case WS2811_GRB:
			color = ditheredLUT[copyBuffer[num].r] << 8 | ditheredLUT[copyBuffer[num].g] << 16 | ditheredLUT[copyBuffer[num].b];
			break;
		case WS2811_GBR:
			color = ditheredLUT[copyBuffer[num].r] | ditheredLUT[copyBuffer[num].g] << 16 | ditheredLUT[copyBuffer[num].b] << 8;
			break;
		case WS2811_BRG:
			color = ditheredLUT[copyBuffer[num].r] << 8 | ditheredLUT[copyBuffer[num].g] | ditheredLUT[copyBuffer[num].b] << 16;
			break;
		case WS2811_BGR:
			color = ditheredLUT[copyBuffer[num].r] | ditheredLUT[copyBuffer[num].g] << 8 | ditheredLUT[copyBuffer[num].b] << 16;
			break;
		default:
			color = ditheredLUT[copyBuffer[num].r] << 16 | ditheredLUT[copyBuffer[num].g] << 8 | ditheredLUT[copyBuffer[num].b];
			break;
		}
		strip = num / stripLen; // global strip idx
		shiftreg = strip / 8;
		strip_shift = 7 - strip % 8; // strip idx on shiftreg
		offset = num % stripLen;

		bit = (1 << shiftreg);
		p = ((uint16_t *)frameBuffer) + offset * 192 + strip_shift;
		for (mask = (1 << 23); mask; mask >>= 1) {
			if (color & mask) {
				*p |= bit;
			}
			else {
				*p &= ~bit;
			}
			p += 8;
		}
	}
}


int MultiWS2811::getPixel(uint32_t num)
{
	int color = *(int *)&drawBuffer[num] & 0x00FFFFFF;
	return color;
}
