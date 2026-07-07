#include <Servo.h>

// --- HARDVÉROVÁ KONFIGURÁCIA ---
const int pin_tl = A0; // Top Left LDR
const int pin_tr = A1; // Top Right LDR
const int pin_bl = A2; // Bottom Left LDR
const int pin_br = A3; // Bottom Right LDR

Servo servo_h; // Horizontálne servo (Pin 9)
Servo servo_v; // Vertikálne servo (Pin 10)

// --- LIMITY A BEZPEČNOSŤ (Mechanické dorazy) ---
const int limit_h_min = 10;
const int limit_h_max = 170;
const int limit_v_min = 20;
const int limit_v_max = 160;

const int home_h = 90; // Domovská pozícia X
const int home_v = 90; // Domovská pozícia Y

// --- PREMENNÉ STAVU A POZÍCIE ---
int pos_h = 90; 
int pos_v = 90;
bool tracking_active = true; // Stavový automat: true = AUTO, false = PAUSE (Manuálny režim)

// --- ALGORITMICKÁ OPTIMALIZÁCIA ---
const int tolerance = 20;               // Hysterézna tolerancia proti kmitaniu
const unsigned long tracking_interval = 25; // Perióda spracovania trackeru v ms (nahrádza delay)
unsigned long last_tracking_time = 0;   // Časová značka pre neblokujúci časovač

// Digitálny filter (EMA): Vyhladzovanie šumu z LDR
// f_val = (alpha * current_read) + ((1 - alpha) * last_f_val)
float f_tl = 0.0, f_tr = 0.0, f_bl = 0.0, f_br = 0.0; 
const float alpha = 0.15; // Koeficient filtra (0.05 až 0.30). Čím nižší, tým silnejší filter.

void setup() {
  // Zvýšenie rýchlosti zbernice na 115200 baud pre minimálnu latenciu prenosu príkazov
  Serial.begin(115200); 
  
  servo_h.attach(9);
  servo_v.attach(10);
  
  // Prvotná inicializácia pozície
  servo_h.write(pos_h);
  servo_v.write(pos_v);

  // Pred-načítanie hodnôt do filtra, aby sme nezačínali od nuly
  f_tl = analogRead(pin_tl);
  f_tr = analogRead(pin_tr);
  f_bl = analogRead(pin_bl);
  f_br = analogRead(pin_br);
  
  printHelp();
}

void loop() {
  // 1. Krok: Okamžitá kontrola a spracovanie prichádzajúcich dát zo Serial linky (Non-blocking)
  if (Serial.available() > 0) {
    processSerialCommands();
  }

  // 2. Krok: Vyhodnotenie Sun Trackera na základe hardvérového časovača millis()
  if (tracking_active) {
    unsigned long current_time = millis();
    if (current_time - last_tracking_time >= tracking_interval) {
      last_tracking_time = current_time;
      runSunTrackerAlgorithm();
    }
  }
}

// --- FUNKCIA PRE AUTOMATICKÉ SLEDOVANIE SLNKA ---
void runSunTrackerAlgorithm() {
  // Aplikácia low-pass filtra (EMA) na potlačenie vysokofrekvenčného šumu
  f_tl = (alpha * analogRead(pin_tl)) + ((1.0 - alpha) * f_tl);
  f_tr = (alpha * analogRead(pin_tr)) + ((1.0 - alpha) * f_tr);
  f_bl = (alpha * analogRead(pin_bl)) + ((1.0 - alpha) * f_bl);
  f_br = (alpha * analogRead(pin_br)) + ((1.0 - alpha) * f_br);

  // Konverzia filtrovaných dát na celé čísla pre maticu diferencií
  int avg_top = ((int)f_tl + (int)f_tr) / 2;
  int avg_bottom = ((int)f_bl + (int)f_br) / 2;
  int avg_left = ((int)f_tl + (int)f_bl) / 2;
  int avg_right = ((int)f_tr + (int)f_br) / 2;

  int diff_v = avg_top - avg_bottom;
  int diff_h = avg_left - avg_right;

  // Vertikálne riadenie (Y-os)
  if (abs(diff_v) > tolerance) {
    if (diff_v > 0) pos_v--; else pos_v++;
    pos_v = constrain(pos_v, limit_v_min, limit_v_max);
    servo_v.write(pos_v);
  }

  // Horizontálne riadenie (X-os)
  if (abs(diff_h) > tolerance) {
    if (diff_h > 0) pos_h--; else pos_h++;
    pos_h = constrain(pos_h, limit_h_min, limit_h_max);
    servo_h.write(pos_h);
  }
}

// --- PARSER SÉRIOVÝCH PRÍKAZOV ---
void processSerialCommands() {
  String input = Serial.readStringUntil('\n');
  input.trim(); // Odstránenie neviditeľných znakov ako \r (Carriage Return) a medzier na koncoch

  // Príkaz: GetPos
  if (input.equalsIgnoreCase("GetPos")) {
    Serial.print("ACK GetPos H_Pos:");
    Serial.print(pos_h);
    Serial.print(" V_Pos:");
    Serial.print(pos_v);
    Serial.print(" State:");
    Serial.println(tracking_active ? "AUTO_TRACKING" : "PAUSED_MANUAL");
  }
  
  // Príkaz: Start
  else if (input.equalsIgnoreCase("Start")) {
    tracking_active = true;
    Serial.println("ACK Start - Automatic tracking enabled.");
  }
  
  // Príkaz: Pause
  else if (input.equalsIgnoreCase("Pause")) {
    tracking_active = false;
    Serial.println("ACK Pause - Automatic tracking suspended. Manual mode active.");
  }
  
  // Príkaz: MoveHome
  else if (input.equalsIgnoreCase("MoveHome")) {
    pos_h = home_h;
    pos_v = home_v;
    servo_h.write(pos_h);
    servo_v.write(pos_v);
    Serial.println("ACK MoveHome - Safe center position executed.");
  }
  
  // Príkaz: MoveX {hodnota}
  else if (input.startsWith("MoveX ") || input.startsWith("moveX ")) {
    int requested_val = input.substring(6).toInt();
    int target_val = constrain(requested_val, limit_h_min, limit_h_max);
    
    // Rozmýšľanie dopredu: Ak užívateľ zadá manuálny posun, automaticky pozastavíme tracking,
    // aby sa algoritmus "nebil" s manuálnym nastavením.
    tracking_active = false; 
    
    pos_h = target_val;
    servo_h.write(pos_h);
    
    Serial.print("ACK MoveX: ");
    Serial.print(target_val);
    if (requested_val != target_val) {
      Serial.print(" (Warning: Constrained from ");
      Serial.print(requested_val);
      Serial.print(")");
    }
    Serial.println(" - Auto-tracking PAUSED.");
  }
  
  // Príkaz: MoveY {hodnota}
  else if (input.startsWith("MoveY ") || input.startsWith("moveY ")) {
    int requested_val = input.substring(6).toInt();
    int target_val = constrain(requested_val, limit_v_min, limit_v_max);
    
    tracking_active = false; // Auto-pause pre bezpečnosť
    
    pos_v = target_val;
    servo_v.write(pos_v);
    
    Serial.print("ACK MoveY: ");
    Serial.print(target_val);
    if (requested_val != target_val) {
      Serial.print(" (Warning: Constrained from ");
      Serial.print(requested_val);
      Serial.print(")");
    }
    Serial.println(" - Auto-tracking PAUSED.");
  }
  
  // Neznámy príkaz (Error handling)
  else {
    Serial.print("ERR Unknown command: '");
    Serial.print(input);
    Serial.println("'. Type commands precisely.");
  }
}

// Informačné menu pri štarte systému
void printHelp() {
  Serial.println(F("\n=============================================="));
  Serial.println(F("       ADVANCED DUAL-AXIS SUN TRACKER v3.0   "));
  Serial.println(F("=============================================="));
  Serial.println(F("Available Serial Commands (Case-Insensitive):"));
  Serial.println(F("  GetPos       - Returns current X/Y angles and mode."));
  Serial.println(F("  Start        - Resumes automatic LDR sun tracking."));
  Serial.println(F("  Pause        - Halts auto-tracking for manual control."));
  Serial.println(F("  MoveHome     - Sends both servos to 90 degrees."));
  Serial.println(F("  MoveX <deg>  - Sets X servo directly (Auto-pauses tracking)."));
  Serial.println(F("  MoveY <deg>  - Sets Y servo directly (Auto-pauses tracking)."));
  Serial.println(F("=============================================="));
}
