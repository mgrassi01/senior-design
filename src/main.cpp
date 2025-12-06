#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"
#include "display.h"
#include "sensors.hpp"
#include "feedback.hpp"
#define BUTTON_PIN 27 //gpio button trigger for sensor test
const uint32_t SMS_COOLDOWN_MS = 5UL * 60UL * 1000UL; //5 mins
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
    display_setup();

    sensors_loop();

    feedback_setup();

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

    display_loop();
    feedback_update();
    bool nowAlert = display_in_alert();
    uint32_t now = millis();

    if (nowAlert && !prevAlert) {
        Serial.println("Entered ALERT State");
        if(now - lastSmsMs > SMS_COOLDOWN_MS){
            Serial.println("Sending SMS via IFTTT...");
            sendIFTTT("Walker has fallen");
            lastSmsMs = now;
        } else{
            Serial.println("SMS supressed (software cooldown)");
        }
    }
    
    if(!nowAlert && prevAlert) {
        Serial.println("Exited ALERT state");
    }
                                                                    
    prevAlert = nowAlert;

    delay(10);
}