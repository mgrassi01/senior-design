#pragma once

// =====================================================
//  ESP32-WROOM-32 Pin Assignments
// =====================================================

// -------------------- TFT DISPLAY ---------------------
#define TFT_MISO      19
#define TFT_MOSI      23
#define TFT_SCLK      18
#define TFT_CS        15
#define TFT_DC        2     // Boot strap → must stay HIGH on boot
#define TFT_RST       4     // Boot strap, keep HIGH on boot

// -------------------- EXTRA SPI DEVICE ----------------
#define SPI_CS2       27

// -------------------- OUTPUTS -------------------------
#define HEADLIGHT     21
#define ERM1          14
#define ERM2          26

#define LED_A         17
#define LED_B          0     // Requires pull-up, boot-sensitive  
#define LED_C         16

// -------------------- INPUT BUTTONS -------------------
#define BUTTON1       35    // input-only, supports interrupts
#define BUTTON2       34    // input-only, supports interrupts
#define BUTTON3       25    // input-only, supports interrupts
#define BUTTON4       39    // input-only, supports interrupts

// -------------------- TILT SENSORS ---------------------
#define TILT1          5
#define TILT2         22
#define TILT3         13

// -------------------- ANALOG INPUTS --------------------
#define ADC_BATTERY   32    // ADC1
#define ADC_ULTRA     33    // ADC1


