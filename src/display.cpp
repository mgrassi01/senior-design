#include <TFT_eSPI.h>
#include <Arduino.h>
#include "sensors.hpp"
#include "pins.hpp"

TFT_eSPI tft;




// ---------- ADC ----------
int adc_min_cal = 0, adc_max_cal = 4095;
float potFilt = 0.0f;
const float alpha = 0.15f;
// ---------- FALL ALERT (GPIO35) ----------
// enum UiState { UI_MAIN, UI_SETTINGS, UI_ALERT };   // add UI_ALERT
volatile UiState ui = UI_MAIN;           // update your existing declaration
//cancel button 
static uint32_t alertStartTimeMs = 0;
static bool cancelButtonPressed = false;
// Cooldown after user acknowledges (10 minutes)
const uint32_t FALL_COOLDOWN_MS = 1UL * 10UL * 1000UL; // 10 seconds 
uint32_t fallCooldownUntilMs = 0;

// Simple debounce: require sustained HIGH for this long to trigger
const uint32_t FALL_DEBOUNCE_MS = 100;
uint32_t fallHighSinceMs = 0;

// ---------- UI ----------
int borderThickness = 10;
volatile int variable0_100 = 100;
uint16_t lastBorderColor = 0;


// toggles
volatile bool lightsOn = false, audioOn = false, hapticsOn = false;

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
Rect gF1, gAUDIO, gHAPTICS, gMain;

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
  uint16_t fill = on ? TFT_DARKGREEN : TFT_RED;
  tft.fillRoundRect(x, y, w, h, 6, fill);
  tft.drawRoundRect(x, y, w, h, 6, TFT_LIGHTGREY); // yellow -> grey

  const int fontNum = 4;  // Bolder text
  const int yNudge  = 3;  // Center correction

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, fill); // black -> white
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
  tft.drawString("We have detected the walker has fallen", w/2, h/2 - 20, 4);
  tft.drawString("Click any button if false alarm",        w/2, h/2 + 20, 4);
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

  // 2 columns x 2 rows grid (F1,AUDIO on row1; HAPTICS, Main UI on row2)
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
  gAUDIO   = { x2, y1, colW, rowH };
  gHAPTICS   = { x1, y2, colW, rowH };
  gMain = { x2, y2, colW, rowH };

  // draw buttons (uses your centered text with y-nudge inside draw* functions)
  drawToggleButton(gF1.x, gF1.y, gF1.w, gF1.h, "LIGHTS ", lightsOn);
  drawToggleButton(gAUDIO.x, gAUDIO.y, gAUDIO.w, gAUDIO.h, "AUDIO ", audioOn);
  drawToggleButton(gHAPTICS.x, gHAPTICS.y, gHAPTICS.w, gHAPTICS.h, "HAPTICS ", hapticsOn);
  drawButton      (gMain.x, gMain.y, gMain.w, gMain.h, "Main UI", TFT_WHITE, TFT_DARKGREY);

  // battery strip (drawn last, stays under the border and above buttons)
  drawBattery(variable0_100);
}


// ---------- Partial redraw helpers (no whole-screen clears) ----------
void updateToggleF1(){ drawToggleButton(gF1.x,gF1.y,gF1.w,gF1.h,"LIGHTS ",lightsOn); }
void updateToggleAUDIO(){ drawToggleButton(gAUDIO.x,gAUDIO.y,gAUDIO.w,gAUDIO.h,"AUDIO ",audioOn); }
void updateToggleHAPTICS(){ drawToggleButton(gHAPTICS.x,gHAPTICS.y,gHAPTICS.w,gHAPTICS.h,"HAPTICS ",hapticsOn); }

void applyOutputs() {
  // Serial.println(lightsOn ? "F1 is on": "F1 is off");
  // Serial.println(audioOn ? "AUDIO is on": "AUDIO is off");
  // Serial.println(hapticsOn ? "HAPTICS is on": "HAPTICS is off");

  // digitalWrite(LED_F1, lightsOn ? HIGH : LOW);
  // digitalWrite(LED_AUDIO, audioOn ? HIGH : LOW);
  // digitalWrite(LED_HAPTICS, hapticsOn ? HIGH : LOW);
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
void IRAM_ATTR isrLIGHTS_BTN_PIN(){ handleBtnISR(0); }
void IRAM_ATTR isrAUDIO_BTN_PIN(){ handleBtnISR(1); }
void IRAM_ATTR isrHAPTICS_BTN_PIN(){ handleBtnISR(2); }
void IRAM_ATTR isrBtn4(){ handleBtnISR(3); }

void IRAM_ATTR onTick(){ tickFlag = true; } // timer ISR only sets a flag

// ---------- ADC read (in loop) ----------
int readPotPercent(){
  int raw = analogRead(ADC_BATTERY);
  raw = constrain(raw, adc_min_cal, adc_max_cal);
  float norm = (float)(raw - adc_min_cal) / (float)(adc_max_cal - adc_min_cal);
  norm = constrain(norm, 0.0f, 1.0f);
  potFilt = (1.0f - alpha)*potFilt + alpha*norm;
  return (int)roundf(potFilt*100.0f);
}

// ---------- Setup ----------
void display_setup(){
  tft.init(); 

  tft.setRotation(1); 

  tft.fillScreen(TFT_BLACK);


  // Buttons (unchanged) ...
  pinMode(LIGHTS_BTN_PIN, INPUT_PULLUP); pinMode(AUDIO_BTN_PIN, INPUT_PULLUP);
  pinMode(HAPTICS_BTN_PIN, INPUT_PULLUP); pinMode(BTN4, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LIGHTS_BTN_PIN), isrLIGHTS_BTN_PIN, FALLING);
  attachInterrupt(digitalPinToInterrupt(AUDIO_BTN_PIN), isrAUDIO_BTN_PIN, FALLING);
  attachInterrupt(digitalPinToInterrupt(HAPTICS_BTN_PIN), isrHAPTICS_BTN_PIN, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN4), isrBtn4, FALLING);

  // ADC (unchanged) ...
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_BATTERY, ADC_11db);

  // Timer (unchanged) ...
  uiTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(uiTimer, &onTick, true);
  timerAlarmWrite(uiTimer, 20000, true);
  timerAlarmEnable(uiTimer);

  // ---- NEW: LED outputs ----
  pinMode(FALL_PIN, INPUT);  // use INPUT; add external pulldown if needed
  // commented out bc we are using those LEDs for different functions
  // pinMode(LED_F1, OUTPUT);
  // pinMode(LED_AUDIO, OUTPUT);
  // pinMode(LED_HAPTICS, OUTPUT);
  applyOutputs();                 // reflect initial lightsOn/audioOn/hapticsOn (all off now)

  // initial UI
  variable0_100 = readPotPercent();
  renderMainOnce();
}


// ---------- Loop ----------
void display_loop(){
  // consume edges atomically
  uint32_t edges;
  portENTER_CRITICAL(&mux);
  edges = buttonEdgeFlags; buttonEdgeFlags = 0;
  portEXIT_CRITICAL(&mux);

  uint32_t nowMs = millis();

  // --- FALL detection (polling with debounce + cooldown) ---
  if (ui != UI_ALERT) {
    // Respect cooldown: ignore sensor while cooling down
    bool inCooldown = (nowMs < fallCooldownUntilMs);

    // if (!inCooldown) {
    //   int fallLevel = digitalRead(FALL_PIN);  // 1 = HIGH = triggered
    //   if (fallLevel == HIGH) {
    //     if (fallHighSinceMs == 0) fallHighSinceMs = nowMs;
    //     if (nowMs - fallHighSinceMs >= FALL_DEBOUNCE_MS) {
    //       // Enter alert state
    //       ui = UI_ALERT;
    //       renderAlertOnce();
    //     }
    //   } else {
    //     fallHighSinceMs = 0; // reset debounce if goes low
    //   }

    if(!inCooldown) {
      bool fallLogical = (tilt_state != 0);
      if(fallLogical) {
        if(fallHighSinceMs == 0) {
          fallHighSinceMs = nowMs;
          //Serial.println("Fall detected - debounce start");
        }
        if(nowMs - fallHighSinceMs >= FALL_DEBOUNCE_MS) {
          //Serial.println("Alert triggered after debounce");
          //ui = UI_ALERT;
          //renderAlertOnce();
          ui = UI_ALERT;
          alertStartTimeMs = nowMs;
          cancelButtonPressed = false;
          renderAlertOnce();
        }        
     
      }else{
        fallHighSinceMs = 0;
      }


    }
    }
  
  if (edges) {
    if ((edges & (1u<<0)) && digitalRead(LIGHTS_BTN_PIN)==LOW) { /* ok */ }
    if ((edges & (1u<<1)) && digitalRead(AUDIO_BTN_PIN)==LOW) { /* ok */ }
    if ((edges & (1u<<2)) && digitalRead(HAPTICS_BTN_PIN)==LOW) { /* ok */ }
    if ((edges & (1u<<3)) && digitalRead(BTN4)==LOW) { /* ok */ }
    // --- NEW: handle alert acknowledgement first ---
    if (ui == UI_ALERT) {
      // Any button press = user says it's a false alarm → start cooldown
      //fallCooldownUntilMs = millis() + FALL_COOLDOWN_MS;
      //fallHighSinceMs = 0;                 // reset debounce window
      // Return to previous UI (choose where to go; Main is typical)
      //ui = UI_MAIN;
      //renderMainOnce();
      uint32_t alertDuration = nowMs - alertStartTimeMs;
      if(alertDuration < 30000){
        cancelButtonPressed = true;
      }
      fallCooldownUntilMs = millis() + FALL_COOLDOWN_MS;
      fallHighSinceMs = 0;
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
      if (edges & (1u<<0)) { lightsOn = !lightsOn; applyOutputs(); updateToggleF1(); }
      if (edges & (1u<<1)) { audioOn = !audioOn; applyOutputs(); updateToggleAUDIO(); }
      if (edges & (1u<<2)) { hapticsOn = !hapticsOn; applyOutputs(); updateToggleHAPTICS(); }
      if ((edges & (1u<<3)) && (nowMs - lastUiChangeMs >= uiCooldownMs)) {
        lastUiChangeMs = nowMs;
        ui = UI_MAIN;
        renderMainOnce();
      }
    }
  }
  // timer-driven ADC & tiny UI updates
  if (tickFlag){

    // check to see if the timer > 10 seconds, if is, get the current state of the till set the appropriate alert 
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


bool display_in_alert(){
  return (ui == UI_ALERT);
} 

bool cancel_pressed(){
  return cancelButtonPressed;
}
