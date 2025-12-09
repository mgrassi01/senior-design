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


const int BUFFER1 = 2;
const int BUFFER2 = 2;
const int BUFFER3 = 2;

enum UltrasonicState {FAR, RIGHT_MIDDLE, RIGHT_CLOSE, LEFT_MIDDLE, LEFT_CLOSE, CENTER_MIDDLE, CENTER_CLOSE};

// constant values, can be changed 
const int VERY_DIM = 1.75;
const int BRIGHT = 1.2;
const int LDR_BUFFER = 0.05; 


#define ESP_INTR_FLAG_DEFAULT 0

#define ULTRASONIC_OUTPUT_MASK ((1 << TRIG_PIN_L1) | (1 << TRIG_PIN_L2) | (1 << TRIG_PIN_R1) | (1 << TRIG_PIN_R2))
#define ULTRASONIC_INPUT_MASK ((1 << ECHO_PIN_L1) | (1 << ECHO_PIN_L2) | (1 << ECHO_PIN_R1) | (1 << ECHO_PIN_R2))



void sensors_init(); // initilizes the tilt sensors and ultrasonic ADC pin 
void set_tilt_state(const int, int); // gets the current 4 bit value of tilt
int check_tilt_time(); // sees if the walker has fallen over long enough to consider it a valid fall, returns the state 
