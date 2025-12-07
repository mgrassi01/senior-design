

// ---------- SENSORS ----------
const int FALL_PIN = 13; // 35; //              // goes HIGH when fall is detected

const int LDR_PIN2 = 14;
const int LDR_PIN1 = FALL_PIN; // this might not work lol 
const int POT_PIN = 34;           // ADC1 only


const int TILT_PIN_L = 39;
const int TILT_PIN_R = 36;
const int TILT_PIN_F = 26;
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



// ---------- USER INTERFACE / DISPLAY ----------
#define BUTTON_PIN 27 //gpio button trigger for sensor test

// ---------- BUTTON PINS (safe) ----------
const int BTN1 = 22;
const int BTN2 = 1; // 16; // 1 (on senior design display)
const int BTN3 = 3; // 17; // 3 (on senior design display)
const int BTN4 = 21;

// ---------- LED OUTPUT PINS ----------
const int LED_F1 = 25;   // pick any safe GPIOs
const int LED_F2 = 32;    // (avoid 1/3 UART, 12/13/14/15 boot/SPI)
const int LED_F3 = 33;


// --------- FEEDBACK ---------

const int GREEN_LED_GPIO = 25; // 1 is taken by BTN2 so change this, corresponts to LED_f1
const int YELLOW_LED_GPIO = 32; // correspongs to LED_F2
const int RED_LED_GPIO = 33; // 3 is taken by BTN3, 33 corresponds to LEDF3

const int HEADLIGHT_GPIO = -1; //26; // 26 is TILT_PIN_F, change this

// Define Haptic pin numbers
const int HAPTIC_L_GPIO = -1; // 14; // 14 is LDR_PIN2-change this
const int HAPTIC_R_GPIO = 5;


// Define speaker stuff
//#define SD_CS 7
const int SD_CS = 15; 
// #define SD_CS 15            // Chip select pin for SD card
