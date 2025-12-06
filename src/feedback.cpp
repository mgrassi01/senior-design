#include <Arduino.h> 
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <BackgroundAudio.h> // Use ESP32-AudioI2S or similar
#include "feedback.hpp"
#include "sensors.hpp"


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


//used for declaration and run ONCE when the esp32 turns on 
void feedback_setup() 
{
  // Set the LED pins as outputs
  // pinMode(GREEN_LED_GPIO, pin1);   //Need to be changed according to correct pin
  // pinMode(YELLOW_LED_GPIO, pin2);  //Need to be changed according to correct pin
  // pinMode(RED_LED_GPIO, pin3);

  // pinMode(HEADLIGHT_GPIO, pin4);

  // pinMode(HAPTIC_L_GPIO, pin5);
  // pinMode(HAPTIC_R_GPIO, pin6);
  pinMode(GREEN_LED_GPIO, OUTPUT);   //Need to be changed according to correct pin
  pinMode(YELLOW_LED_GPIO, OUTPUT);  //Need to be changed according to correct pin
  pinMode(RED_LED_GPIO, OUTPUT);

  pinMode(HEADLIGHT_GPIO, OUTPUT);

  pinMode(HAPTIC_L_GPIO, OUTPUT);
  pinMode(HAPTIC_R_GPIO, OUTPUT);

  // Example call to the function (you can call it from anywhere)
  handleLEDs(4, 2);

  //for the speaker
  //Serial.begin(115200);   // Cannot remeber if this number is correct
  Wire.begin();           // Initialize I2C
  if (!SD.begin(SD_CS)) 
  {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");
}
//MAIN UPDATE CALLED FROM loop()
void feedback_update() 
{
  int leftDist = (ultrasonic_state >> 3) & 0x7; //L1
  int rightDist = ultrasonic_state       & 0x7;


  //unpack ldr state
  int leftLdr = (ldr_state >> 3) & 0x7;
  int rightLdr = ldr_state       & 0x7;

  int ldrMin = min(leftLdr, rightLdr);

  handleLEDs(leftDist, rightDist);
  hapticHell(leftDist, rightDist);
  headlights(ldrMin);
  audioLogic(leftDist, rightDist);
}

// Function definition
static void handleLEDs(int stateL, int stateR) 
{
  // Find the minimum of the two values
  int minValue = min(stateL, stateR);

  // Conditional logic for LEDs
  if (minValue == 4) 
  {
    digitalWrite(GREEN_LED_GPIO, HIGH);
    digitalWrite(YELLOW_LED_GPIO, LOW);
    digitalWrite(RED_LED_GPIO, LOW);
  } 
  else if (minValue == 2) 
  {
    digitalWrite(GREEN_LED_GPIO, LOW);
    digitalWrite(YELLOW_LED_GPIO, HIGH);
    digitalWrite(RED_LED_GPIO, LOW);
  } 
  else if (minValue == 1) 
  {
    digitalWrite(GREEN_LED_GPIO, LOW);
    digitalWrite(YELLOW_LED_GPIO, LOW);
    digitalWrite(RED_LED_GPIO, HIGH);
  }
}

///////////////
// function defn for headlights
void headlights(int ldrState) 
{
  if (ldrState == 1)
  {
    digitalWrite(HEADLIGHT_GPIO, HIGH);
  }
  else
  {
    digitalWrite(HEADLIGHT_GPIO, LOW);
  }
}

/////////////// 
// function defn for haptic 
void hapticHell(int stateL, int stateR) 
{
  // --- LEFT Side ---
  if (stateL == 2) 
  {
    digitalWrite(HAPTIC_L_GPIO, HIGH);
    delay(500);
    digitalWrite(HAPTIC_L_GPIO, LOW);
    delay(500);
  } 
  else if (stateL == 1) 
  {
    for (int i = 0; i < 2; i++) 
    {
      digitalWrite(HAPTIC_L_GPIO, HIGH);
      delay(250);
      digitalWrite(HAPTIC_L_GPIO, LOW);
      delay(250);
    }
  }
  else 
  {
    digitalWrite(HAPTIC_L_GPIO, LOW);
  }

  // --- RIGHT Side ---
  if (stateR == 2) 
  {
    digitalWrite(HAPTIC_R_GPIO, HIGH);
    delay(500);
    digitalWrite(HAPTIC_R_GPIO, LOW);
    delay(500);
  } 
  else if (stateR == 1) 
  {
    for (int i = 0; i < 2; i++) 
    {
      digitalWrite(HAPTIC_R_GPIO, HIGH);
      delay(250);
      digitalWrite(HAPTIC_R_GPIO, LOW);
      delay(250);
    }
  } 
  else 
  {
    digitalWrite(HAPTIC_R_GPIO, LOW);
  }
}

///////////////
// Audio file logic:
void audioLogic(int stateL, int stateR)
{
  if (stateL <= 2 && stateR <= 2)
  {
    playAudio("Center");
  }
  else if (stateL <= 2)
  {
    playAudio("Left");
  }
  else if (stateR <= 2)
  {
    playAudio("Right");
  }
  else
  {
    return;
  }
}

// function defn for speakers (assuming WAV file and DAC)
void playAudio(const char* filename) 
{
  File audioFile = SD.open(filename);     //opens audio file from SD card

  // if audio file is false (i.e. not openable), prints error message and exits the function early
  if (!audioFile) 
  {
    Serial.println("Failed to open audio file");
    return;
  }

  //loops while there is data left to read in the file (returns number of bytes left to read)
  while (audioFile.available()) 
  {
    byte buffer[64];   //creates buffer to hold chunks of audio data (64 bytes at a time)
    int bytesRead = audioFile.read(buffer, sizeof(buffer));   // reads 64 bytes from the file in buffer
    for (int i = 0; i < bytesRead; i++)  // sends each byte individually to the DAC
    {
      Wire.beginTransmission(I2C_DAC_ADDR);  //begin transmittion 
      Wire.write(buffer[i]); // Send byte to DAC
      Wire.endTransmission();
    }
  }

  audioFile.close();
}
