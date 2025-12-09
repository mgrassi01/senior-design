// ----- DO NOT USE PINS 1 or 3 FOR ANYTHING IF YOU WANT SERIAL PRINT STATEMENTS!!!

// ---------- IF DEFS -----

//#define wifi_en
// #define esp_ultrasonics

// ---------- EXTERNS ----------
enum UiState {UI_MAIN, UI_SETTINGS, UI_ALERT};   // add UI_ALERT

extern enum UltrasonicState ultrasonic_state;

extern volatile int tilt_state;


// for the feedback functions that can be turned on and off by pushbuttons
extern volatile bool audio ;
extern volatile bool haptics;
extern volatile bool lights ;



// ---------- SENSORS ----------
const int FALL_PIN = 13; // 35; //              // goes HIGH when fall is detected

const int LDR_PIN2 = 14;
const int LDR_PIN1 = FALL_PIN; // this might not work lol 
const int POT_PIN = 34;           // ADC1 only


const int TILT_PIN_L = 39;
const int TILT_PIN_R = 36;
const int TILT_PIN_F = 12;
const int TILT_PIN_B = 27;

// # define esp_utrasonics // since we are using the arduino instead
#ifdef esp_ultrasonics 
    const int TRIG_PIN_L1 =  3;
    const int TRIG_PIN_R1 =  19;
    const int TRIG_PIN_L2 =  15; 
    const int TRIG_PIN_R2 =  16;

    const int ECHO_PIN_L1 =  35;
    const int ECHO_PIN_R1 =  18;
    const int ECHO_PIN_L2 =  12;
    const int ECHO_PIN_R2 =  17;

#endif
#ifndef esp_ultrasonics
    const int ULTRASONIC_PIN = -1; // change later
#endif


// ---------- USER INTERFACE / DISPLAY ----------
#define BUTTON_PIN 27; //gpio button trigger for sensor test

// ---------- BUTTON PINS (safe) ----------
const int LIGHTS_BTN_PIN = 22; // F1
const int HAPTICS_BTN_PIN = 21; // 16 originally SPEAKER 
const int AUDIO_BTN_PIN = 17; // AUDIO
const int BTN4 = 16; // 21 originally. not working for some reason, supposed to be main uI

// ---------- LED OUTPUT PINS ----------
// const int LED_F1 = 25;   // pick any safe GPIOs
// const int LED_F2 = 32;    // (avoid 1/3 UART, 12/13/14/15 boot/SPI)
// const int LED_audio = 33;


// --------- FEEDBACK ---------

const int GREEN_LED_GPIO = 25; // 1 is taken by SPEAKER_BTN_PIN so change this, corresponts to LED_f1
const int YELLOW_LED_GPIO = 32; // correspongs to LED_F2
const int RED_LED_GPIO = 33; // 3 is taken by AUDIO_BTN_PIN, 33 corresponds to LEDaudio

const int HEADLIGHT_GPIO_LEFT = -1; //26; // 26 is TILT_PIN_F, change this
const int HEADLIGHT_GPIO_RIGHT = -1; 

// Define Haptic pin numbers
const int HAPTIC_L_GPIO = -1; // 14; // 14 is LDR_PIN2-change this
const int HAPTIC_R_GPIO = 5;


// Define speaker stuff
//#define SD_CS 7
const int SD_CS = 15; 
// #define SD_CS 15            // Chip select pin for SD card
