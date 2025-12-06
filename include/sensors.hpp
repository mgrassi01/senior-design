#ifndef SENSORS_HPP
#define SENSORS_HPP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <TFT_eSPI.h>
#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_sntp.h"
#include "rom/gpio.h"
#include "esp32-hal-timer.h"
#include "esp_timer.h"
#include "esp32-hal-gpio.h"

const float ULTRASONIC_NOTHING_SEEN = 48;
const float ANGLE_RATIO = 48/50.92;

#define LDR_PIN1 13 // for ADC1_CH0
#define LDR_PIN2 14 // for ADC1_CH2 // i think we can do this

// GPIO output/input 


const int TILT_PIN_L = 39;
const int TILT_PIN_R = 36;
const int TILT_PIN_F = 26;
const int TILT_PIN_B = 27;

// ---------- BUTTON PINS (safe) ----------
const int BTN1 = 22;
const int BTN2 = 1;
const int BTN3 = 3;
const int BTN4 = 21;
// ---------- LED OUTPUT PINS ----------
const int LED_F1 = 25;   // pick any safe GPIOs
const int LED_F2 = 32;    // (avoid 1/3 UART, 12/13/14/15 boot/SPI)
const int LED_F3 = 33;


// ---------- ADC ----------
const int POT_PIN = 34; 
const int FALL_PIN = 13;                 // goes HIGH when fall is detected


const int TRIG_PIN_L1 =  3;
const int TRIG_PIN_R1 =  19;
const int TRIG_PIN_L2 =  15; 
const int TRIG_PIN_R2 =  16;

const int ECHO_PIN_L1 =  35;
const int ECHO_PIN_R1 =  18;
const int ECHO_PIN_L2 =  12;
const int ECHO_PIN_R2 =  17;

#define BUFFER1 2;
#define BUFFER2 2;
#define BUFFER3 2;

// constant values, can be changed 
#define VERY_DIM 1.75
#define BRIGHT 1.2
#define LDR_BUFFER 0.05 


#define ESP_INTR_FLAG_DEFAULT 0

#define ULTRASONIC_OUTPUT_MASK ((1 << TRIG_PIN_L1) | (1 << TRIG_PIN_L2) | (1 << TRIG_PIN_R1) | (1 << TRIG_PIN_R2))
#define ULTRASONIC_INPUT_MASK ((1 << ECHO_PIN_L1) | (1 << ECHO_PIN_L2) | (1 << ECHO_PIN_R1) | (1 << ECHO_PIN_R2))

int ldr(int);
int ultrasonic(int, int);
void ultrasonic_ldr_isr();
void ultrasonic_init();
void ldr_init();
void tilt_init();
void timer0_init();
void sensors_loop();
  

#endif