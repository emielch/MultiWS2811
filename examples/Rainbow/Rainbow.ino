/*  MultiWS2811 Rainbow.ino - Rainbow Shifting Test
    Copyright (c) 2013 Paul Stoffregen, PJRC.COM, LLC
    Adapted by Emiel Harmsen

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


  Required Connections
  --------------------
    pin 15: Shift Register #1
    pin 22: Shift Register #2
    pin 23: Shift Register #3
    pin 9:  Shift Register #4
    pin 10: Shift Register #5
    pin 13: Shift Register #6
    pin 11: Shift Register #7
    pin 12: Shift Register #8
    pin 2:  Shift Register #9
    pin 14: Shift Register #10
    pin 7:  Shift Register #11
    pin 8:  Shift Register #12
    pin 6:  Shift Register #13
    pin 20: Shift Register #14
    pin 21: Shift Register #15
    pin 5:  Shift Register #16
    
    pin 4:  Shift Register SHIFT CLOCK pin
    pin 3:  Do not use as PWM.  Normal use is ok.
    pin 29: Shift Register LATCH CLOCK pin & Multiplexer DATA SELECT pin
    pin 35: Multiplexer DATA INPUTS FROM SOURCE 0
*/

#include <MultiWS2811.h>

const int ledsPerStrip = 240;

DMAMEM int displayMemory[ledsPerStrip*96];
int drawingMemory[ledsPerStrip*96];

const int config = WS2811_GRB | WS2811_800kHz;

MultiWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);

int rainbowColors[180];


void setup() {
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);
  for (int i=0; i<180; i++) {
    int hue = i * 2;
    int saturation = 100;
    int lightness = 5;
    // pre-compute the 180 rainbow colors
    rainbowColors[i] = makeColor(hue, saturation, lightness);
  }
  digitalWrite(1, LOW);
  leds.begin();
}


void loop() {
  rainbow(10, 2500);
}


// phaseShift is the shift between each row.  phaseShift=0
// causes all rows to show the same colors moving together.
// phaseShift=180 causes each row to be the opposite colors
// as the previous.
//
// cycleTime is the number of milliseconds to shift through
// the entire 360 degrees of the color wheel:
// Red -> Orange -> Yellow -> Green -> Blue -> Violet -> Red
//
void rainbow(int phaseShift, int cycleTime)
{
  int color, x, y, wait;

  wait = cycleTime * 1000 / ledsPerStrip;
  for (color=0; color < 180; color++) {
    digitalWrite(1, HIGH);
    for (x=0; x < ledsPerStrip; x++) {
      for (y=0; y < 128; y++) {
        int index = (color + x + y*phaseShift/2) % 180;
        leds.setPixel(x + y*ledsPerStrip, rainbowColors[index]);
      }
    }
    leds.show();
    digitalWrite(1, LOW);
    delayMicroseconds(wait);
  }
}

