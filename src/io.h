#pragma once
#include <Arduino.h>
#include "pins.h"

// Public functions
void io_init();
bool io_readButton(int idx);
int  io_readPot();
int  io_readBattery();
int  io_readUltrasonic();
void io_setLED(int idx, bool on);

// External boolean references
extern volatile bool speakerOn;
extern volatile bool headlightOn;
extern volatile bool ermOn;

// LED index mapping
enum {
  LED_IDX_F1 = 0,
  LED_IDX_F2 = 1,
  LED_IDX_F3 = 2
};
