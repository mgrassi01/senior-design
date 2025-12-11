#include <Arduino.h>
#include <TFT_eSPI.h>

#include "pins.h"
#include "io.h"

TFT_eSPI tft;

// ---------- UI / SYSTEM STATE ----------
enum UiState {  UI_SETTINGS, UI_ALERT };
volatile UiState ui = UI_SETTINGS;   // DEFAULT SCREEN = MAIN

int borderThickness = 10;
volatile int variable0_100 = 100;
uint16_t lastBorderColor = 0;

// ---------- FEATURE TOGGLES ----------
// lightsOn (F1): Speaker (UI only, no hardware)
// audioOn (F2): Headlight (HEADLIGHT pin)
// hapticsOn (F3): ERM (ERM1 + ERM2 pins)
volatile bool lightsOn   = false;
volatile bool audioOn    = false;
volatile bool hapticsOn  = false;

// ---------- FALL DETECTION ----------
const uint32_t FALL_COOLDOWN_MS = 1UL * 10UL * 1000UL; // 10 seconds for testing
uint32_t fallCooldownUntilMs = 0;

const uint32_t FALL_DEBOUNCE_MS = 100;
uint32_t fallHighSinceMs = 0;

// ---------- BUTTON ISR FLAGS ----------
volatile uint32_t buttonEdgeFlags = 0;
volatile bool tickFlag = false;
volatile uint32_t lastEdgeUs[3] = {0,0,0};   // 3 buttons only
const uint32_t debounceUs = 120000;          // 80ms
uint32_t lastUiChangeMs = 0;
const uint32_t uiCooldownMs = 200;           // 200ms cooldown between screen switches

// ---------- TIMER ----------
hw_timer_t* uiTimer = nullptr;

// ---------- RECTANGLES FOR LAYOUT ----------
struct Rect { int x,y,w,h; };
Rect gF1, gAUDIO, gHAPTICS, gMain;

// ------------------------------------------------------
//                COLOR HELPERS
// ------------------------------------------------------
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

uint16_t borderColorFromVariable(int v){
  v = constrain(v,0,100);
  return hsv_to_565(120.0f*(v/100.0f),1.0f,1.0f);
}


// ------------------------------------------------------
//                   DRAW HELPERS
// ------------------------------------------------------
void drawBorder(int thickness, uint16_t color){
  int w=tft.width(), h=tft.height();
  for(int i=0;i<thickness;i++)
    tft.drawRect(i,i,w-2*i,h-2*i,color);
}

void clearInnerArea(int thickness){
  int w=tft.width(), h=tft.height();
  tft.fillRect(thickness, thickness, w-2*thickness, h-2*thickness, TFT_BLACK);
}

void drawToggleButton(int x,int y,int w,int h,const char* label,bool on){
  uint16_t fill = on ? TFT_DARKGREEN : TFT_RED;
  tft.fillRoundRect(x,y,w,h,6,fill);
  tft.drawRoundRect(x,y,w,h,6,TFT_LIGHTGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, fill);
  String s = String(label) + (on ? " ON" : " OFF");
  tft.drawString(s, x+w/2, y+h/2+3, 4);
}

void drawBattery(int pct){
  int w=tft.width(); 
  int x=borderThickness, y=borderThickness+2, h=30;
  int innerW=w-2*borderThickness;

  tft.fillRect(x,y,innerW,h,TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.drawString("Battery: " + String(pct) + "%", x+innerW/2, y+h/2, 4);
}


// ------------------------------------------------------
//                   MAIN SCREEN
// ------------------------------------------------------
void renderMainOnce(){
  uint16_t c=borderColorFromVariable(variable0_100); 
  lastBorderColor=c;
  drawBorder(borderThickness,c); 
  clearInnerArea(borderThickness);

  int w=tft.width(), h=tft.height();
  int bw=w*3/4, bh=100, bx=(w-bw)/2, by=(h-bh)/2;
  drawButton(bx,by,bw,bh,"Settings",TFT_WHITE,TFT_DARKGREY);
  drawBattery(variable0_100);
}


// Helper function for drawButton
void drawButton(int x, int y, int w, int h, const char* label, uint16_t outline, uint16_t fill) {
  tft.fillRoundRect(x, y, w, h, 6, fill);
  tft.drawRoundRect(x, y, w, h, 6, outline);

  const int fontNum = 4;
  const int yNudge  = 3;

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, fill);
  tft.drawString(label, x + w / 2, y + h / 2 + yNudge, fontNum);
}


// ------------------------------------------------------
//                   SETTINGS SCREEN
// ------------------------------------------------------
void renderSettingsOnce(){

  uint16_t c = borderColorFromVariable(variable0_100);
  lastBorderColor = c;

  drawBorder(borderThickness,c);
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


// ------------------------------------------------------
//                       ALERT SCREEN
// ------------------------------------------------------
void renderAlertOnce(){
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);

  int w=tft.width(), h=tft.height();
  tft.drawString("Walker has fallen!",            w/2, h/2 - 20, 4);
  tft.drawString("Press any button to dismiss",   w/2, h/2 + 20, 4);
}


// ------------------------------------------------------
//                TOGGLE UPDATE (partial redraw)
// ------------------------------------------------------
void updateToggleF1()    { drawToggleButton(gF1.x,     gF1.y,     gF1.w,     gF1.h,     "LIGHTS ",  lightsOn); }
void updateToggleAUDIO() { drawToggleButton(gAUDIO.x,  gAUDIO.y,  gAUDIO.w,  gAUDIO.h,  "AUDIO ",   audioOn); }
void updateToggleHAPTICS(){ drawToggleButton(gHAPTICS.x,gHAPTICS.y,gHAPTICS.w,gHAPTICS.h,"HAPTICS ", hapticsOn); }


// ------------------------------------------------------
//              APPLY HARDWARE OUTPUTS
// ------------------------------------------------------
void applyOutputs() {
  // lightsOn: UI only for now, no hardware
  
  // audioOn: Headlight control with pull-down when off
  if (audioOn) {
    pinMode(HEADLIGHT, OUTPUT);
    digitalWrite(HEADLIGHT, HIGH);
  } else {
    pinMode(HEADLIGHT, INPUT_PULLDOWN);
  }

  // hapticsOn: ERM motor control (both pins together)
  digitalWrite(ERM1, hapticsOn ? HIGH : LOW);
  digitalWrite(ERM2, hapticsOn ? HIGH : LOW);
}


// ------------------------------------------------------
//                   INTERRUPT HANDLING
// ------------------------------------------------------
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR handleBtnISR(int idx){
  uint32_t now = micros();
  if (now - lastEdgeUs[idx] < debounceUs) return;

  lastEdgeUs[idx] = now;

  portENTER_CRITICAL_ISR(&mux);
  buttonEdgeFlags |= (1u << idx);
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR isrBtn1(){ handleBtnISR(0); }
void IRAM_ATTR isrBtn2(){ handleBtnISR(1); }
void IRAM_ATTR isrBtn3(){ handleBtnISR(2); }
void IRAM_ATTR onTick(){ tickFlag = true; }


// ------------------------------------------------------
//                     ADC WRAPPER
// ------------------------------------------------------
int readPotPercent(){
  int raw = io_readPot();
  raw = constrain(raw, 0, 4095);

  static float potFilt = 0.0f;
  const float alpha = 0.15f;

  float norm = raw / 4095.0f;
  potFilt = potFilt*(1.0f - alpha) + norm*alpha;

  return (int)roundf(potFilt * 100.0f);
}


// ------------------------------------------------------
//             FALL DETECTION (helper)
// ------------------------------------------------------
void updateFallDetection(uint32_t nowMs) {
  if (ui == UI_ALERT) return;  // already in alert

  bool inCooldown = (nowMs < fallCooldownUntilMs);
  if (inCooldown) return;

  // Check tilt_state from external sensor code
  bool fallLogical = (tilt_state != 0);
  
  if (fallLogical) {
    if (fallHighSinceMs == 0)
      fallHighSinceMs = nowMs;

    if (nowMs - fallHighSinceMs >= FALL_DEBOUNCE_MS) {
      ui = UI_ALERT;
      renderAlertOnce();
    }
  } else {
    fallHighSinceMs = 0;
  }
}


// ------------------------------------------------------
//             BUTTON HANDLING (helper)
// ------------------------------------------------------
void handleButtons(uint32_t edges, uint32_t nowMs) {

  if (!edges) return;

  // ---- ALERT SCREEN ----
  if (ui == UI_ALERT) {
    // Any button press dismisses alert
    fallCooldownUntilMs = nowMs + FALL_COOLDOWN_MS;
    fallHighSinceMs = 0;
    ui = UI_SETTINGS;
    renderSettingsOnce();
    return;
  }

  // ---- MAIN SCREEN ----
  

  // ---- SETTINGS SCREEN ----
  if (ui == UI_SETTINGS) {

    // BTN1 → Lights toggle (UI only)
    if (edges & (1u << 0)) {
      lightsOn = !lightsOn;
      updateToggleF1();
      applyOutputs();
    }

    // BTN2 → Audio toggle (Headlight)
    if (edges & (1u << 1)) {
      audioOn = !audioOn;
      applyOutputs();
      updateToggleAUDIO();
    }

    // BTN3 → Haptics toggle (ERM)
    if (edges & (1u << 2)) {
      hapticsOn = !hapticsOn;
      applyOutputs();
      updateToggleHAPTICS();
    }

    return;
  }
}


// ------------------------------------------------------
//                UI TICK (battery/border)
// ------------------------------------------------------
void updateUiTick() {
  if (!tickFlag) return;
  tickFlag = false;

  int pct = readPotPercent();
  if (pct == variable0_100) return;

  variable0_100 = pct;

  uint16_t c = borderColorFromVariable(variable0_100);
  if (c != lastBorderColor) {
    lastBorderColor = c;
    drawBorder(borderThickness, c);
  }

  drawBattery(variable0_100);
}


// ------------------------------------------------------
//                        SETUP
// ------------------------------------------------------
void setup(){
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  io_init();

  // Ensure these are outputs even if io_init didn't
  pinMode(HEADLIGHT, OUTPUT);
  pinMode(ERM1, OUTPUT);
  pinMode(ERM2, OUTPUT);
  applyOutputs();   // Apply initial OFF states (all false initially)

  attachInterrupt(digitalPinToInterrupt(BUTTON1), isrBtn1, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2), isrBtn2, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON3), isrBtn3, FALLING);

  uiTimer = timerBegin(0,80,true);
  timerAttachInterrupt(uiTimer,&onTick,true);
  timerAlarmWrite(uiTimer,20000,true);
  timerAlarmEnable(uiTimer);

  variable0_100 = readPotPercent();
  renderMainOnce();
}


// ------------------------------------------------------
//                        LOOP
// ------------------------------------------------------
void loop() {

  // --- Read ISR edges atomically ---
  uint32_t edges;
  portENTER_CRITICAL(&mux);
  edges = buttonEdgeFlags;
  buttonEdgeFlags = 0;
  portEXIT_CRITICAL(&mux);

  uint32_t nowMs = millis();

  updateFallDetection(nowMs);
  handleButtons(edges, nowMs);
  updateUiTick();
}
