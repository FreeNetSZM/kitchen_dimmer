/**
   ATTiny85 PWM LED Kitchen Dimmer
   v. 1.3
   Copyright (C) 2016 Rastislav Sasik

   Smooth Led ON/OFF controlled with digital pin 1.
   Use HC-SR501 as signal source.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// ATMEL ATTINY85
//
//                      +-\/-+
//          (D 5) PB5  1|    |8  Vcc
//          (D 3) PB3  2|    |7  PB2 (D 2)
//          (D 4) PB4  3|    |6  PB1 (D 1) LED (MOSFET DRIVER)
//                GND  4|    |5  PB0 (D 0) PIR (HC-SR501)
//                      +----+
//

// PERIOD DIMMER
//
//   100% (ON)       ______________
//                  /              \ 
//   30% (dusk)    /                \_______________ 
//   0% (OFF)   __/                                 \____
//


#include "wiring_private.h"
#include <avr/sleep.h>

// pin definitions
const int led = 1; //LED
const int dig = 0; //PIR

// states
enum states {stBoot, stDark, stOn, stHold, stOff, stOff30, stHold30};
states state = stBoot;
enum wdtimer {wdt16ms = 0, wdt32ms, wdt64ms, wdt128ms, wdt250ms, wdt500ms, wdt1s, wdt2s, wdt4s, wdt8s};
volatile boolean signal_wdt = 1;

// base ON period
const unsigned long pot_value = 2000;
unsigned long dig_last;
int value = 0;

// base hold period
const unsigned long hold_value = 60 * 1000L;

// table of exponential values
// generated for values of i from 0 to 255 -> x=round( pow( 2.0, i/32.0) - 1);
const byte table[] PROGMEM = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,
  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,   5,
  5,   5,   5,   5,   5,   5,   5,   6,   6,   6,   6,   6,   6,   6,   7,   7,
  7,   7,   7,   8,   8,   8,   8,   8,   9,   9,   9,   9,   9,  10,  10,  10,
  10,  11,  11,  11,  11,  12,  12,  12,  12,  13,  13,  13,  14,  14,  14,  15,
  15,  15,  16,  16,  16,  17,  17,  18,  18,  18,  19,  19,  20,  20,  21,  21,
  22,  22,  23,  23,  24,  24,  25,  25,  26,  26,  27,  28,  28,  29,  30,  30,
  31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  39,  40,  40,  41,  42,  43,
  44,  45,  46,  47,  48,  49,  51,  52,  53,  54,  55,  56,  58,  59,  60,  62,
  63,  64,  66,  67,  69,  70,  72,  73,  75,  77,  78,  80,  82,  84,  86,  88,
  90,  91,  94,  96,  98, 100, 102, 104, 107, 109, 111, 114, 116, 119, 122, 124,
  127, 130, 133, 136, 139, 142, 145, 148, 151, 155, 158, 161, 165, 169, 172, 176,
  180, 184, 188, 192, 196, 201, 205, 210, 214, 219, 224, 229, 234, 239, 244, 250
};

///
/// Setup ports
///
void setup() {
  // disable adc
  cbi(ADCSRA, ADEN);
  setup_watchdog(wdt64ms);

  pinMode(led, OUTPUT);
  pinMode(dig, INPUT);

  for (int i = 0; i < 2; i++)
    ShowLedIsLive();

  state = stDark;
}

///
/// Test of LEDs functionality
///
void ShowLedIsLive()
{
  digitalWrite(led, HIGH);
  delay(200);
  digitalWrite(led, LOW);
  delay(200);
}

///
/// Slow increasing of intensity
///
void LightOn()
{
  unsigned long ms = millis();
  unsigned long m = 0;
  unsigned long m2 = pot_value;
  while (m < m2)
  {
    int p = digitalRead(dig);
    int v = map(m, 0, m2, value, 255);
    int vm = pgm_read_byte(&table[v]);
    analogWrite(led, vm);
    m = millis() - ms;
  }
  digitalWrite(led, HIGH);
  value = 255;
  dig_last = millis() + hold_value;

  state = stHold;
}

///
///  Wait until digital pin is OFF + hold_value
///
void LightHold() {
  while (millis() < dig_last)
  {
    int p = digitalRead(dig);
    if (p) // still ON
    {
      dig_last = millis() + hold_value;
    }
  }
  state = stOff;
}

///
/// Slow decreasing of intensity
///
void LightOff() {
  unsigned long ms = millis();
  unsigned long m = 0;
  unsigned long m2 = pot_value * 6; // 6x on period
  while (m < m2)
  {
    int p = digitalRead(dig);
    if (p)
    {
      state = stOn;
      return;
    }
    value = map(m, 0, m2, 255, 0);
    int vm = pgm_read_byte(&table[value]);
    value = vm;
    analogWrite(led, vm);
    if (value < 20) {
      state = stHold30;
      return;
    }
    m = millis() - ms;
  }
  digitalWrite(led, LOW);
  value = 0;
  state = stDark;
}

///
/// Handles darkness
///
void LightDark() {
  int p = digitalRead(dig);
  if (p)
    state = stOn;
  else
    system_sleep();
}

///
/// Handled dusk
///
void LightHold30() {
  dig_last = millis() + hold_value;
  while (millis() < dig_last)
  {
    int p = digitalRead(dig);
    if (p) // still ON
    {
      state = stOn;
      return;
    }
  }
  state = stOff30;
}

///
/// From dusk to darkness
///
void LightOff30() {
  unsigned long ms = millis();
  unsigned long m = 0;
  unsigned long m2 = pot_value;
  while (m < m2)
  {
    int p = digitalRead(dig);
    if (p)
    {
      state = stOn;
      return;
    }
    int v = map(m, 0, m2, value, 0);
    analogWrite(led, v);
    value = v;
    m = millis() - ms;
  }
  digitalWrite(led, LOW);
  value = 0;
  state = stDark;
}

///
/// Main loop
///
void loop() {
  switch (state) {
    case stDark:
      LightDark();
      break;
    case stOn:
      LightOn();
      break;
    case stHold:
      LightHold();
      break;
    case stOff:
      LightOff();
      break;
    case stHold30:
      LightHold30();
      break;
    case stOff30:
      LightOff30();
      break;
  }
}

///
/// set system into the sleep state
/// system wakes up when watchdog is timed out
///
void system_sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_enable();
  sleep_mode();                        // System sleeps here
  sleep_disable();                     // System continues execution here when watchdog timed out
}

///
/// Setup watchdog
/// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
/// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
///
void setup_watchdog(byte t) {
  byte bb;
  int ww;
  if (t > wdt8s)
    t = wdt8s;
  bb = t & 7; // WDP[2:0]
  if (t > wdt2s)
    sbi(bb, WDP3);
  sbi(bb, WDCE);
  ww = bb;

  cbi(MCUSR, WDRF);
  // start timed sequence
  WDTCR |= (1 << WDCE) | (1 << WDE);
  // set new watchdog timeout value
  WDTCR = bb;
  sbi(WDTCR, WDIE);
}

///
/// Watchdog Interrupt Service
/// is executed when watchdog timed out
///
ISR(WDT_vect) {
  signal_wdt = 1; // set global flag
}
