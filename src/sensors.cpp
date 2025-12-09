#include "sensors.hpp"
#include "pins.hpp"


/*
ultrasonic, tilt, and light sensors. 
*/



int ldr(int);
int ultrasonic(int, int);
void ultrasonic_ldr_isr();
void ultrasonic_init();
void ldr_init();
void tilt_init();
void sensors_timer_init();
void sensors_loop();
void IRAM_ATTR tilt_isr(int);

void IRAM_ATTR isr_tilt_F();
void IRAM_ATTR isr_tilt_B();
void IRAM_ATTR isr_tilt_R();
void IRAM_ATTR isr_tilt_L();

// ----------------------------------- GLOBAL VARIABLES  --------------------------------//
int ldr_state = 2; // start out assuming dim, the in-between value
// volatile int tilt_state = 0; // 3/4 bit val depending on which tilt sensors are enabled
hw_timer_t* timer1 = nullptr;           // 50 Hz tick ??
unsigned long tilt_threshold = 1000 * 3; // 10 seconds ( 3 for testing)
volatile uint32_t last_tilt_us[4] = {0,0,0,0};
const uint32_t tiltdebounceUs = 100;       // 0.1 ms - seems to be better than longer debounces 
portMUX_TYPE tilt_mux = portMUX_INITIALIZER_UNLOCKED; // copied from display for similar isr
volatile int tilt_flags = 0;   // bit0..3 set by ISRs
unsigned long tilt_time = 0; // time at which walker has fallen over and is stable


// enum UiState {UI_MAIN, UI_SETTINGS, UI_ALERT};   // add UI_ALERT
// extern volatile UiState ui = UI_MAIN;           // update your existing declaration


void set_tilt_state(const int gpio_num, int idx){
   
  // if nothing is going on, just return
  if (((tilt_flags >> idx ) & 1) == 0) return;
  if (((tilt_flags >> idx ) & 1) == 1){
    tilt_flags &= ~(1 << idx);
  }

  delay(10); // we may be able to make this delay shorter but 10 ms seems to work
  int level = digitalRead(gpio_num); // tell if its rising or falling - necessary

  // 4 bit val goes like FRONT | BACK | LEFT | RIGHT
  if(level == HIGH){
    tilt_state |= (1 << idx);
    Serial.println("\n tilt state: ");
    Serial.print(tilt_state);
    // reset the timer and start counting again: will count time starting from last debounce
    tilt_time = millis(); // current time in ms
  }

  else if(level == LOW){
    tilt_state &= ~(1 << idx); 
    Serial.println("\n tilt state: ");
    Serial.print(tilt_state);
    // reset the timer and do not start counting again
    tilt_time = 0; // indicator for timer stopped  
  }

}



void IRAM_ATTR tilt_isr(int idx){
  uint32_t now=micros();
  if (now - last_tilt_us[idx] < tiltdebounceUs) return;
  last_tilt_us[idx]=now;
  portENTER_CRITICAL_ISR(&tilt_mux);
  tilt_flags |= (1<<idx); // sets the flag high 
  portEXIT_CRITICAL_ISR(&tilt_mux);
}

void IRAM_ATTR isr_tilt_F(){
  tilt_isr(3);
}
void IRAM_ATTR isr_tilt_B(){
  tilt_isr(2);
}
void IRAM_ATTR isr_tilt_L(){
  tilt_isr(1);
}
void IRAM_ATTR isr_tilt_R(){
  tilt_isr(0);
}



// this is timer triggered and will run after the ultrasonics, in a similar way
int ldr(int pin_num)
{
  int ldr_value = 0; // value read from the ldr
  float ldr_voltage = 0.00;
  int ldr_state_next = 0;

  ldr_value = analogRead(pin_num);
  ldr_voltage = float(ldr_value) * 5.00 / 1023.00;
  Serial.print("ldr voltage = ");
  Serial.println(ldr_voltage);


  if(ldr_state == 1){ // very dim
    if(ldr_voltage < (BRIGHT - LDR_BUFFER)) {
      ldr_state_next = 4; // BRIGHT
    } else if(ldr_voltage < (VERY_DIM - LDR_BUFFER)){
      ldr_state_next = 2; // dim
    } else{
      ldr_state_next = 1; // very dim
    }
    
  } else if (ldr_state == 2) { // dim
    if(ldr_voltage < (BRIGHT - LDR_BUFFER)) {
      ldr_state_next = 4;
    } else if (ldr_voltage > (VERY_DIM + LDR_BUFFER)){
      ldr_state_next = 1;
    } else {
      ldr_state_next = 2;
    }
    
  } else if(ldr_state == 4){ // BRIGHT
    if(ldr_voltage > (VERY_DIM + LDR_BUFFER)){
      ldr_state_next = 1;
    } else if(ldr_voltage > (BRIGHT + LDR_BUFFER)) {
      ldr_state_next = 2;
    } else{
      ldr_state_next = 4;
    }
  }
  

  return ldr_state_next;
}



#ifdef esp_ultrasonics

int ultrasonic(int TRIG_PIN, int ECHO_PIN){

  float timing = 0.0;
  float distance = 0.0; 
  int buffer = 2;

  int next_ultrasonic_state = 0;

  // trigger the ultrasonic in order to get a measurement
  digitalWrite(TRIG_PIN, LOW);
  delay(2);
  digitalWrite(TRIG_PIN, HIGH);
  delay(10);
  digitalWrite(TRIG_PIN, LOW);

  // get the input from the echo pin
  timing = pulseIn(ECHO_PIN, HIGH);
  distance = (timing * 0.034) /(2*2.54);
  distance *= 10/8.85; // adjust for accuracy

  // beginning state
  if(ultrasonic_state == 0){
    if(distance <= 12){
      next_ultrasonic_state = 1; // far
    } else if((distance > 12) && (distance < 24)){
      next_ultrasonic_state = 2; // close
    } else if(distance > 24){
      next_ultrasonic_state = 4; // far
    } 
     
  } 
  // very close 
  else if(ultrasonic_state == 1){
    if(distance > (24 + 2)) {
     next_ultrasonic_state = 4; // far
    } else if(distance > (12 + buffer)) {
      next_ultrasonic_state = 2; // close
    }else {
      next_ultrasonic_state = 1; // very close 
    }
  }
  // clBUFFER2ose 
  else if(ultrasonic_state == 2) {
    if(distance > (24 + buffer)) {
      next_ultrasonic_state = 4;
    } else if(distance < (12 - buffer)){
      next_ultrasonic_state = 1;
    } else {
      next_ultrasonic_state = 2;
    }
  }
  // far
  else if(ultrasonic_state == 4) {
    if (distance < (12 - buffer)){
      next_ultrasonic_state = 1; // transition to very close
    } else if (distance < (24 - buffer)){
      next_ultrasonic_state = 2; // transition to close
    } else {
      next_ultrasonic_state = 4; // stay the same
    }
    
  }

  ultrasonic_state = next_ultrasonic_state;

  return ultrasonic_state;
}
#endif
 
  #ifdef esp_ultrasonics


  void ultrasonic_ldr_isr(){
    // call the ultrasonic function and get the value
    // call it for L1 pins

    int L1_ultrasonic_state = 0;
    int L2_ultrasonic_state = 0;
    int R1_ultrasonic_state = 0;
    int R2_ultrasonic_state = 0;

    L1_ultrasonic_state = ultrasonic(TRIG_PIN_L1, ECHO_PIN_L1);
    R1_ultrasonic_state = ultrasonic(TRIG_PIN_R1, ECHO_PIN_R1);
  
    // the order is L2 | R2 | L1 | R1, MSB is L2
    ultrasonic_state = (L1_ultrasonic_state << 3) | (R1_ultrasonic_state ) | (L2_ultrasonic_state << 9) | (R2_ultrasonic_state << 6); // update the state
    

    int ldr1_state = ldr(LDR_PIN1);
    int ldr2_state = ldr(LDR_PIN2);
    ldr_state = (ldr1_state << 3) | ldr2_state;



    
    // arm the alarm for 1,5, or 10 seconds 
    int target = 1; // in seconds
    timerAlarmWrite(timer1, 1000*1000 * target, true); // in us
    timerAlarmEnable(timer1);

  }

  #endif


void sensors_timer_init(){
    // add an interrupt handler
    // make the ultrasonic_ldr_isr the interrupt handler
    // set the alarm for 1 or 5 seconds
    timer1 = timerBegin(1, 80, true);// guessing this brings it down to 1 us?
    timerAttachInterrupt(timer1, &ultrasonic_ldr_isr, true);
    timerAlarmWrite(timer1, 1000*1000, true);
    timerAlarmEnable(timer1);

}


void tilt_init(){
  pinMode(TILT_PIN_B, INPUT);
  pinMode(TILT_PIN_F, INPUT);
  pinMode(TILT_PIN_L, INPUT);
  pinMode(TILT_PIN_R, INPUT);

  attachInterrupt(digitalPinToInterrupt(TILT_PIN_F), isr_tilt_F, CHANGE); // needs to be change 
  // attachInterrupt(digitalPinToInterrupt(TILT_PIN_B), isr_tilt_B, CHANGE); // not sure this will work
  // attachInterrupt(digitalPinToInterrupt(TILT_PIN_L), isr_tilt_L, CHANGE); // not sure this will work
  // attachInterrupt(digitalPinToInterrupt(TILT_PIN_R), isr_tilt_R, CHANGE); // not sure this will work


}

void ultrasonic_init() {
  #ifndef esp_ultrasonics
  Serial.println("\nWe are not using the Ultrasonics on ESP32. Please connect the arduino. \n");
  pinMode(ULTRASONIC_PIN, ANALOG);
  #endif
  
  #ifdef esp_ultrasonics
  pinMode(ECHO_PIN_L1, INPUT);
  pinMode(ECHO_PIN_R1, INPUT);
  pinMode(ECHO_PIN_L2, INPUT);
  pinMode(ECHO_PIN_R2, INPUT);

  pinMode(TRIG_PIN_L1, OUTPUT);
  pinMode(TRIG_PIN_R1, OUTPUT);
  pinMode(TRIG_PIN_L2, OUTPUT);
  pinMode(TRIG_PIN_R2, OUTPUT);

  digitalWrite(TRIG_PIN_L1, LOW);
  digitalWrite(TRIG_PIN_R1, LOW);
  digitalWrite(TRIG_PIN_L2, LOW);
  digitalWrite(TRIG_PIN_R2, LOW);

  #endif
}

void ldr_init() {
  pinMode(LDR_PIN1, INPUT);
  pinMode(LDR_PIN2, INPUT);
}


// when the gold lead is tilted DOWN, the circuit closes. 
// put the gold lead facing upwards and connec that to the input pin
// connect the silver lead to Vcc
void sensors_init() {
  // ldr_init(); // dont need this if doing it the analog way 
  ultrasonic_init();
  tilt_init();  
  // sensors_timer_init(); // this appears to be causing sthe micro to reset
}

int check_tilt_time(){
  // if the timer has not been set to 0 (indicating off) 
  // and the time the walker has been tipped is greater than 10 seconds
  // then set the ui to UI_ALERT state 
  int alert = UI_MAIN;// will have to change based on settings

  // for some reason this isnt registering the time
  if((tilt_time > 0) && ((millis() - tilt_time ) > tilt_threshold)) {
    if( tilt_state!=0){
      alert = UI_ALERT;
      Serial.println("\nwalker has tipped over");
      tilt_time = 0; // turn off the timer so it doesnt keep sending the message 
    }
  }
  return(alert);

}