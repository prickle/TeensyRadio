#define DAB_LINEIN1  -1
#define DAB_LINEIN2  -2
#define DAB_SCAN     -3
#define DAB_STOP     -4
#define DAB_NONE     -5

long dabFrequency = DAB_NONE;

//Set up callbacks and allocate MOT
void prepareDab() {
  dab.SetStatusChangeCallback(dabStatusCallback);
  dab.SetNameChangeCallback(dabNameCallback);
  dab.SetTextChangeCallback(dabTextCallback);
  dab.SetFrequencyChangeCallback(dabFreqCallback);
  dab.SetStrengthChangeCallback(dabStrengthCallback);
  dab.SetStereoChangeCallback(dabStereoCallback);
  dab.SetUserAppChangeCallback(dabUserAppCallback);
  dab.SetTimeChangeCallback(dabTimeCallback);
  dab.SetIdleCallback(dabIdleCallback);
  //Prepare MOT slideshow
  dab.MotBegin();
  dab.SetMotImageCallback(dabImageCallback);
  dab.SetMotProgressCallback(dabProgressCallback);
}

//(re)init the DAB module
void initDab() {
  //Prepare MOT slideshow
  dab.MotReset();
  dabFrequency = DAB_STOP;
  //dab.StopStream();
}

//Load station list, find last station and begin
void startDab() {
  if (progNameLbl) lv_label_set_text(progNameLbl, "Start DAB Radio..");
  dabFrequency = readDabStations();
  kickstartDab();
}

//Set frequency display and start
void startFM() {
  dabFrequency = settings.dabFM;
  serial.print("> Starting DAB FM freq:");
  serial.println(dabFrequency);
  updateFMDisplay(dabFrequency / 100);
  kickstartDab();
}

//Start the line in bypass modes
void startVSbypass(int line) {
  dabFrequency = line;
  kickstartDab();
}

//Start here - load settings to DAB and switch DAB mode 
void kickstartDab() {
  if (!DABFound) DABFound = dab.begin();      //try to resolve issues..
  if (DABFound) {  
    dab.SetVolume(settings.dabVolume);
    setDabEQ(settings.dabEQ);
    updateUrlEditText();
    if (currentDabStatus == DAB_STATUS_STOP)
      dabStatusCallback(currentDabStatus);
  } else errorContinue("DAB+ Radio Module not responding!");   
}

//DAB channel scan - init a software reset callback
void startDabScan() {
  if (dabFrequency == DAB_SCAN) return;
  if (scanSpinner) lv_obj_set_hidden(scanSpinner, false);
  dab.reset(dabScanCallback);  
  clearStations();
  dabFrequency = DAB_SCAN;
}

//Try to restore prior mode
void restartDab() {
  if (dabFrequency == DAB_LINEIN1 || dabFrequency == DAB_LINEIN2) 
    startVSbypass(dabFrequency);
  else if (dabFrequency >= 10000) startFM();
  else startDab();  
}

//FM signal search up or down
bool dabFMSearching = false;
void dabFMSearch(uint8_t dir) {
  if (dabFMSearching) return;
  dab.FMSearch(dir);
  dabFMSearching = true;
}

//Keep animations smooth by updating screen in the DAB's delay loops 
void dabIdleCallback() {
  //if (!ScreenSaverActive) lv_refr_now(display);
}

//Pop, rock, classical, that kind of thing
// doesn't seem to work anyway..
void setDabEQ(uint8_t eq) {
  if (eq == 0) dab.SetEQ(false, 0);
  else dab.SetEQ(true, eq - 1);
}

//Channel scan reset init done, begin searching
// only scanning aussie channels..
void dabScanCallback(uint8_t ver1, uint16_t ver2) {
  serial.printf("> KeySight DAB+ v%d.%d\r\n", ver1, ver2);
  dab.SetNotification(127); //All of them
  if (currentDabStatus == DAB_STATUS_STOP) dab.AutoSearch(0, 40); 
}

//Read dab stations into station list
long readDabStations() {
  char programName[35] = {0};
  long prevStation = DAB_STOP;
  if (progNowLbl) lv_label_set_text(progNowLbl, "Reading station list..");
  clearStations();
  dab.SetSorter(DAB_SORTER_NAME);     //Set the sorter before reading names
  if (dab.TotalProgram > 0) {
    serial.printf("> Reading %d DAB stations.\r\n", dab.TotalProgram);
    for (int i = 0; i < dab.TotalProgram - 1; i++) {
      if (dab.GetProgramName(STREAM_MODE_DAB, i, DAB_NAMEMODE_LONG, programName, 35)) {
        //serial.printf("[%d]\t%s\r\n", i, programName);
        lv_obj_t * list_btn = addStationButton(programName, dabStationAction, &i, sizeof(int));
        if (strcasecmp(programName, settings.dabChannel) == 0) {
          serial.println("> Previous station found.");
          listSetSelect(list_btn);
          prevStation = i;        
        }  
      }
    }
  } else {
    serial.println("> No stations in DAB+ database!");
    if (progTextLbl) lv_label_set_text(progTextLbl, LV_SYMBOL_WARNING " No Stations! Go to settings and do a DAB Scan.");
  }
  if (progNowLbl) lv_label_set_text(progNowLbl, "");  
  return prevStation;
}

//Station list item selected
void dabStationAction(lv_event_t * event) {
  if (!dabStationList) return;
  lv_obj_t * obj = lv_event_get_target(event);
  listSetSelect(obj);
  serial.printf("Clicked: %s\n", lv_list_get_btn_text(dabStationList, obj));
  if (tabView) lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_ON);
  int i = *(int*)lv_obj_get_user_data(obj);
  strncpy(settings.dabChannel, lv_list_get_btn_text(dabStationList, obj), 34);
  settings.dabChannel[34] = '\0';
  writeSettings();
  dabPlay(i);
}

//FM tuning dial movement
#define DAB_DIAL_DELAY  500
int newDialFreq = 0;
//Fired when tuning dial changes
unsigned long dabDialTimer = 0;
void dabTuningAction(int freq) {
  if (currentFunction == FUNC_TA && settings.mode == MODE_FM) {
    dabDialTimer = millis() + DAB_DIAL_DELAY;
    updateFMDisplay(freq);
    newDialFreq = freq;
  }
}

void dabRewind() {
  if (currentFunction == FUNC_TA && settings.mode == MODE_FM) dabFMSearch(0);
}

void dabFastforward() {
  if (currentFunction == FUNC_TA && settings.mode == MODE_FM) dabFMSearch(1);
}


//determine which stereo/mono/slideshow should be top line icon
bool stmoSlideshow = false;
void updateSTMOLabel(uint8_t mode) {
  if (mode == STMO_MONO) {
    setStmoLbl("MO");        
    stmoSlideshow = false;
  } else if (mode == STMO_STEREO) {
    if (!stmoSlideshow)
      setStmoLbl("");        
  } else if (mode == STMO_SLIDE) {
    setStmoLbl(LV_SYMBOL_IMAGE);
    stmoSlideshow = true;
  } else {
    setStmoLbl("");        
  }
}

//FM tuning dial movement ceased, start reception on new frequency
// Called from main loop
void updateTuning() {
  if (currentFunction == FUNC_TA && dabDialTimer && dabDialTimer < millis()) {
    dabDialTimer = 0;
    dabFrequency = newDialFreq * 100;
    settings.dabFM = dabFrequency;
    writeSettings();
    dab.PlayStream(STREAM_MODE_FM, dabFrequency);    
    updateSTMOLabel(STMO_MONO);
  }
}

//Compose Digital FM frequency display
void updateFMDisplay(int freq) {
  char str[10];
  if (freq < 1000) sprintf(str, " %.1f", (float)freq / 10);
  else sprintf(str, "%.1f", (float)freq / 10);
  if (freq < 1000) lv_obj_set_pos(fmFreqLbl, 9, 22);  
  else lv_obj_set_pos(fmFreqLbl, -10, 22);
  if (fmFreqLbl) lv_label_set_text(fmFreqLbl, str);      
  if (progNowLbl) lv_label_set_text(progNowLbl, "");      
  if (progTextLbl) lv_label_set_text(progTextLbl, "");      
}

//DAB status change callback
bool wasTuning = false;
void dabStatusCallback(uint8_t status) {
  currentDabStatus = status;
  fmStationName[0] = '\0';
  if (status == DAB_STATUS_PLAYING) {
    if (settings.mode == MODE_FM) {
      if (dabFMSearching) {
        dabFrequency = dab.GetPlayIndex();
        updateFMDisplay(dabFrequency / 100);
        settings.dabFM = dabFrequency;
        writeSettings();
      }
    }    
    if (progNameLbl && dabFrequency != DAB_LINEIN1 && dabFrequency != DAB_LINEIN2 && currentFunction == FUNC_TA) {
      lv_label_set_text(progNameLbl, LV_SYMBOL_PLAY " Playing");
      updateUrlEditText();
    }
    if (progTextLbl) lv_label_set_text(progTextLbl, ""); //remove "station not found"
    dabFMSearching = false;
    wasTuning = false;
    if (dabFrequency != DAB_LINEIN1 && dabFrequency != DAB_LINEIN2) serial.println("> DAB Playing");
  }
  else if (status == DAB_STATUS_SEARCHING) {
    if (dabScanLabel && settings.mode == MODE_DAB) lv_label_set_text(dabScanLabel, LV_SYMBOL_WIFI " Searching..");
    if (progNameLbl) lv_label_set_text(progNameLbl, LV_SYMBOL_WIFI " Searching..");
    wasTuning = false;
    serial.println("> DAB Searching");
  }
  else if (status == DAB_STATUS_TUNING) {
    if (dabFrequency == DAB_SCAN) {
      char buf[30];
      sprintf(buf, "%d Stations Found.", (int)dab.GetTotalProgram());
      if (dabScanLabel) lv_label_set_text(dabScanLabel, buf);
      if (scanSpinner) lv_obj_set_hidden(scanSpinner, true);
      dabStop();
      restartDab();
    }
    if (!wasTuning) {
      char str[40];
      sprintf(str, LV_SYMBOL_WIFI " Tuning %s", settings.dabChannel);
      if (progNameLbl && currentFunction == FUNC_TA) lv_label_set_text(progNameLbl, str);
      setSigStrengthLbl(LV_SYMBOL_WIFI " 0%");
      serial.println("> DAB Tuning");
    }
    wasTuning = true;
  }
  else if (status == DAB_STATUS_STOP) {
    if (dabFrequency > 10000) {
      dab.PlayStream(STREAM_MODE_FM, dabFrequency);
      updateSTMOLabel(STMO_MONO);
    }
    else if (dabFrequency >= 0) {
      dab.PlayStream(STREAM_MODE_DAB, dabFrequency);
      if (wasTuning) {
        if (progTextLbl && currentFunction == FUNC_TA) 
          lv_label_set_text(progTextLbl, LV_SYMBOL_WARNING " Station not found!");
      }
      updateSTMOLabel(STMO_MONO);
    }
    else if (dabFrequency == DAB_LINEIN1) {
      serial.print("> DAB LINE1 Passthrough now: ");
      if (dab.PlayStream(STREAM_MODE_LINE1, 0)) serial.println("OK");
      else serial.println("Error!");
    }
    else if (dabFrequency == DAB_LINEIN2) {
      serial.print("> DAB LINE2 Passthrough now: ");
      if (dab.PlayStream(STREAM_MODE_LINE2, 0)) serial.println("OK");
      else serial.println("Error!");
    }
    else if (dabFrequency == DAB_SCAN) {
      serial.print("> DAB Channel Scan now: ");
      if (dab.AutoSearch(0, 40)) {
        if (dabScanLabel) lv_label_set_text(dabScanLabel, "Scan commences..");
        serial.println("OK");
      }
      else {
        if (dabScanLabel) lv_label_set_text(dabScanLabel, "Can't start scan!");
        serial.println("Error!");
      }
    }
    else {
      serial.println("> DAB Stop");
      if (progNameLbl && dabFrequency != DAB_LINEIN1 && dabFrequency != DAB_LINEIN2 && currentFunction == FUNC_TA) 
        lv_label_set_text(progNameLbl, LV_SYMBOL_STOP " Stopped");
    }
  }
  else {
    serial.print("> DAB Status unknown! : ");
    serial.println(status);
  }
}

//Thumbnail image
LV_IMG_DECLARE(dabLogo);

//Stop the DAB now
void dabStop() {
  dabFrequency = DAB_STOP;
  DABActive = false;
  clear_output();
  if (dab_img) lv_img_set_src(dab_img, &dabLogo);
  if (dab_img_bar) lv_bar_set_value(dab_img_bar, 0, LV_ANIM_OFF);
  dab.StopStream();
}

//Change the DAB to a given frequency
void dabPlay(long dabFreq) {
  dabStop();
  clearProgLbl();
  serial.println("> DAB Starting now");
  if (dabFreq > 10000) dab.PlayStream(STREAM_MODE_FM, dabFreq);
  else if (dabFreq >= 0) dab.PlayStream(STREAM_MODE_DAB, dabFreq);
  else if (dabFreq == DAB_LINEIN1) dab.PlayStream(STREAM_MODE_LINE1, 0);
  else if (dabFreq == DAB_LINEIN2) dab.PlayStream(STREAM_MODE_LINE2, 0);
  dabFrequency = dabFreq;
}

//Station name callback
void dabNameCallback(char *name, int8_t typeIndex) {
  char buf[64];
  if (settings.mode != MODE_DAB && settings.mode != MODE_FM) return;
  serial.print("> DAB event: Station name change: ");
  serial.println(name);
  if (settings.mode == MODE_FM) {
    strncpy(fmStationName, name, 34);
    fmStationName[34] = '\0';
  }
  strncpy(buf, name, 34);
  buf[34] = '\0';
  if (typeIndex > 0) {
    const char* type = dab.GetTypeString(typeIndex);
    serial.print(">            Station type: ");
    serial.println(type);
    int len = strlen(name);
    if (len < 35) {
      strcpy(&buf[len], "  (");
      strcpy(&buf[len+3], type);
      len += strlen(type) + 3;
      if (len < 62) strcpy(&buf[len++], ")");
      buf[len] = '\0';
    }
  }
  if (progNowLbl) lv_label_set_text(progNowLbl, buf);
}

//Program text callback
void dabTextCallback(char *text) {
  int offset = 0;
  if (settings.mode != MODE_DAB && settings.mode != MODE_FM) return;
  if (strncasecmp(text, "now", 3) == 0) {
    offset = 3;
    if (text[offset] == ' ') offset++;
    if (strncasecmp(&text[offset], "playing", 7) == 0) offset += 7;
    if (text[offset] == ' ') offset++;
    if (text[offset] == ':') offset++;
    if (strncmp(&text[offset], "...", 3) == 0) offset += 3;
    if (text[offset] == ' ') offset++;
    String txt = ((String)LV_SYMBOL_AUDIO) + " -" + (&text[offset]) + " -";            
    if (progNameLbl) lv_label_set_text(progNameLbl, txt.c_str());
  }
  else if (strncasecmp(text, "on now", 6) == 0) {
    offset = 6;
    if (text[offset] == ' ') offset++;
    if (text[offset] == ':') offset++;
    if (strncmp(&text[offset], "...", 3) == 0) offset += 3;
    if (text[offset] == ' ') offset++;
    String txt = ((String)LV_SYMBOL_AUDIO) + " -" + (&text[offset]) + " -";            
    if (progNameLbl) lv_label_set_text(progNameLbl, txt.c_str());
  }
  else if (progTextLbl) lv_label_set_text(progTextLbl, text);
}

//Frequency changed during channel scan
void dabFreqCallback(uint8_t frequency) {
  char str[30];
  if (settings.mode != MODE_DAB) return;    //Silence progress if DAB not visible
  if (frequency < 100) {
    sprintf(str, "[%d] %s", (int)dab.GetTotalProgram(), dab.GetFrequencyString(frequency));
    if (dabScanLabel) lv_label_set_text(dabScanLabel, str);
  } 
}

//DAB signal strength callback
void dabStrengthCallback(uint8_t signal) {
  char s[25] = {0};
  if (settings.mode != MODE_DAB) return;
  sprintf(s, LV_SYMBOL_WIFI " %d%%", signal);
  setSigStrengthLbl(s);
}

//DAB stereo mode callback
void dabStereoCallback(uint8_t mode) {
  if (settings.mode != MODE_DAB && settings.mode != MODE_FM) return;
  serial.print("> DAB event: Stereo mode change: ");
  if (mode == DAB_STEREO_MONO) {
    serial.println("Mono");
    updateSTMOLabel(STMO_MONO);
    return;
  } 
  else if (mode == DAB_STEREO_STEREO) serial.println("Stereo");
  else if (mode == DAB_STEREO_JOINT) serial.println("Joint");
  else if (mode == DAB_STEREO_DUAL) serial.println("Dual");
  else {
    serial.println("Error?");
    return;    
  }
  updateSTMOLabel(STMO_STEREO);
}

//DAB slideshow available callback
void dabUserAppCallback(uint8_t userApp) {
  if (settings.mode != MODE_DAB) return;
  //serial.print("App Type: ");
  //serial.println(dab.GetApplicationString(userApp));
  if (userApp == KSApplicationType_SLS) updateSTMOLabel(STMO_SLIDE);
}

//TOD not working?
void dabTimeCallback() {
  serial.println("DAB event: Time changed");
}

//Slideshow image arrived
uint16_t transport_id = 0;
void dabImageCallback(char *name, uint8_t type, uint16_t id, uint8_t *data, int length) {
  uint16_t w = 0, h = 0;
  if (settings.mode != MODE_DAB) return;
  //if (dab_img_bar) lv_bar_set_value(dab_img_bar, 0, LV_ANIM_OFF);
  TJpgDec.getJpgSize(&w, &h, data, length);
  if (w == 0 || h == 0) return;     //Problem with image
  if (ScreenSaverPending) {
    if (!ScreenSaverActive || id != transport_id) {
      if (w <= 320 && h <= 240) TJpgDec.setJpgScale(1);
      else if (w <= 640 && h <= 480) TJpgDec.setJpgScale(2);
      else TJpgDec.setJpgScale(4);
      TJpgDec.setCallback(tft_output);
      //serial.print("Width = "); serial.print(w); serial.print(", height = "); serial.println(h);
      // Draw the image, top left at 0,0
      TJpgDec.drawJpg(0, 0, data, length);
      serial.printf("> -Draw Fullscreen '%s', type: %s\r\n", name, type==CONTENT_SUB_TYPE_JFIF?"JPEG":"PNG");
      ScreenSaverActive = true;
    }
  }
  if (id != transport_id) {
    if (w <= 320 && h <= 240) TJpgDec.setJpgScale(4);
    else if (w <= 640 && h <= 480) TJpgDec.setJpgScale(8);
    else return;          //Too big
    TJpgDec.setCallback(render_output);
    TJpgDec.drawJpg(0, 0, data, length);      
    if (dab_img) lv_img_set_src(dab_img, &dab_img_dsc);
    serial.printf("> -Update Thumbnail '%s', type: %s\r\n", name, type==CONTENT_SUB_TYPE_JFIF?"JPEG":"PNG");
    transport_id = id;
  }
}

//Slideshow image is building
void dabProgressCallback(uint8_t percent) {
  //serial.printf("body %d%% finished\r\n" , percent);
  if (settings.mode != MODE_DAB) return;
  if (percent == 0) serial.println("> Start new image");
  if (ScreenSaverActive) tft.fillRect(0, 236, percent * 3.19, 4, ILI9341_RED);
  if (dab_img_bar) lv_bar_set_value(dab_img_bar, percent, LV_ANIM_OFF);
}


// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  //if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.writeRect(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}

// This next function will be called during decoding of the jpeg file to
// render each block.
bool render_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  uint16_t *d = bitmap;
  for (int r = y; r < y+h; r++) {
    int p = r * 80;
    for (int c = x; c < x+w; c++) 
      dab_img_data[p + c] = *d++;
  }
  // Return 1 to decode next block
  return 1;
}

//Clear the thumbnail
void clear_output() {
  for (int r = 0; r < 60; r++) {
    int p = r * 80;
    for (int c = 0; c < 80; c++) 
      dab_img_data[p + c] = 0;
  }
}
