#pragma once
void feedback_setup();
void feedback_update();

// #define feedback_en
#ifdef feedback_en

// Define LED pin numbers
//const int GREEN_LED_GPIO = 1; 
//const int YELLOW_LED_GPIO = 2;
//const int RED_LED_GPIO = 3;
extern int ultrasonic_state;
extern int ldr_state;

const int GREEN_LED_GPIO = 18;
const int YELLOW_LED_GPIO = 19;
const int RED_LED_GPIO = 23;
// define HLS pin numbers
const int HEADLIGHT_GPIO = 26;

// Define Haptic pin numbers
const int HAPTIC_L_GPIO = 14;
const int HAPTIC_R_GPIO = 5;
// Define speaker stuff
//#define SD_CS 7
#define SD_CS 15            // Chip select pin for SD card
#define I2C_DAC_ADDR 0x4C   // Example I2C address for DAC

// Function declaration
static void handleLEDs(int stateL, int stateR);
void headlights(int ldrState);
void hapticHell(int stateL, int stateR);
void audioLogic(int stateL, int stateR);
void playAudio(const char* filename);

#endif