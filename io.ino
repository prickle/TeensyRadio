unsigned long io_timer = millis();

//Input/output routines

#define IO_DELAY  20

//IO Pin declarations
#define ANALOG_VBAT A3
#define ANALOG_CBAT A8  //22
#define ANALOG_FFRW A10
#define ANALOG_AMFM A11
#define ANALOG_TUNE A2
#define DIGITAL_STMO 30

//prepare IO pin assignments
void io_setup() {
  // Backlight
  pinMode( TFT_BL, OUTPUT );
  digitalWrite( TFT_BL, LOW );  //off initially
  // Analog/digital inputs
  pinMode(ANALOG_VBAT, INPUT);
  pinMode(ANALOG_CBAT, INPUT);
  pinMode(ANALOG_FFRW, INPUT);
  pinMode(ANALOG_AMFM, INPUT);
  pinMode(ANALOG_TUNE, INPUT);
  pinMode(DIGITAL_STMO, INPUT);
  analogReadResolution(12);
}


// voltage charge
//2634 - 12.54v
// voltage discharge
//2405 - 12.29v

// current
//22 - charge 271mA
//2051 - no current flowing
//-10,-13 - discharge

//1999 full charge


// am/fm/dub/tape
//2000 - am
//1000 - fm
//500 - dub
//330 - tape
//24 - none

// FF/REW
//4094 - open
//1320 - rew
//1877 - ff
//955 - both

// tune
// 3720 - 10

float filtered_voltage = 2500;
float filtered_current = 2050;

float smoothed_tune = -1;
float filtered_tune = 0;

int current_fm_tune = -1;
int current_am_tune = -1;

int old_fm_tune = -1;
int old_am_tune = -1;

#define BATT_MAX  12.6
#define BATT_MIN  9.0
#define BATT_RANGE (BATT_MAX-BATT_MIN)

float battVoltage = 0;

uint8_t oldFunction = FUNC_NONE;
uint8_t newFunction = FUNC_NONE;
uint8_t functionDebounce = 0;

void io_handle() {
  static uint8_t oldBattPercent = 0;
  static uint8_t oldBattState = BATT_DISCHARGE;
  if (disconnected) return;
  unsigned long now = millis();
  if (now > io_timer) {
    io_timer = now + IO_DELAY;
    int raw_voltage = analogRead(ANALOG_VBAT);
    int raw_current = analogRead(ANALOG_CBAT);
    int raw_ffrw = analogRead(ANALOG_FFRW);
    int raw_amfmdubtape = analogRead(ANALOG_AMFM);
    int raw_tune = analogRead(ANALOG_TUNE);

    filtered_voltage = filtered_voltage * 0.99 + (float)raw_voltage * 0.01;
    filtered_current = filtered_current * 0.9 + (float)raw_current * 0.1;    
    
    float volt_div = 209.186;//210.048;                         //Charge path
    if (filtered_current > 2045) {
      battState = BATT_CHARGE;
      if (filtered_voltage > 2640) filtered_voltage = 2640; 
      battVoltage = filtered_voltage / volt_div;
      battPercent = (int)(((battVoltage - BATT_MIN) / BATT_RANGE) * 101);
    }
    else if (filtered_current < 2015) {
      volt_div = 188.744;  //Discharge path
      battVoltage = filtered_voltage / volt_div;
      battState = BATT_DISCHARGE;
      battPercent = (int)(((battVoltage - BATT_MIN) / BATT_RANGE) * 101);
    } else {
      battVoltage = filtered_voltage / volt_div;
      battState = BATT_FULL;
      battPercent = 100;
    }
    if (battPercent > 100) battPercent = 100;
    
    if (oldBattState != battState || oldBattPercent != battPercent) {
      oldBattState = battState;
      oldBattPercent = battPercent;
      updateBatteryDisplay();
    }

  /*char s[25] = {0};
  if (testLbl2) {
    sprintf(s, "%d, %2.2fv, %dmA", (int)filtered_voltage, (filtered_voltage / volt_div), (int)((2030 - filtered_current) * 11.7));
    lv_label_set_text(testLbl2, s);      
  }*/

    //Tuning dial
    if (smoothed_tune < 0) smoothed_tune = raw_tune;
    else smoothed_tune = smoothed_tune * 0.9 + (float)raw_tune * 0.1;
    
    //FM frequency
    int fm_raw = (float)smoothed_tune * -0.05196 + 1081;
    if (current_fm_tune < 0) current_fm_tune = fm_raw;
    else if (fm_raw < current_fm_tune - 1) current_fm_tune = fm_raw + 1;
    else if (fm_raw > current_fm_tune + 1) current_fm_tune = fm_raw - 1;
    if (old_fm_tune >= 0 && old_fm_tune != current_fm_tune) fmFreqChanged(current_fm_tune); 
    old_fm_tune = current_fm_tune;
/*
    //AM frequency
    int am_raw = (int)(((float)tuningDial * -0.2811 + 1347) / 9) * 9;
    if (current_am_tune < 0) current_am_tune = am_raw;
    else if (am_raw < current_am_tune - 9) current_am_tune = am_raw + 9;
    else if (am_raw > current_am_tune + 9) current_am_tune = am_raw - 9;
    if (old_am_tune >= 0 && old_am_tune != current_am_tune) amFreqChanged(current_am_tune); 
    old_am_tune = current_am_tune;
*/
/*
    
    if (tuningDial == DIAL_START_DELAY) smoothed_tune = filtered_tune = raw_tune; 
    if (raw_tune < smoothed_tune - 6) smoothed_tune = raw_tune + 6;
    if (raw_tune > smoothed_tune + 6) smoothed_tune = raw_tune - 6;
    if (smoothed_tune < filtered_tune - 1) filtered_tune = smoothed_tune + 1;
    if (smoothed_tune > filtered_tune + 1) filtered_tune = smoothed_tune - 1;
    if (tuningDial != filtered_tune) {
      if (tuningDial < 0) tuningDial++;
      else {
        tuningDial = filtered_tune;
        updateTuningDisplay();
      }
    }
*/

    static bool oldFF, oldRW = oldFF = false;
    //Fast forward and Rewind buttons : idle = 4073, ff = 1833, rew = 1274, both = 910
    if (raw_ffrw < 1092) { buttonFF = true; buttonRW = true; }
    else if (raw_ffrw < 1553) { buttonFF = false; buttonRW = true; }
    else if (raw_ffrw < 2953)  { buttonFF = true; buttonRW = false; }
    else { buttonFF = false; buttonRW = false; }
    if (oldFF != buttonFF) {
      oldFF = buttonFF;
      dabFastforward();
    }
    if (oldRW != buttonRW) {
      oldRW = buttonRW;
      dabRewind();
    }
    
    buttonDUB = false;
    tapeActive = false;
    //AM, FM, Dubbing and Tape functions : 
    //tp = 0,
    //132
    //tp+tapeplay = 265,
    //363
    //tp+dub = 461
    //588
    //tp+tapeplay+dub = 716,
    //835
    //fm = 954,
    //1064
    //fm+tapeplay = 1174,
    //1211
    //fm+dub = 1248,
    //1348
    //fm+tapeplay+dub = 1448,
    //1704
    //am = 1960,
    //2030
    //am+dub = 2100,
    //2107
    //am+tapeplay = 2114,
    //2177
    //am+tapeplay+dub = 2240,

    if (raw_amfmdubtape < 132) currentFunction = FUNC_TA;
    else if (raw_amfmdubtape < 363) currentFunction = FUNC_PB;
    else if (raw_amfmdubtape < 588) { currentFunction = FUNC_TA; buttonDUB = true;}
    else if (raw_amfmdubtape < 835) { currentFunction = FUNC_TA; tapeActive = true; buttonDUB = true;}
    else if (raw_amfmdubtape < 1064) currentFunction = FUNC_FM;
    else if (raw_amfmdubtape < 1211) { currentFunction = FUNC_FM; tapeActive = true;}
    else if (raw_amfmdubtape < 1348) { currentFunction = FUNC_TA; buttonDUB = true;}
    else if (raw_amfmdubtape < 1704) { currentFunction = FUNC_FM; tapeActive = true; buttonDUB = true;}
    else if (raw_amfmdubtape < 2030) currentFunction = FUNC_AM;
    else if (raw_amfmdubtape < 2107) { currentFunction = FUNC_AM; buttonDUB = true;}
    else if (raw_amfmdubtape < 2177) { currentFunction = FUNC_AM; tapeActive = true;}
    else { currentFunction = FUNC_AM; tapeActive = true; buttonDUB = true;}
    
    if (currentFunction != oldFunction) {
      if (currentFunction != newFunction) {
        newFunction = currentFunction;
        functionDebounce = 0;
      } else if (functionDebounce++ > 5) { //100ms
        oldFunction = currentFunction;
        setRadioMode();
      }
    }

    /*if (buttonFF) Serial.print("[FF] ");
    if (buttonRW) Serial.print("[REW] ");
    if (buttonDUB) Serial.print("[DUB] ");
    if (tapeActive) Serial.print("[TAPE] ");
    Serial.print(", Func:");
    if (currentFunction == FUNC_TA) Serial.print("Tape/Off");
    if (currentFunction == FUNC_FM) Serial.print("FM");
    if (currentFunction == FUNC_AM) Serial.print("AM");

    Serial.print(", ");
    Serial.print(raw_ffrw);
    Serial.print(", ");
    Serial.println(raw_amfmdubtape);
    //Serial.println(filtered_tune);
    //Serial.println((1 - ((float)(filtered_tune - 10) / 3710.0)) * 20.0 + 88.0);
    //Serial.print(", ");
    //Serial.println(digitalRead(DIGITAL_STMO));   

    */
  
/*  char s[25] = {0};
  if (progTextLbl) {
    sprintf(s, "%d, %d", raw_ffrw, raw_amfmdubtape);// + Serial?40:0);
    lv_label_set_text(progTextLbl, s);      
  }
*/
  
  }
}

void updateBatteryDisplay() {
  char s[25] = {0};
  if (battState == BATT_CHARGE || battState == BATT_FULL || battPercent == 100) {
    sprintf(s, " " LV_SYMBOL_CHARGE " %d%%", battPercent);
  } else {
    if (battPercent < 20) 
      sprintf(s, LV_SYMBOL_BATTERY_EMPTY " %d%%", battPercent);    
    else if (battPercent < 40) 
      sprintf(s, LV_SYMBOL_BATTERY_1 " %d%%", battPercent);    
    else if (battPercent < 60) 
      sprintf(s, LV_SYMBOL_BATTERY_2 " %d%%", battPercent);    
    else if (battPercent < 80) 
      sprintf(s, LV_SYMBOL_BATTERY_3 " %d%%", battPercent);    
    else 
      sprintf(s, LV_SYMBOL_BATTERY_FULL " %d%%", battPercent);    
  }
  setBattChgLbl(s);  
}

void fmFreqChanged(int fmFreq) {
  if (currentFunction == FUNC_TA) dabTuningAction(fmFreq);
  //if (currentFunction == FUNC_FM) {
  //  showFmFreq(fmFreq);
  //}
}
/*void showFmFreq(int fmFreq) {
  char str[10];
  sprintf(str, "%.1f", (float)fmFreq / 10);
  if (analogFreqLbl) lv_label_set_text(analogFreqLbl, str);          
}
*/

void amFreqChanged(int amFreq) {
  //if (currentFunction == FUNC_AM) {
  //  showAmFreq(amFreq);
  //}
}
/*void showAmFreq(int amFreq) {
  char str[10];
  sprintf(str, "%d", amFreq);
  if (analogFreqLbl) lv_label_set_text(analogFreqLbl, str);        
}
*/
