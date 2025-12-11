#include "io.h"
#include "pins.h"

// internal LED array for mapping
static const int ledPins[3] = { LED_A, LED_B, LED_C };

void io_init() {
  // ---------------------- BUTTONS ----------------------
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);
  pinMode(BUTTON4, INPUT_PULLUP);

  // ---------------------- OUTPUTS ----------------------
  pinMode(HEADLIGHT, OUTPUT);
  pinMode(ERM1, OUTPUT);
  pinMode(ERM2, OUTPUT);

  pinMode(LED_A, OUTPUT);
  pinMode(LED_B, OUTPUT); // ensure external pull-up!
  pinMode(LED_C, OUTPUT);

  // ---------------------- SENSOR INPUTS ----------------
  pinMode(TILT1, INPUT);
  pinMode(TILT2, INPUT);
  pinMode(TILT3, INPUT);


  // ---------------------- ADC SETUP --------------------
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_BATTERY, ADC_11db);
  analogSetPinAttenuation(ADC_ULTRA, ADC_11db);

  // ---------------------- DAC OUTPUT -------------------
}

// Read a button by index 0–3
bool io_readButton(int idx) {
  switch (idx) {
    case 0: return digitalRead(BUTTON1) == LOW;
    case 1: return digitalRead(BUTTON2) == LOW;
    case 2: return digitalRead(BUTTON3) == LOW;
    case 3: return digitalRead(BUTTON4) == LOW;
  }
  return false;
}

// Read filtered potentiometer (0–4095)
int io_readPot() {
  return analogRead(ADC_BATTERY);  // POT_PIN replaced with ADC_BATTERY --- IGNORE ---
}

int io_readBattery() {
  return analogRead(ADC_BATTERY);
}

int io_readUltrasonic() {
  return analogRead(ADC_ULTRA);
}

void io_setLED(int idx, bool on) {
  if (idx < 0 || idx > 2) return;
  digitalWrite(ledPins[idx], on ? HIGH : LOW);
}
