#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"
#include "display.h"
#include "sensors.hpp"
#include "feedback.hpp"
#include "pins.hpp"

// ---------- EXTERNS (declared in pin.hpp) ---------- //
volatile int       tilt_state;
enum UltrasonicState ultrasonic_state = FAR;
bool walker_fallen = false;


// for the feedback functions that can be turned on and off by pushbuttons
bool audio   = true;
bool haptics = true;
bool lights  = true;




const uint32_t SMS_COOLDOWN_MS = 1UL * 10UL * 1000UL; //10 seconds (changed from 5 minutes)
uint32_t lastSmsMs = 0;



void sendIFTTT(String msg){

    if(WiFi.status() == WL_CONNECTED){
        HTTPClient http;

        String url = String("https://maker.ifttt.com/trigger/") + IFTTT_EVENT + "/with/key/" + IFTTT_KEY;
        //Serial.print("URL = ["); //Serial.print(url); //Serial.print("]");
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        String body = String("{\"value1\":\"")+ msg + "\"}";
        int code = http.POST(body);

        Serial.print("HTTP Response code: ");
        Serial.println(code);
        http.end();
    }
        else{
            Serial.println("Wi-Fi not connected");
        }   
       
}


void setup(){
    //pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);
    
#ifdef wifi_en
    //connect wifi
    // Serial.print("ssid=");
    // Serial.print(WIFI_SSID);
    Serial.print("connecting wifi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.println(".");
    }
    Serial.println("\nWi-Fi connected!");
#endif

    display_setup();
    Serial.println("\ndisplay has been setup");

    sensors_init();
    Serial.println("\nsensors have been setup");


    #ifdef feedback_en

        feedback_setup();
    #endif

}

// void loop(){
//     uint32_t now = millis();
//     int cur = digitalRead(BUTTON_PIN);
//     if(lastBtn == HIGH && cur == LOW ) {
//         Serial.print("Edge detected at ms =");
//         Serial.println(now);
//         if((now - lastFireMs) > COOLDOWN_MS){
//             Serial.println("Button pressed -> sending SMS");
//             sendIFTTT("Button pressed - sending SMS");
//             lastFireMs = now;
//         } else{
//             Serial.println("ignored still in cooldown");
//         }
//     }
//     lastBtn = cur;


//     display_loop();
//     delay(10);

// }

void loop() {
    static bool prevAlert = false;
    static bool testtrig = false;
    // if(!testtrig){
    //     tilt_state = 1;
    //     Serial.println("tilt to 1 test");
    //     testtrig = true;
    // }
    display_loop();
    #ifdef feedback_en
        feedback_update();
    #endif
    bool nowAlert = display_in_alert();
    uint32_t now = millis();

    if (nowAlert && !prevAlert) {
        Serial.println("Entered ALERT State");
        if(now - lastSmsMs > SMS_COOLDOWN_MS){
            Serial.println("Sending SMS via IFTTT...");
            #ifdef wifi_en
            sendIFTTT("Walker has fallen");
            #endif
            lastSmsMs = now;
        } else{
            Serial.println("SMS supressed (software cooldown)");
        }
    }
    
    if(!nowAlert && prevAlert) {
        Serial.println("Exited ALERT state");
        if(cancel_pressed()){
            Serial.println("User cancelled alert");
        }
    }
                                                                    
    prevAlert = nowAlert;

    // maybe goes first? 

    // set_tilt_state(TILT_PIN_R, 0);
    // set_tilt_state(TILT_PIN_L, 1);
    // set_tilt_state(TILT_PIN_B, 2);
    set_tilt_state(TILT_PIN_F, 3);
    
    nowAlert = check_tilt_time();

    enum UltrasonicState prev_ultrasonic_state = ultrasonic_state;
    ultrasonic_state = get_ultrasonic_state(prev_ultrasonic_state);
    update_ultrasonic_led(ultrasonic_state);
    if(haptics){
        update_ultrasonic_haptics(ultrasonic_state);
    }

    delay(10);
}