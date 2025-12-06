#include <TFT_eSPI.h>
#include <Arduino.h>
#include "sensors.hpp"

TFT_eSPI tft;

// // ---------- BUTTON PINS (safe) ----------
// const int BTN1 = 22;
// const int BTN2 = 1;
// const int BTN3 = 3;
// const int BTN4 = 21;
// // ---------- LED OUTPUT PINS ----------
// const int LED_F1 = 25;   // pick any safe GPIOs
// const int LED_F2 = 32;    // (avoid 1/3 UART, 12/13/14/15 boot/SPI)
// const int LED_F3 = 33;


// // ---------- ADC ----------
// const int POT_PIN = 34;           // ADC1 only
int adc_min_cal = 0, adc_max_cal = 4095;
float potFilt = 0.0f;
const float alpha = 0.15f;
// ---------- FALL ALERT (GPIO13) ----------
// const int FALL_PIN = 13;                 // goes HIGH when fall is detected
enum UiState { UI_MAIN, UI_SETTINGS, UI_ALERT };   // add UI_ALERT
volatile UiState ui = UI_MAIN;           // update your existing declaration

// Cooldown after user acknowledges (10 minutes)
const uint32_t FALL_COOLDOWN_MS = 5UL * 60UL * 1000UL;
uint32_t fallCooldownUntilMs = 0;

// Simple debounce: require sustained HIGH for this long to trigger
const uint32_t FALL_DEBOUNCE_MS = 100;
uint32_t fallHighSinceMs = 0;

// ---------- UI ----------
int borderThickness = 10;
volatile int variable0_100 = 100;
uint16_t lastBorderColor = 0;

// toggles
volatile bool f1On = false, f2On = false, f3On = false;

// ---------- Interrupt flags / debounce ----------
volatile uint32_t buttonEdgeFlags = 0;   // bit0..3 set by ISRs
volatile bool tickFlag = false;          // timer ISR flag
volatile uint32_t lastEdgeUs[4] = {0,0,0,0};
const uint32_t debounceUs = 120000;       // 80ms (stiffer)
uint32_t lastUiChangeMs = 0;
const uint32_t uiCooldownMs = 200;       // 200ms cooldown between screen switches

// ---------- Timer ----------
hw_timer_t* uiTimer = nullptr;           // 50 Hz tick

// ---------- Geometry cache for partial redraws ----------
struct Rect { int x,y,w,h; };
Rect gF1, gF2, gF3, gMain;

// ---------- Colors ----------
uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
uint16_t hsv_to_565(float h, float s, float v) {
  float c=v*s, x=c*(1-fabs(fmod(h/60.0f,2)-1)), m=v-c;
  float r=0,g=0,b=0;
  if(h<60){r=c;g=x;} else if(h<120){r=x;g=c;}
  else if(h<180){g=c;b=x;} else if(h<240){g=x;b=c;}
  else if(h<300){r=x;b=c;} else {r=c;b=x;}
  return rgb888_to_565((uint8_t)((r+m)*255),(uint8_t)((g+m)*255),(uint8_t)((b+m)*255));
}
uint16_t borderColorFromVariable(int v) {
  v = constrain(v,0,100);
  return hsv_to_565(120.0f*(v/100.0f),1.0f,1.0f); // red->green
}

// ---------- Draw helpers ----------
void drawBorder(int thickness, uint16_t color) {
  int w=tft.width(), h=tft.height();
  for (int i=0;i<thickness;i++) tft.drawRect(i,i,w-2*i,h-2*i,color);
}
void clearInnerArea(int thickness) {
  int w=tft.width(), h=tft.height();
  tft.fillRect(thickness, thickness, w-2*thickness, h-2*thickness, TFT_BLACK);
}
void drawButton(int x, int y, int w, int h, const char* label, uint16_t outline, uint16_t fill) {
  tft.fillRoundRect(x, y, w, h, 6, fill);
  tft.drawRoundRect(x, y, w, h, 6, outline);

  const int fontNum = 4;  // Larger, bolder font
  const int yNudge  = 3;  // Small offset to vertically center perfectly

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, fill);
  tft.drawString(label, x + w / 2, y + h / 2 + yNudge, fontNum);
}

void drawToggleButton(int x, int y, int w, int h, const char* label, bool on) {
  uint16_t fill = on ? TFT_GREEN : TFT_RED;
  tft.fillRoundRect(x, y, w, h, 6, fill);
  tft.drawRoundRect(x, y, w, h, 6, TFT_YELLOW);

  const int fontNum = 4;  // Bolder text
  const int yNudge  = 3;  // Center correction

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, fill);
  String s = String(label) + (on ? " ON" : " OFF");
  tft.drawString(s, x + w / 2, y + h / 2 + yNudge, fontNum);
}

void drawBattery(int pct){
  int w=tft.width(); int x=borderThickness; int y=borderThickness+2; int h=30;
  int innerW = w-2*borderThickness;
  tft.fillRect(x,y,innerW,h,TFT_BLACK);
  tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String("Battery: ")+pct+"%", x+innerW/2, y+h/2, 4);
}

// ---------- Screens ----------
void renderMainOnce(){
  uint16_t c=borderColorFromVariable(variable0_100); lastBorderColor=c;
  drawBorder(borderThickness,c); clearInnerArea(borderThickness);

  int w=tft.width(), h=tft.height();
  int bw=w*3/4, bh=100, bx=(w-bw)/2, by=(h-bh)/2;
  drawButton(bx,by,bw,bh,"Settings",TFT_WHITE,TFT_DARKGREY);
  drawBattery(variable0_100);
}
void renderAlertOnce() {
  // Full-screen red background (ignore border/UI layout)
  tft.fillScreen(TFT_RED);

  // Big white message centered
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);

  int w = tft.width();
  int h = tft.height();

  // Split message into two lines for readability
  tft.drawString("We have detected the walker has fallen.", w/2, h/2 - 20, 4);
  tft.drawString("Click any button if false alarm.",        w/2, h/2 + 20, 4);
}

void renderSettingsOnce() {
  // --- Border & clear ---
  uint16_t c = borderColorFromVariable(variable0_100);
  lastBorderColor = c;
  drawBorder(borderThickness, c);
  clearInnerArea(borderThickness);

  // --- Geometry that respects border + battery strip ---
  const int topStripH = 30;            // height of your battery strip
  const int gapTop    = 6;             // small gap under the strip
  const int pad       = 16;            // inner padding at left/right and between widgets
  const int cornerR   = 6;             // same round-rect radius used in buttons

  const int W = tft.width();
  const int H = tft.height();

  // Inner content rect (inside the border)
  const int innerX = borderThickness;
  const int innerY = borderThickness;
  const int innerW = W - 2 * borderThickness;
  const int innerH = H - 2 * borderThickness;

  // Content area *below* the battery strip
  const int contentX = innerX + pad;
  const int contentY = innerY + topStripH + gapTop;
  const int contentW = innerW - 2 * pad;
  const int contentH = innerH - topStripH - gapTop - pad; // leave bottom pad

  // 2 columns x 2 rows grid (F1,F2 on row1; F3, Main UI on row2)
  const int cols = 2;
  const int rows = 2;
  const int colGap = pad;
  const int rowGap = pad;

  // compute cell width/height that fully fit
  const int colW = (contentW - (cols - 1) * colGap) / cols;
  const int rowH = (contentH - (rows - 1) * rowGap) / rows;

  // top-left of each cell
  const int x1 = contentX;
  const int x2 = contentX + colW + colGap;
  const int y1 = contentY;
  const int y2 = contentY + rowH + rowGap;

  // cache rects
  gF1   = { x1, y1, colW, rowH };
  gF2   = { x2, y1, colW, rowH };
  gF3   = { x1, y2, colW, rowH };
  gMain = { x2, y2, colW, rowH };

  // draw buttons (uses your centered text with y-nudge inside draw* functions)
  drawToggleButton(gF1.x, gF1.y, gF1.w, gF1.h, "F1", f1On);
  drawToggleButton(gF2.x, gF2.y, gF2.w, gF2.h, "F2", f2On);
  drawToggleButton(gF3.x, gF3.y, gF3.w, gF3.h, "F3", f3On);
  drawButton      (gMain.x, gMain.y, gMain.w, gMain.h, "Main UI", TFT_WHITE, TFT_DARKGREY);

  // battery strip (drawn last, stays under the border and above buttons)
  drawBattery(variable0_100);
}


// ---------- Partial redraw helpers (no whole-screen clears) ----------
void updateToggleF1(){ drawToggleButton(gF1.x,gF1.y,gF1.w,gF1.h,"F1",f1On); }
void updateToggleF2(){ drawToggleButton(gF2.x,gF2.y,gF2.w,gF2.h,"F2",f2On); }
void updateToggleF3(){ drawToggleButton(gF3.x,gF3.y,gF3.w,gF3.h,"F3",f3On); }

void applyOutputs() {
  digitalWrite(LED_F1, f1On ? HIGH : LOW);
  digitalWrite(LED_F2, f2On ? HIGH : LOW);
  digitalWrite(LED_F3, f3On ? HIGH : LOW);
}


// ---------- ISRs ----------
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR handleBtnISR(int idx){
  uint32_t now=micros();
  if (now - lastEdgeUs[idx] < debounceUs) return;
  lastEdgeUs[idx]=now;
  portENTER_CRITICAL_ISR(&mux);
  buttonEdgeFlags |= (1u<<idx);
  portEXIT_CRITICAL_ISR(&mux);
}
void IRAM_ATTR isrBtn1(){ handleBtnISR(0); }
void IRAM_ATTR isrBtn2(){ handleBtnISR(1); }
void IRAM_ATTR isrBtn3(){ handleBtnISR(2); }
void IRAM_ATTR isrBtn4(){ handleBtnISR(3); }

void IRAM_ATTR onTick(){ tickFlag = true; } // timer ISR only sets a flag

// ---------- ADC read (in loop) ----------
int readPotPercent(){
  int raw = analogRead(POT_PIN);
  raw = constrain(raw, adc_min_cal, adc_max_cal);
  float norm = (float)(raw - adc_min_cal) / (float)(adc_max_cal - adc_min_cal);
  norm = constrain(norm, 0.0f, 1.0f);
  potFilt = (1.0f - alpha)*potFilt + alpha*norm;
  return (int)roundf(potFilt*100.0f);
}

// ---------- Setup ----------


















#include "sensors.hpp"


/*
ultrasonic, tilt, and light sensors. 
*/

// ----------------------------------- GLOBAL VARIABLES  --------------------------------//
int ldr_state = 2; // start out assuming dim, the in-between value
int ultrasonic_state = 0; 
int tilt_state = 0;
hw_timer_t* timer0 = nullptr;           // 50 Hz tick ??


void renderTiltOnce() {
  // Full-screen red background (ignore border/UI layout)
  tft.fillScreen(TFT_BLUE);

  // Big white message centered
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);

  int w = tft.width();
  int h = tft.height();

  // Split message into two lines for readability
  tft.drawString("We have detected the walker has tipped", w/2, h/2 - 20, 4);
  tft.drawString("Click any button if false alarm",        w/2, h/2 + 20, 4);
}


void IRAM_ATTR tilt_isr(const int gpio_num){
  // uint32_t gpio_num = (uint32_t) arg;
  // gpio_num_t gpio_numT = (gpio_num_t)gpio_num;

  int level = digitalRead((int)gpio_num); // another option: use the arduino logic
  // int level = gpio_get_level(gpio_numT); // check and see what the level is 

  // 4 bit val goes like FRONT | BACK | LEFT | RIGHT
  if(level == HIGH){
    digitalWrite(LED_F3, HIGH);
    // renderTiltOnce();

    if(gpio_num == TILT_PIN_R) {
      tilt_state |= 1;
    } else if(gpio_num == TILT_PIN_L) {
      tilt_state |= (1 << 1);
      digitalWrite(LED_F3, HIGH);

    } else if(gpio_num == TILT_PIN_B) {
      tilt_state |= (1 << 2);
    } else if(gpio_num == TILT_PIN_F) {
      tilt_state |= (1 << 3);
    }
  }

  else if(level == LOW){
    digitalWrite(LED_F3, LOW);
    if(gpio_num == TILT_PIN_R) {
      tilt_state &= ~(1 << 0);
    } else if(gpio_num == TILT_PIN_L) {
      tilt_state &= ~(1 << 1);
      digitalWrite(LED_F3, LOW);

    } else if(gpio_num == TILT_PIN_F) {
      tilt_state &= ~(1 << 2);
    } else if(gpio_num == TILT_PIN_B) {
      tilt_state &= ~(1 << 3);
    }
  }


}
void IRAM_ATTR isr_tilt_F(){
  tilt_isr(TILT_PIN_F);
}
void IRAM_ATTR isr_tilt_B(){
  tilt_isr(TILT_PIN_B);
}
void IRAM_ATTR isr_tilt_L(){
  tilt_isr(TILT_PIN_L);
}
void IRAM_ATTR isr_tilt_R(){
  tilt_isr(TILT_PIN_R);
}


void tilt_init(){
  pinMode(TILT_PIN_B, INPUT);
  pinMode(TILT_PIN_F, INPUT);
  pinMode(TILT_PIN_L, INPUT);
  pinMode(TILT_PIN_R, INPUT);
  
  // attachInterrupt(digitalPinToInterrupt(TILT_PIN_F), isr_tilt_F, RISING); // not sure this will work
  // attachInterrupt(digitalPinToInterrupt(TILT_PIN_B), isr_tilt_B, RISING); // not sure this will work
  // attachInterrupt(digitalPinToInterrupt(TILT_PIN_L), isr_tilt_L, RISING); // not sure this will work
  // attachInterrupt(digitalPinToInterrupt(TILT_PIN_R), isr_tilt_R, RISING); // not sure this will work

}

// this is timer triggered and will run after the ultrasonics, in a similar way
int ldr(int pin_num)
{
  int ldr_value = 0; // value read from the ldr
  float ldr_voltage = 0.00;
  int ldr_state_next = 0;

  ldr_value = analogRead(pin_num);
  ldr_voltage = float(ldr_value) * 5.00 / 1023.00;
  // Serial.print("ldr voltage = ");
  // Serial.println(ldr_voltage);


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
  distance = (timing * 0.034) /(2*2.54) * ANGLE_RATIO;
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
  // close 
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

  // Serial.print("Distance : ");
  // Serial.print(distance);
  // Serial.print(" in.  | ultrasonic state: ");
  // Serial.println(ultrasonic_state);

  // if(distance < 1) {
  //   Serial.println("Ouch! Stop hitting me!");
  // }  

  return ultrasonic_state;
}
 
uint8_t tilt_vals[] = {0,0,0,0}; // {F, B, L, R} gets 8 measurements 


void get_tilt(int tilt_pin, int index){
  int val = digitalRead((int)tilt_pin);
  tilt_vals[index] = (tilt_vals[index] << 1) | val;
  // renderTiltOnce();
}


int ledF1OnOff = 0;

void ultrasonic_ldr_tilt_isr(){
  // call the ultrasonic function and get the value
  // call it for L1 pins

 ledF1OnOff = !ledF1OnOff;
 digitalWrite(LED_F1, ledF1OnOff);

  int L1_ultrasonic_state = 0;
  int L2_ultrasonic_state = 0;
  int R1_ultrasonic_state = 0;
  int R2_ultrasonic_state = 0;

  get_tilt(TILT_PIN_F, 3);
  get_tilt(TILT_PIN_B, 2);
  get_tilt(TILT_PIN_L, 1);
  get_tilt(TILT_PIN_R, 0);


  // L1_ultrasonic_state = ultrasonic(TRIG_PIN_L1, ECHO_PIN_L1);
  // R1_ultrasonic_state = ultrasonic(TRIG_PIN_R1, ECHO_PIN_R1);

  // // the order is L2 | R2 | L1 | R1, MSB is L2
  // ultrasonic_state = (L1_ultrasonic_state << 3) | (R1_ultrasonic_state ) | (L2_ultrasonic_state << 9) | (R2_ultrasonic_state << 6); // update the state
  
  // int ldr1_state = ldr(LDR_PIN1);
  // int ldr2_state = ldr(LDR_PIN2);
  // ldr_state = (ldr1_state << 3) | ldr2_state;
  
  // arm the alarm for 1,5, or 10 seconds 
  // int target = 1; // in seconds
  // timerAlarmWrite(timer0, 1000*1000 * target, true); // in us
  // timerAlarmEnable(timer0);

}

void timer0_init(){
    // add an interrupt handler
    // make the ultrasonic_ldr_isr the interrupt handler
    // set the alarm for 1 or 5 seconds
    timer0 = timerBegin(0, 80, true); // guessing this brings it down to 1 us?
    timerAttachInterrupt(timer0, &ultrasonic_ldr_tilt_isr, true);
    timerAlarmWrite(timer0, 20000, true);
    timerAlarmEnable(timer0);
    // ledF1OnOff = !ledF1OnOff;
    // digitalWrite(LED_F1, ledF1OnOff);
    
}




void ultrasonic_init() {
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


}

void ldr_init() {
  pinMode(LDR_PIN1, INPUT);
  pinMode(LDR_PIN2, INPUT);
}

















void setup(){
  tilt_init();
  ultrasonic_init();
  ldr_init();
  timer0_init();
  
  tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);

  // Buttons (unchanged) ...
  pinMode(BTN1, INPUT_PULLUP); pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP); pinMode(BTN4, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN1), isrBtn1, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN2), isrBtn2, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN3), isrBtn3, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN4), isrBtn4, FALLING);

  // ADC (unchanged) ...
  analogReadResolution(12);
  analogSetPinAttenuation(POT_PIN, ADC_11db);

  // Timer (unchanged) ...
  uiTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(uiTimer, &onTick, true);
  timerAlarmWrite(uiTimer, 20000, true);
  timerAlarmEnable(uiTimer);

  // ---- NEW: LED outputs ----
  pinMode(FALL_PIN, INPUT);  // use INPUT; add external pulldown if needed
  pinMode(LED_F1, OUTPUT);
  pinMode(LED_F2, OUTPUT);
  pinMode(LED_F3, OUTPUT);
  applyOutputs();                 // reflect initial f1On/f2On/f3On (all off now)

  // initial UI
  variable0_100 = readPotPercent();
  renderMainOnce();
}

// ---------- Loop ----------
void loop(){
  // consume edges atomically

  // for(int i = 0; i < 4; i ++){
  //   if(tilt_vals[i] == B11111111){
  //     renderTiltOnce();
  //   }
  // }
  
  if((tilt_vals[0]&tilt_vals[1]&tilt_vals[2]&tilt_vals[3] == 0)){
    ui = UI_MAIN;
  }
  if((tilt_vals[1]|tilt_vals[2]|tilt_vals[3]|tilt_vals[1]) == B11111111){
    ui = UI_ALERT;
    renderAlertOnce();
  }


  uint32_t edges;
  portENTER_CRITICAL(&mux);
  edges = buttonEdgeFlags; buttonEdgeFlags = 0;
  portEXIT_CRITICAL(&mux);

  uint32_t nowMs = millis();


  // --- FALL detection (polling with debounce + cooldown) ---
  if (ui != UI_ALERT) {
    // Respect cooldown: ignore sensor while cooling down
    bool inCooldown = (nowMs < fallCooldownUntilMs);

    if (!inCooldown) {
      int fallLevel = digitalRead(FALL_PIN);  // 1 = HIGH = triggered
      if (fallLevel == HIGH) {
        if (fallHighSinceMs == 0) fallHighSinceMs = nowMs;
        if (nowMs - fallHighSinceMs >= FALL_DEBOUNCE_MS) {
          // Enter alert state
          ui = UI_ALERT;
          renderAlertOnce();
        }
      } else {
        fallHighSinceMs = 0; // reset debounce if goes low
      }
    }
  }

  
  if (edges) {
    if ((edges & (1u<<0)) && digitalRead(BTN1)==LOW) { /* ok */ }
    if ((edges & (1u<<1)) && digitalRead(BTN2)==LOW) { /* ok */ }
    if ((edges & (1u<<2)) && digitalRead(BTN3)==LOW) { /* ok */ }
    if ((edges & (1u<<3)) && digitalRead(BTN4)==LOW) { /* ok */ }
    // --- NEW: handle alert acknowledgement first ---
    if (ui == UI_ALERT) {
      // Any button press = user says it's a false alarm → start cooldown
      fallCooldownUntilMs = millis() + FALL_COOLDOWN_MS;
      fallHighSinceMs = 0;                 // reset debounce window
      // Return to previous UI (choose where to go; Main is typical)
      ui = UI_MAIN;
      renderMainOnce();
    }
    else if (ui == UI_MAIN) {
        if (nowMs - lastUiChangeMs >= uiCooldownMs){
          lastUiChangeMs = nowMs;
          ui = UI_SETTINGS;
          // one-time draw (no fillScreen)
          renderSettingsOnce();
        }
      }
    else if (ui == UI_SETTINGS) {
      if (edges & (1u<<0)) { f1On = !f1On; applyOutputs(); updateToggleF1(); }
      if (edges & (1u<<1)) { f2On = !f2On; applyOutputs(); updateToggleF2(); }
      if (edges & (1u<<2)) { f3On = !f3On; applyOutputs(); updateToggleF3(); }
      if ((edges & (1u<<3)) && (nowMs - lastUiChangeMs >= uiCooldownMs)) {
        lastUiChangeMs = nowMs;
        ui = UI_MAIN;
        renderMainOnce();
      }
    }
  }

  // timer-driven ADC & tiny UI updates
  if (tickFlag){
    tickFlag = false;
    int pct = readPotPercent();
    if (pct != variable0_100){
      variable0_100 = pct;
      // update border only if color changed
      uint16_t c = borderColorFromVariable(variable0_100);
      if (c != lastBorderColor){ lastBorderColor = c; drawBorder(borderThickness, c); }
      // small-area text update (no flash)
      drawBattery(variable0_100);
    }
  }
  
}
