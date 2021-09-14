unsigned int eeAddress = 0;   //Location we want the data to be put.

//Doing it this brain-dead looking way because EEPROM.put() blocks interrupts too long
void writeSettings() {
  for (uint16_t i = 0; i < sizeof(settings); i++) {
    uint8_t value = ((uint8_t*)&settings)[i];
    if (EEPROM.read(i) != value) EEPROM.write(i, value);
  }
}

bool readSettings() {
  if (EEPROM.read(eeAddress) != EE_MAGIC) {
    serial.println("> EEPROM Uninitialised! Writing defaults.");
    serial.printf("> Size of Settings object: %d (Max 1080)\r\n", sizeof(settingsObject));  
    writeSettings();
    return false;
  }
  EEPROM.get( eeAddress, settings );  
  return true;
}

//------------------------------------------------------------------
//lvgl widgets

// *******************************************************************
//Settings window
lv_obj_t * dabVolSlider;
lv_obj_t * webVolSlider;
lv_obj_t * webToneSlider1;
lv_obj_t * webToneSlider2;
lv_obj_t * webToneSlider3;
lv_obj_t * webToneSlider4;
lv_obj_t * dabEqList;
lv_obj_t * wifiNetworkList = NULL;
lv_obj_t * wifiStatusLbl = NULL;
lv_obj_t * wifiDisconnectBtn = NULL;
lv_obj_t * wifiDisconnectBtnLbl = NULL;
lv_obj_t * passwordEditText = NULL;

bool editWifiPassword = false;


void createSettingsWindow(lv_obj_t * page) {

  static lv_style_t style_bg;
  lv_style_init(&style_bg);
  lv_style_set_bg_color(&style_bg, lv_color_make(0x90, 0x90, 0x90));
  lv_style_set_bg_grad_color(&style_bg, lv_color_black());
  lv_style_set_bg_grad_dir(&style_bg, LV_GRAD_DIR_VER);
  lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
  
  static lv_style_t style_caption;
  lv_style_init(&style_caption);
  lv_style_set_text_font(&style_caption, &lv_font_montserrat_10);

  static lv_style_t style_slider;
  lv_style_init(&style_slider);
  lv_style_set_border_color(&style_slider, lv_color_hex(0xFF6025));
  lv_style_set_border_width(&style_slider, 2);
  lv_style_set_border_opa(&style_slider, LV_OPA_COVER);
  lv_style_set_radius(&style_slider, 5);
  lv_style_set_pad_all(&style_slider, 7);
  static lv_style_t style_sliderbg;
  lv_style_init(&style_sliderbg);
  lv_style_set_radius(&style_sliderbg, 5);
  static lv_style_t style_knob;
  lv_style_init(&style_knob);
  lv_style_set_radius(&style_knob, 5);
  lv_style_set_pad_all(&style_knob, 0);

  static lv_style_t style_ta;
  lv_style_init(&style_ta);
  lv_style_set_bg_color(&style_ta, lv_color_hex(0x606060));
  lv_style_set_bg_grad_color(&style_ta, lv_color_hex(0x606060));
  lv_style_set_bg_opa(&style_ta, LV_OPA_80);
  lv_style_set_radius(&style_ta, 3);
  lv_style_set_border_width(&style_ta, 1);
  lv_style_set_border_color(&style_ta, lv_color_hex(0xFF7F50));
  lv_style_set_text_color(&style_ta, lv_color_white());
  lv_style_set_text_font(&style_ta, &lv_font_montserrat_14);
  lv_style_set_pad_top(&style_ta, 2);
  lv_style_set_pad_bottom(&style_ta, 4);
  lv_style_set_pad_left(&style_ta, 6);
  lv_style_set_pad_right(&style_ta, 6);

  lv_obj_add_style(page, &style_bigfont, LV_PART_MAIN);

      //The main info container window
    mainContainer = lv_obj_create(page);
    lv_obj_set_size(mainContainer, 308, 52);
    lv_obj_set_pos(mainContainer, 4, 8);
    lv_obj_add_style(mainContainer, &style_groupbox, LV_PART_MAIN);
    lv_obj_clear_flag(mainContainer, LV_OBJ_FLAG_SCROLLABLE);
 
    // Add the master (DAB) volume slider
    lv_obj_t * volDABLbl = lv_label_create(mainContainer); //First parameters (scr) is the parent
    lv_obj_set_pos(volDABLbl, 10, 10);
    lv_label_set_text(volDABLbl, "Master Vol");  //Set the text
    dabVolSlider = lv_slider_create(mainContainer);                            //Create a slider
    lv_obj_set_size(dabVolSlider, 190, 18);   //Set the size
    lv_obj_align_to(dabVolSlider, volDABLbl, LV_ALIGN_OUT_RIGHT_MID, 10, 0);         //Align next to the slider
    lv_slider_set_range(dabVolSlider, 0, 16);                                            //Set the current value
    lv_slider_set_value(dabVolSlider, settings.dabVolume, LV_ANIM_OFF);
    lv_obj_add_style(dabVolSlider, &style_slider, LV_PART_MAIN);
    lv_obj_add_style(dabVolSlider, &style_sliderbg, LV_PART_INDICATOR);
    lv_obj_add_style(dabVolSlider, &style_knob, LV_PART_KNOB);
    lv_obj_add_event_cb(dabVolSlider, dabVolumeAction, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(dabVolSlider, settingReleasedAction, LV_EVENT_RELEASED, NULL);


    // -- WIFI controls
      //The WIFI info container window
    wifiContainer = lv_obj_create(page);
    lv_obj_set_size(wifiContainer, 308, 80);
    lv_obj_add_style(wifiContainer, &style_groupbox, LV_PART_MAIN);
    lv_obj_clear_flag(wifiContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(wifiContainer, mainContainer, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);         //Align next to the slider
    lv_obj_set_hidden(wifiContainer, true);

    //Create a network dropdown
    lv_obj_t * btn_label = lv_label_create(wifiContainer);
    lv_label_set_text(btn_label, "WIFI");
    lv_obj_set_pos(btn_label, 10, 10);                //Align below the first button
    wifiNetworkList = lv_dropdown_create(wifiContainer);
    lv_obj_align_to(wifiNetworkList, btn_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);         //Align next to the slider
    lv_obj_set_width(wifiNetworkList, 236);                //Align below the first button
    lv_obj_add_event_cb(wifiNetworkList, wifiSelectedAction, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(wifiNetworkList, wifiScanAction, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(wifiNetworkList, wifiLongPressAction, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_style(wifiNetworkList, &style_bg, LV_PART_MAIN);
    lv_dropdown_set_options(wifiNetworkList, settings.networks[settings.currentNetwork].ssid);
    
    wifiStatusLbl = lv_label_create(wifiContainer);
    lv_obj_align_to(wifiStatusLbl, btn_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 16);         //Align next to the slider
    setWifiStatus(getWlanState());

    passwordEditText = lv_textarea_create(wifiContainer);
    lv_obj_set_size(passwordEditText, 236, 20);
    lv_obj_align_to(passwordEditText, wifiNetworkList, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 9);         //Align next to the slider
    lv_obj_add_style(passwordEditText, &style_ta, LV_PART_MAIN);
    lv_textarea_set_text_selection(passwordEditText, false);
    lv_textarea_set_one_line(passwordEditText, true);
    lv_textarea_set_text(passwordEditText, settings.networks[settings.currentNetwork].password);
    lv_obj_add_event_cb(passwordEditText, passwordEditAction, LV_EVENT_PRESSED, NULL);
    
    wifiDisconnectBtn = lv_btn_create(wifiContainer);
    lv_obj_add_event_cb(wifiDisconnectBtn, wifiDisconnectAction, LV_EVENT_CLICKED, NULL);
    lv_obj_add_style(wifiDisconnectBtn, &style_bg, LV_PART_MAIN);
    wifiDisconnectBtnLbl = lv_label_create(wifiDisconnectBtn);
    lv_label_set_text(wifiDisconnectBtnLbl, LV_SYMBOL_CLOSE);
    lv_obj_align_to(wifiDisconnectBtn, wifiNetworkList, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);         //Align next to the slider
    setPasswordVisibility(false, false);


    // -- DAB controls
      //The DAB Radio info container window
    dabContainer = lv_obj_create(page);
    lv_obj_set_size(dabContainer, 308, 86);
    lv_obj_add_style(dabContainer, &style_groupbox, LV_PART_MAIN);
    lv_obj_clear_flag(dabContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(dabContainer, mainContainer, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);         //Align next to the slider
    lv_obj_set_hidden(dabContainer, true);

        //Create a scan button
    lv_obj_t * scanBtn = lv_btn_create(dabContainer);
    lv_obj_set_pos(scanBtn, 10, 10);                //Align below the first button
    lv_obj_add_event_cb(scanBtn, dabScanAction, LV_EVENT_CLICKED, NULL);
    lv_obj_add_style(scanBtn, &style_bg, LV_PART_MAIN);
    btn_label = lv_label_create(scanBtn);
    lv_label_set_text(btn_label, "DAB Scan");

    dabScanLabel = lv_label_create(dabContainer);
    lv_obj_align_to(dabScanLabel, scanBtn, LV_ALIGN_OUT_RIGHT_MID, 10, 0);         //Align next to the slider
    lv_obj_set_width(dabScanLabel, 155);
    lv_label_set_text(dabScanLabel, "");

    //Loading spinner
    scanSpinner = lv_spinner_create(dabContainer, 1000, 60);
    lv_obj_set_size(scanSpinner, 25, 25);
    lv_obj_align_to(scanSpinner, dabScanLabel, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_hidden(scanSpinner, true);

    // ADD A DROP DOWN LIST
    lv_obj_t * eqLbl = lv_label_create(dabContainer); //First parameters (scr) is the parent
    lv_obj_align_to(eqLbl, scanBtn, LV_ALIGN_OUT_BOTTOM_LEFT, 5, 14);         //Align next to the slider
    lv_label_set_text(eqLbl, "DAB DSP");  //Set the text

    dabEqList = lv_dropdown_create(dabContainer);            //Create a drop down list
    lv_obj_align_to(dabEqList, eqLbl, LV_ALIGN_OUT_RIGHT_MID, 20, 0);         //Align next to the slider
    lv_obj_set_size(dabEqList, 114, 22);
    lv_obj_add_style(dabEqList, &style_bg, LV_PART_MAIN);
    lv_dropdown_set_options(dabEqList, "EQ Off\nBass Boost\nJazz\nLive\nVocal\nAcoustic"); //Set the options
    lv_dropdown_set_selected(dabEqList, settings.dabEQ);
    lv_obj_add_event_cb(dabEqList, dabEqAction, LV_EVENT_VALUE_CHANGED, NULL);                         //Set function to call on new option is chosen
    lv_obj_add_event_cb(dabEqList, dabOpenEqAction, LV_EVENT_CLICKED, NULL);

    // -- VS1053 controls
      //The VS Codec info container window
    vsContainer = lv_obj_create(page);
    lv_obj_set_size(vsContainer, 308, 162);
    lv_obj_add_style(vsContainer, &style_groupbox, LV_PART_MAIN);
    lv_obj_clear_flag(vsContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(vsContainer, mainContainer, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);         //Align next to the slider
    lv_obj_set_hidden(vsContainer, true);

    // ADD A SLIDER
    lv_obj_t * volVSLbl = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_obj_set_pos(volVSLbl, 10, 10);
    //lv_obj_align_to(volVSLbl, eqLbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);         //Align next to the slider
    lv_label_set_text(volVSLbl, "Codec Vol");  //Set the text

    webVolSlider = lv_slider_create(vsContainer);                            //Create a slider
    lv_obj_set_size(webVolSlider, 190, 18);   //Set the size
    lv_obj_align_to(webVolSlider, volVSLbl, LV_ALIGN_OUT_RIGHT_MID, 13, 0);         //Align next to the slider
    lv_slider_set_range(webVolSlider, 50, 100);                                            //Set the current value
    lv_slider_set_value(webVolSlider, settings.vsVolume, LV_ANIM_OFF);
    lv_obj_add_style(webVolSlider, &style_slider, LV_PART_MAIN);
    lv_obj_add_style(webVolSlider, &style_sliderbg, LV_PART_INDICATOR);
    lv_obj_add_style(webVolSlider, &style_knob, LV_PART_KNOB);
    lv_obj_add_event_cb(webVolSlider, webVolumeAction, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(webVolSlider, settingReleasedAction, LV_EVENT_RELEASED, NULL);

    // ADD SLIDERS
    lv_obj_t * toneLbl = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_obj_align_to(toneLbl, volVSLbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 14);         //Align next to the slider
    lv_label_set_text(toneLbl, "Codec\nTone");  //Set the text

    webToneSlider1 = lv_slider_create(vsContainer);                            //Create a slider
    lv_obj_set_size(webToneSlider1, 18, 80);   //Set the size
    lv_obj_align_to(webToneSlider1, toneLbl, LV_ALIGN_OUT_RIGHT_TOP, 40, 4);                //Align below the first button
    //lv_slider_set_knob_in(webToneSlider1, true);
    lv_slider_set_range(webToneSlider1, 0, 15);                                            //Set the current value
    lv_slider_set_value(webToneSlider1, ((settings.vsTone >> 12) + 8) & 0x0F, LV_ANIM_OFF);                                            //Set the current value
    lv_obj_add_style(webToneSlider1, &style_slider, LV_PART_MAIN);
    lv_obj_add_style(webToneSlider1, &style_sliderbg, LV_PART_INDICATOR);
    lv_obj_add_style(webToneSlider1, &style_knob, LV_PART_KNOB);
    lv_obj_t * label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "Treble");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider1, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "+12");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider1, LV_ALIGN_OUT_LEFT_TOP, -4, 0);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "-12");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider1, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 0);                //Align below the first button
    lv_obj_add_event_cb(webToneSlider1, webToneAction, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(webToneSlider1, settingReleasedAction, LV_EVENT_RELEASED, NULL);

    webToneSlider2 = lv_slider_create(vsContainer);                            //Create a slider
    lv_obj_set_size(webToneSlider2, 18, 80);   //Set the size
    lv_obj_align_to(webToneSlider2, webToneSlider1, LV_ALIGN_OUT_RIGHT_TOP, 34, 0);                //Align below the first button
    //lv_slider_set_knob_in(webToneSlider2, true);
    lv_slider_set_range(webToneSlider2, 1, 15);                                            //Set the current value
    lv_slider_set_value(webToneSlider2, (settings.vsTone >> 8) & 0x0F, LV_ANIM_OFF);                                            //Set the current value
    lv_obj_add_style(webToneSlider2, &style_slider, LV_PART_MAIN);
    lv_obj_add_style(webToneSlider2, &style_sliderbg, LV_PART_INDICATOR);
    lv_obj_add_style(webToneSlider2, &style_knob, LV_PART_KNOB);
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "TFreq");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider2, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "15K");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider2, LV_ALIGN_OUT_LEFT_TOP, -4, 0);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "1K");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider2, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 0);                //Align below the first button
    lv_obj_add_event_cb(webToneSlider2, webToneAction, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(webToneSlider2, settingReleasedAction, LV_EVENT_RELEASED, NULL);

    webToneSlider3 = lv_slider_create(vsContainer);                            //Create a slider
    lv_obj_set_size(webToneSlider3, 18, 80);   //Set the size
    lv_obj_align_to(webToneSlider3, webToneSlider2, LV_ALIGN_OUT_RIGHT_TOP, 49, 0);                //Align below the first button
    //lv_slider_set_knob_in(webToneSlider3, true);
    lv_slider_set_range(webToneSlider3, 0, 15);                                            //Set the current value
    lv_slider_set_value(webToneSlider3, (settings.vsTone >> 4) & 0x0F, LV_ANIM_OFF);                                            //Set the current value
    lv_obj_add_style(webToneSlider3, &style_slider, LV_PART_MAIN);
    lv_obj_add_style(webToneSlider3, &style_sliderbg, LV_PART_INDICATOR);
    lv_obj_add_style(webToneSlider3, &style_knob, LV_PART_KNOB);
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "Bass");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider3, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "+15");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider3, LV_ALIGN_OUT_LEFT_TOP, -4, 0);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "OFF");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider3, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 0);                //Align below the first button
    lv_obj_add_event_cb(webToneSlider3, webToneAction, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(webToneSlider3, settingReleasedAction, LV_EVENT_RELEASED, NULL);

    webToneSlider4 = lv_slider_create(vsContainer);                            //Create a slider
    lv_obj_set_size(webToneSlider4, 18, 80);   //Set the size
    lv_obj_align_to(webToneSlider4, webToneSlider3, LV_ALIGN_OUT_RIGHT_TOP, 34, 0);                //Align below the first button
    //lv_slider_set_knob_in(webToneSlider4, true);
    lv_slider_set_range(webToneSlider4, 2, 15);                                            //Set the current value
    lv_slider_set_value(webToneSlider4, settings.vsTone & 0x0F, LV_ANIM_OFF);                                            //Set the current value
    lv_obj_add_style(webToneSlider4, &style_slider, LV_PART_MAIN);
    lv_obj_add_style(webToneSlider4, &style_sliderbg, LV_PART_INDICATOR);
    lv_obj_add_style(webToneSlider4, &style_knob, LV_PART_KNOB);
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "BFreq");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider4, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "150");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider4, LV_ALIGN_OUT_LEFT_TOP, -4, 0);                //Align below the first button
    label = lv_label_create(vsContainer); //First parameters (scr) is the parent
    lv_label_set_text(label, "20");  //Set the text
    lv_obj_add_style(label, &style_caption, LV_PART_MAIN);
    lv_obj_align_to(label, webToneSlider4, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 0);                //Align below the first button
    lv_obj_add_event_cb(webToneSlider4, webToneAction, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(webToneSlider4, settingReleasedAction, LV_EVENT_RELEASED, NULL);

}


//-----------------------------------------------------------------------
// main container actions

static void dabVolumeAction(lv_event_t * event) {
  settings.dabVolume = lv_slider_get_value(dabVolSlider); 
  dab.SetVolume(settings.dabVolume);
}

static void settingReleasedAction(lv_event_t * event) {
  writeSettings();
}


//-----------------------------------------------------------------------
// WIFI container actions

static void wifiDisconnectAction(lv_event_t * event) {
  uint8_t state = getWlanState();
  if (state == WLAN_IDLE) {
    wlanConnect();
    connectToHost(settings.server); 
  }
  else if (state == WLAN_CONNECTED) {
    wlanDisconnect();
  }
}

void setWifiStatus(int state) {
  if (editWifiPassword) return;
  if (wifiStatusLbl) {
    char str[64];
    sprintf(str, "Status: %s", wlanStateString[state]);
    lv_label_set_text(wifiStatusLbl, str);  //Set the text
  }
  if (state == WLAN_IDLE) {
    if (wifiDisconnectBtnLbl && wifiNetworkList) {
      lv_label_set_text(wifiDisconnectBtnLbl, LV_SYMBOL_REFRESH);
      lv_obj_align_to(wifiDisconnectBtn, wifiNetworkList, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);         //Align next to the slider
    }
  } else if (state == WLAN_CONNECTED) {
    if (wifiDisconnectBtnLbl && wifiNetworkList) {
      lv_label_set_text(wifiDisconnectBtnLbl, LV_SYMBOL_CLOSE);
      lv_obj_align_to(wifiDisconnectBtn, wifiNetworkList, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);         //Align next to the slider
    }
  }
}

bool wifiLongPressed = false;
static void wifiLongPressAction(lv_event_t * event) {
  lv_obj_t * dropdown = lv_event_get_target(event);
  lv_obj_t * ddlist = lv_dropdown_get_list(dropdown);
  if (!ddlist) {  
    if (passwordEditText) lv_textarea_set_text(passwordEditText, settings.networks[settings.currentNetwork].password);
    setPasswordVisibility(!editWifiPassword, true);
    wifiLongPressed = true;
  }
}

static void wifiScanAction(lv_event_t * event) {
  if (wifiLongPressed) {
    wifiLongPressed = false;
    lv_dropdown_close(wifiNetworkList);
    return;
  }
  lv_obj_t * dropdown = lv_event_get_target(event);
  lv_obj_t * ddlist = lv_dropdown_get_list(dropdown);
  lv_state_t state = lv_obj_get_state(dropdown);
  if (ddlist && (state & LV_STATE_CHECKED)) {  
    serial.println("Starting WIFI AP Scan..");
    lv_dropdown_set_options(wifiNetworkList, "Scanning..");
    lv_dropdown_open(wifiNetworkList);
    startWifiScan();
  }
}

void wifiScanEntry(int index, char* entry) {
  if (!wifiNetworkList) return;
  if (index == 1) {
    setPasswordVisibility(false, false);
    lv_dropdown_clear_options(wifiNetworkList);
  }
  char * nameStart = strchr(entry, '"');
  if (nameStart) {
    char * name = nameStart + 1;
    char * nameEnd = strchr(name, '"');
    if (nameEnd) {
      *nameEnd = 0;
      char str[32];
      serial.print("> Network found: ");
      serial.println(name);
      bool us = (strcasecmp(name, settings.networks[settings.currentNetwork].ssid) == 0);
      sprintf(str, "%s%s", us? LV_SYMBOL_WIFI " ":"", name); 
      lv_dropdown_add_option(wifiNetworkList, str, LV_DROPDOWN_POS_LAST);
        if (us) lv_dropdown_set_selected(wifiNetworkList, index - 1);
      lv_dropdown_open(wifiNetworkList);
    }
  }
}

static void wifiSelectedAction(lv_event_t * event) {
  char name[16];
  lv_dropdown_get_selected_str(wifiNetworkList, name, 15);
  bool known = false;
  bool added = false;
  for(int net = 0; net < 4; net++) {
    if (strcmp(settings.networks[net].ssid, name) == 0) {
      if (net != settings.currentNetwork) {
        settings.currentNetwork = net;
        writeSettings();
        serial.print("Known network:");
        serial.println(net);
        
      } else { 
        serial.print("Same network:");
        serial.println(net);
      }
      wlanConnect();
      connectToHost(settings.server); 
      known = true;
      added = true;
      break;
    }
  }
  if (!known) {
    for(int net = 0; net < 4; net++) {
      if (strcasecmp(settings.networks[net].ssid, "<Empty>") == 0) {
        strncpy(settings.networks[net].ssid, name, 15);
        settings.networks[net].ssid[15] = '\0';
        strcpy(settings.networks[settings.currentNetwork].password, "");
        settings.currentNetwork = net;
        writeSettings();
        wlanConnect();
        connectToHost(settings.server); 
        serial.print("New network:");
        serial.println(net);
        added = true;
        break;
        //connectToHost(settings.server); 
      }  
    }
  }
  if (!added) {
    if (++settings.currentNetwork >= 4) settings.currentNetwork = 0;
    strncpy(settings.networks[settings.currentNetwork].ssid, name, 15);
    settings.networks[settings.currentNetwork].ssid[15] = '\0';
    strcpy(settings.networks[settings.currentNetwork].password, "");
    writeSettings();
    wlanConnect();
    connectToHost(settings.server); 
        serial.print("Networks Full:");
        serial.println(settings.currentNetwork);
  }
  if (passwordEditText) 
    lv_textarea_set_text(passwordEditText, settings.networks[settings.currentNetwork].password);
}

void wifiConnectError(int error) {
  if (progTextLbl) lv_label_set_text(progTextLbl, wlanErrorString[error]);
  if (error == 2) setPasswordVisibility(true, false);    
  else if (wifiStatusLbl) lv_label_set_text(wifiStatusLbl, wlanErrorString[error]);  //Set the text
  serial.println(wlanErrorString[error]);  
}

#define PASSWORD_SHOW_TIME  5000  //five secs

uint64_t passwordCountdown = 0;
void beginPasswordCountdown() {
  passwordCountdown = millis() + PASSWORD_SHOW_TIME;
}

void passwordHandle() {
  if (passwordCountdown && passwordCountdown < millis()) {
    setPasswordVisibility(false, false);
  }
}

void setPasswordVisibility(bool visible, bool timed) {
  if ((editWifiPassword = visible)) {
    if (wifiStatusLbl) lv_label_set_text(wifiStatusLbl, "PASS");
    if (wifiDisconnectBtn) lv_obj_add_flag(wifiDisconnectBtn, LV_OBJ_FLAG_HIDDEN);
    if (passwordEditText) lv_obj_clear_flag(passwordEditText, LV_OBJ_FLAG_HIDDEN);
    if (timed) beginPasswordCountdown();
  } else {
    setWifiStatus(getWlanState());
    if (passwordEditText) lv_obj_add_flag(passwordEditText, LV_OBJ_FLAG_HIDDEN);
    if (wifiDisconnectBtn) lv_obj_clear_flag(wifiDisconnectBtn, LV_OBJ_FLAG_HIDDEN);
    passwordCountdown = 0;
  }
}

void passwordEditAction(lv_event_t * event) {
  if (keyBoard == NULL) {
    keyboardShow(passwordEditText);
    lv_obj_add_event_cb(keyBoard, keyboardPasswordKeyAction, LV_EVENT_ALL, NULL);
    beginPasswordCountdown();
  }
}

void keyboardPasswordKeyAction(lv_event_t * event) {
  uint32_t res = lv_event_get_code(event);
  if(res == LV_EVENT_READY || res == LV_EVENT_CANCEL){
    if (keyBoard) keyboardHide(true);
    if(res == LV_EVENT_READY) {
      setPasswordVisibility(false, false);
      strncpy(settings.networks[settings.currentNetwork].password, lv_textarea_get_text(passwordEditText), 31); 
      settings.networks[settings.currentNetwork].password[31] = '\0';
      writeSettings();
      wlanConnect();
      connectToHost(settings.server); 
    }
    else if (res == LV_EVENT_CANCEL){
      lv_textarea_set_text(passwordEditText, settings.networks[settings.currentNetwork].password);  
    }
  }
  beginPasswordCountdown();
}


void wifiScanComplete() {
  serial.println("WIFI AP Scan Finished.");
}

//-----------------------------------------------------------------------
// DAB container actions

static void dabScanAction(lv_event_t * event) {
  if (dabScanLabel) lv_label_set_text(dabScanLabel, "Starting..");
  startDabScan();
}

static void dabEqAction(lv_event_t * event) {
  lv_obj_t * obj = lv_event_get_target(event);
  uint16_t opt = lv_dropdown_get_selected(obj);      //Get the id of selected option
  settings.dabEQ = opt; 
  setDabEQ(opt);
  writeSettings();
}

static void dabOpenEqAction(lv_event_t * event) {
  lv_obj_t * dropdown = lv_event_get_target(event);
  lv_obj_t * ddlist = lv_dropdown_get_list(dropdown);
  lv_state_t state = lv_obj_get_state(dropdown);
  if (ddlist && (state & LV_STATE_CHECKED)) {  
    lv_obj_add_style(ddlist, &style_bigfont, LV_PART_MAIN);
  }
}

//-----------------------------------------------------------------------
// VS1053 container actions

static void webVolumeAction(lv_event_t * event)
{
  settings.vsVolume = lv_slider_get_value(webVolSlider); 
  setVSVolume(settings.vsVolume);
}

static void webToneAction(lv_event_t * event) {
  settings.vsTone = formatVSTone(lv_slider_get_value(webToneSlider1), lv_slider_get_value(webToneSlider2), lv_slider_get_value(webToneSlider3), lv_slider_get_value(webToneSlider4)); 
  setVSTone(settings.vsTone);
}
