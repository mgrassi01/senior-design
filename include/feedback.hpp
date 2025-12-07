#pragma once
void feedback_setup();
void feedback_update();

#define feedback_en
#ifdef feedback_en

// Define LED pin numbers

extern int ultrasonic_state;
extern int ldr_state;

// // define HLS pin numbers

#define I2C_DAC_ADDR 0x4C   // Example I2C address for DAC

// Function declaration
static void handleLEDs(int stateL, int stateR);
void headlights(int ldrState);
void hapticHell(int stateL, int stateR);
void audioLogic(int stateL, int stateR);
void playAudio(const char* filename);

#endif