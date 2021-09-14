//===============================================================================================
// LVGL Gui constructions

//Shared styles
static lv_style_t style_kb;
static lv_style_t style_kb_rel;
static lv_style_t style_kb_pr;
static lv_style_t style_groupbox;
static lv_style_t style_transp;
static lv_style_t style_bigfont;
static lv_style_t style_bigfont_list;

//Global window objects
static lv_obj_t * urlEditText;
static lv_obj_t * keyBoard;
static lv_obj_t * mainWindow;
static lv_obj_t * presetsWindow;
static lv_obj_t * terminalWindow;
static lv_obj_t * settingsWindow;
static lv_obj_t * stationsWindow;
static lv_obj_t * playlistWindow;
static lv_obj_t * sysmonWindow;
static lv_obj_t * functionLbl;
static lv_obj_t * modeList;


//Init the screen and build the GUI windows
void screenInit(void)
{
   //Initialize the theme (this will need the extra fonts in future LVGL versions)
  lv_theme_t * th = lv_theme_default_init(NULL,  //Use the DPI, size, etc from default display 
                                          lv_palette_main(LV_PALETTE_ORANGE), lv_palette_main(LV_PALETTE_CYAN),   //Primary and secondary palette
                                          true,    //Light or dark mode 
                                          &lv_font_unscii_8); //Small, normal, large fonts
                                          
  lv_disp_set_theme(NULL, th); //Assign the theme to the default display

  //Style of the wallpaper
  lv_style_init(&style_wp);
  lv_style_set_bg_color(&style_wp, lv_color_hex(0x707070));
  lv_style_set_bg_grad_color(&style_wp, lv_color_black());
  lv_style_set_bg_grad_dir(&style_wp, LV_GRAD_DIR_VER);
  lv_style_set_text_color(&style_wp, lv_color_white());
  lv_obj_add_style(lv_scr_act(), &style_wp, 0);

  //Create a wallpaper on the screen
  LV_IMG_DECLARE(wallpaper);
  lv_obj_t * wp = lv_img_create(lv_scr_act());
  //lv_obj_set_protect(wp, LV_PROTECT_PARENT);          //Don't let to move the wallpaper by the layout 
  lv_obj_set_parent(wp, lv_scr_act());
  lv_img_set_src(wp, &wallpaper);//bgimg);
  lv_obj_set_pos(wp, 0, 33);
  //lv_img_set_auto_size(wp, false);

  //---- Top line ----------------
  //Setup some styles
  lv_style_init(&style_bigfont);
  lv_style_set_text_font(&style_bigfont, &lv_font_montserrat_14);
  lv_style_init(&style_bigfont_orange);
  lv_style_set_text_font(&style_bigfont_orange, &lv_font_montserrat_16);
  lv_style_set_text_color(&style_bigfont_orange, lv_palette_main(LV_PALETTE_ORANGE));
  lv_style_init(&style_bigfont_list);
  lv_style_set_text_font(&style_bigfont_list, &lv_font_montserrat_16);
  lv_style_init(&style_halfopa);
  lv_style_set_bg_opa(&style_halfopa, LV_OPA_50);
  lv_style_init(&style_groupbox);
  lv_style_set_bg_color(&style_groupbox, lv_color_hex(0x0));//487fb7);
  lv_style_set_bg_grad_color(&style_groupbox, lv_color_hex(0x0));//487fb7);
  lv_style_set_bg_opa(&style_groupbox, LV_OPA_50);
  lv_style_set_border_width(&style_groupbox, 3);
  lv_style_set_border_color(&style_groupbox, lv_color_hex(0x202020));
  lv_style_init(&style_listsel);
  lv_style_set_radius(&style_listsel, 6);
  lv_style_set_bg_opa(&style_listsel, LV_OPA_70);
  lv_style_set_border_width(&style_listsel, 2);
  lv_style_set_border_color(&style_listsel, lv_color_hex(0xFF7F50));
  lv_style_set_border_side(&style_listsel, LV_BORDER_SIDE_FULL);
  lv_style_init(&style_win);
  lv_style_set_text_font(&style_win, &lv_font_montserrat_14);
  lv_style_set_bg_opa(&style_win, LV_OPA_TRANSP);
  lv_style_set_border_width(&style_win, 0);
  lv_style_set_pad_top(&style_win, 0);
  lv_style_set_pad_bottom(&style_win, 0);
  lv_style_set_pad_left(&style_win, 0);
  lv_style_set_pad_right(&style_win, 0);


  // Top line mode (dab, web, sd..) dropdown list
  modeList = lv_dropdown_create(lv_scr_act());            //Create a drop down list
  lv_obj_add_style(modeList, &style_wp, LV_PART_MAIN);
  lv_obj_add_style(modeList, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_set_pos(modeList, 2, 2);  
  lv_dropdown_set_options(modeList, LV_SYMBOL_PLUS " DAB Radio\n" LV_SYMBOL_AUDIO " Digital FM\n" LV_SYMBOL_IMAGE " Web Radio\n" LV_SYMBOL_SAVE " SD Card\n" LV_SYMBOL_UPLOAD " Line IN"); //Set the options
  lv_dropdown_set_selected(modeList, settings.mode);
  lv_obj_set_size(modeList, 138, 29);
  lv_obj_add_event_cb(modeList, dabModeAction, LV_EVENT_VALUE_CHANGED, NULL);                         //Set function to call on new option is chosen
  lv_obj_add_event_cb(modeList, modeListActivated, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(modeList, LV_OBJ_FLAG_HIDDEN);

  //Top line labels
  functionLbl = lv_label_create(lv_scr_act()); //First parameters (scr) is the parent
  lv_obj_set_pos(functionLbl, 8, 8);  
  lv_label_set_text(functionLbl, LV_SYMBOL_AUDIO " Digital Boombox");
  lv_obj_add_style(functionLbl, &style_bigfont, LV_PART_MAIN);
  battChgLbl = lv_label_create(lv_scr_act()); //First parameters (scr) is the parent
  lv_label_set_text(battChgLbl, "");
  lv_obj_add_style(battChgLbl, &style_bigfont, LV_PART_MAIN);
  lv_obj_align(battChgLbl, LV_ALIGN_TOP_RIGHT, -8, 8);  //Align below the label
  sigStrengthLbl = lv_label_create(lv_scr_act()); //First parameters (scr) is the parent
  lv_label_set_text(sigStrengthLbl, "");
  lv_obj_add_style(sigStrengthLbl, &style_bigfont, LV_PART_MAIN);
  lv_obj_align_to(sigStrengthLbl, battChgLbl, LV_ALIGN_OUT_LEFT_MID, -2, 0);  //Align below the label
  stmoLbl = lv_label_create(lv_scr_act()); //First parameters (scr) is the parent
  lv_label_set_text(stmoLbl, "");
  lv_obj_add_style(stmoLbl, &style_bigfont, LV_PART_MAIN);
  lv_obj_align_to(stmoLbl, sigStrengthLbl, LV_ALIGN_OUT_LEFT_MID, -6, 0);  //Align below the label

 
  //--body

  //Create a tabview for the body container
  lv_style_init(&style_transp);
  lv_style_set_bg_opa(&style_transp, LV_OPA_TRANSP);
  lv_style_set_border_opa(&style_transp, LV_OPA_TRANSP);
  tabView = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 33);
  lv_obj_t * tabButtons = lv_tabview_get_tab_btns(tabView);
  lv_obj_add_flag(tabButtons, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_style(tabView, &style_transp, LV_PART_MAIN);
  lv_obj_set_size(tabView, LV_HOR_RES, LV_VER_RES - 33);
  lv_obj_set_pos(tabView, 0, 33);
  //get the drag event - used to cancel some actions when screen is slid away
  lv_obj_t * cont = lv_tabview_get_content(tabView);
  lv_obj_add_event_cb(cont, tabViewAction, LV_EVENT_ALL, NULL);

  //Create the tabs
  settingsWindow = lv_tabview_add_tab(tabView, "Settings");
  stationsWindow = lv_tabview_add_tab(tabView, "Stations");
  mainWindow = lv_tabview_add_tab(tabView, "Main");
  presetsWindow = lv_tabview_add_tab(tabView, "Presets");
  terminalWindow = lv_tabview_add_tab(tabView, "Terminal");
  sysmonWindow = lv_tabview_add_tab(tabView, "Debug");
  lv_tabview_set_act(tabView, 4, LV_ANIM_OFF);              //Start on the terminal
  
  //Create the contents
  createSettingsWindow(settingsWindow);
  createFileBrowserWindow(stationsWindow);
  createStationsWindow(stationsWindow);
  createMainWindow(mainWindow);
  createPresetsWindow(presetsWindow);
  createPlaylistWindow(presetsWindow);
  createTerminalWindow(terminalWindow);
  createSysmonWindow(sysmonWindow);
}

//Required when changing the top line labels to preserve their layout
void realignTopLabels() {
  if (battChgLbl) lv_obj_align(battChgLbl, LV_ALIGN_TOP_RIGHT, -8, 8);  //Align below the label
  if (sigStrengthLbl) lv_obj_align_to(sigStrengthLbl, battChgLbl, LV_ALIGN_OUT_LEFT_MID, -6, 0);  //Align below the label
  if (stmoLbl) lv_obj_align_to(stmoLbl, sigStrengthLbl, LV_ALIGN_OUT_LEFT_MID, -8, 0);  //Align below the label  
}

//Signal strength
void setSigStrengthLbl(const char * txt) {
  if (sigStrengthLbl) {
    lv_label_set_text(sigStrengthLbl, txt);
    realignTopLabels();  
  }
}

//Battery charge
void setBattChgLbl(const char * txt) {
  if (battChgLbl) {
    lv_label_set_text(battChgLbl, txt);
    realignTopLabels();  
  }
}

//Stero/mono/slideshow
void setStmoLbl(const char * txt) {
  if (stmoLbl) {
    lv_label_set_text(stmoLbl, txt);
    realignTopLabels();  
  }
}

//Mode dropdown opened, set the style of the dropdown
void modeListActivated(lv_event_t * event) {
  lv_obj_t * dropdown = lv_event_get_target(event);
  lv_obj_t * ddlist = lv_dropdown_get_list(dropdown);
  lv_state_t state = lv_obj_get_state(dropdown);
  if (ddlist && (state & LV_STATE_CHECKED)) {  
    lv_obj_add_style(ddlist, &style_bigfont_list, LV_PART_MAIN);
    lv_obj_set_style_max_height(ddlist, 300, LV_PART_MAIN);
    lv_obj_set_height(ddlist, LV_SIZE_CONTENT);
  }
}

//Tabview slid left or right - cancel any editing actions
bool tabViewScrolling = false;
void tabViewAction(lv_event_t * event) {
  lv_event_code_t code = lv_event_get_code(event);
  if(code == LV_EVENT_SCROLL_BEGIN) {
    if (keyBoard) {
      keyboardHide(false);
      lv_textarea_set_text(urlEditText, settings.server);  
      lv_obj_scroll_to(urlEditText, 0, 0, LV_ANIM_ON);
    }
    tabViewScrolling = true;
  }
  else if(code == LV_EVENT_SCROLL_END) tabViewScrolling = false;
}


//------------------------------------------------------------------------------------
// Stations list window - sorted list

void createStationsWindow(lv_obj_t * page) {
  //Station list - not much to it
  dabStationList = lv_list_create(page);
  lv_obj_set_pos(dabStationList, 0, 0);
  lv_obj_set_size(dabStationList, lv_obj_get_content_width(page), LV_VER_RES - 33);
  lv_obj_align(dabStationList, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_style(dabStationList, &style_transp, LV_PART_MAIN);
  lv_obj_add_style(dabStationList, &style_bigfont, LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(dabStationList, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_hidden(dabStationList, true);  
}

//Clear a station list and also free the entry's attached userdata
void clearStations() {
  for (uint16_t i = 0; i < lv_obj_get_child_cnt(dabStationList); i++) {
    lv_obj_t* child = lv_obj_get_child(dabStationList, i);
    void * data = lv_obj_get_user_data(child);
    if (data) {
      free(data);
      data = NULL;
    }
  }
  lv_obj_clean(dabStationList);
}

//Add an entry to the station list
lv_obj_t * addStationButton(const char* text, lv_event_cb_t callback, void* data, int datalen) {
  lv_obj_t * list_btn;
  if (!dabStationList) return NULL;
  list_btn = lv_list_add_btn(dabStationList, LV_SYMBOL_AUDIO, text);
  //Insertion sort
  int num = lv_obj_get_child_cnt(dabStationList);
  if (num > 1) {
    for (int i = num - 1; i >= 0 ; i--) {
      lv_obj_t* btn = lv_obj_get_child(dabStationList, i);
      const char * lbl = lv_list_get_btn_text(dabStationList, btn); 
      if (strncasecmp(lbl, text, 34) < 0) break;
      lv_obj_swap(list_btn, btn);
    }
  }
  lv_obj_add_event_cb(list_btn, callback, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(list_btn, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_add_style(list_btn, &style_halfopa, LV_PART_MAIN);
  lv_obj_add_style(list_btn, &style_listsel, LV_STATE_CHECKED);
  if (data) {
    void* ptr = malloc(datalen);
    memcpy(ptr, data, datalen);
    lv_obj_set_user_data(list_btn, ptr);
  }
  return list_btn;
}

//Set the currently highlighted station list entry selection
void listSetSelect(lv_obj_t * btn) {
  lv_obj_t* parent = lv_obj_get_parent(btn);
  for (uint16_t i = 0; i < lv_obj_get_child_cnt(parent); i++) {
    lv_obj_t* child = lv_obj_get_child(parent, i);
    if (child == btn) {
      lv_obj_add_state(child, LV_STATE_CHECKED);
      lv_obj_scroll_to_view(child, LV_ANIM_OFF);
    }
    else lv_obj_clear_state(child, LV_STATE_CHECKED);
  }
}

//See if a station with a given url is already in the list
bool stationInList(char * url) {
  if (!dabStationList) return false;
  for (uint16_t i = 0; i < lv_obj_get_child_cnt(dabStationList); i++) {
    lv_obj_t* child = lv_obj_get_child(dabStationList, i);
    void * data = lv_obj_get_user_data(child);
    if (data && strncmp((char*)data, url, 255) == 0) return true;
  }
  return false;
}


//------------------------------------------------------------------------------
//Presets window
// Not using a button map as the label control is insufficient

#define NUM_PRESETS 15

//A map of actual buttons
lv_obj_t * presetButtons[NUM_PRESETS];
lv_obj_t * presetLabels[NUM_PRESETS];

void createPresetsWindow(lv_obj_t * parent) {
  //Styles first
  static lv_style_t style_ta;
  lv_style_init(&style_ta);
  lv_style_set_bg_color(&style_ta, lv_color_hex(0x606060));
  lv_style_set_bg_grad_color(&style_ta, lv_color_hex(0x606060));
  lv_style_set_bg_opa(&style_ta, LV_OPA_80);
  lv_style_set_radius(&style_ta, 3);
  lv_style_set_border_width(&style_ta, 3);
  lv_style_set_border_color(&style_ta, lv_color_hex(0xFF7F50));
  lv_style_set_text_color(&style_ta, lv_color_white());
  lv_style_set_text_font(&style_ta, &lv_font_montserrat_14);
  lv_style_set_pad_top(&style_ta, 6);
  lv_style_set_pad_bottom(&style_ta, 8);
  lv_style_set_pad_left(&style_ta, 8);
  lv_style_set_pad_right(&style_ta, 8);
  lv_style_init(&style_kb);
  lv_style_set_bg_color(&style_kb, lv_color_hex(0x707070));
  lv_style_set_bg_grad_color(&style_kb, lv_color_black());
  lv_style_set_bg_grad_dir(&style_kb, LV_GRAD_DIR_VER);
  lv_style_set_bg_opa(&style_kb, LV_OPA_70);
  lv_style_set_pad_top(&style_kb, 0);
  lv_style_set_pad_bottom(&style_kb, 0);
  lv_style_set_pad_left(&style_kb, 0);
  lv_style_set_pad_right(&style_kb, 0);
  lv_style_init(&style_kb_rel);
  lv_style_set_bg_color(&style_kb_rel, lv_color_hex3(0x333));
  lv_style_set_bg_grad_color(&style_kb_rel, lv_color_hex3(0x333));
  lv_style_set_radius(&style_kb_rel, 0);
  lv_style_set_border_width(&style_kb_rel, 1);
  lv_style_set_border_color(&style_kb_rel, lv_color_hex(0xC0C0C0));
  lv_style_set_border_opa(&style_kb_rel, LV_OPA_50);
  lv_style_set_text_color(&style_kb_rel, lv_color_white());
  lv_style_init(&style_kb_pr);
  lv_style_set_radius(&style_kb_pr, 0);
  lv_style_set_bg_opa(&style_kb_pr, LV_OPA_50);
  lv_style_set_bg_color(&style_kb_pr, lv_color_white());
  lv_style_set_bg_grad_color(&style_kb_pr, lv_color_white());
  lv_style_set_border_width(&style_kb_pr, 1);
  lv_style_set_border_color(&style_kb_pr, lv_color_hex(0xC0C0C0));

  //URL edit textbox
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  urlEditText = lv_textarea_create(parent);
  lv_obj_set_pos(urlEditText, 0, 10);
  lv_obj_add_style(urlEditText, &style_ta, LV_PART_MAIN);
  lv_textarea_set_text_selection(urlEditText, false);
  lv_textarea_set_one_line(urlEditText, true);
  lv_obj_set_size(urlEditText, lv_obj_get_content_width(parent), 36);
  lv_textarea_set_text(urlEditText, "");
  lv_obj_add_event_cb(urlEditText, editTextAction, LV_EVENT_PRESSED, NULL);

  //Playlist button
  playlistBtn = lv_btn_create(parent);
  lv_obj_set_pos(playlistBtn, 272, 10);
  lv_obj_set_size(playlistBtn, 40, 36);
  lv_obj_add_style(playlistBtn, &style_wp, LV_PART_MAIN);
  lv_obj_add_style(playlistBtn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(playlistBtn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_add_event_cb(playlistBtn, playlistBtnAction, LV_EVENT_CLICKED, NULL);
  lv_obj_t * label = lv_label_create(playlistBtn);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(label, LV_SYMBOL_LIST);
  lv_obj_set_hidden(playlistBtn, true);

  //Presets button "map"
  for (int ind = 0; ind < NUM_PRESETS; ind++) {
    presetButtons[ind] = lv_btn_create(parent);
    lv_obj_set_size(presetButtons[ind], (lv_obj_get_content_width(parent) - 20) / 5, 45);
    if (ind == 0) lv_obj_align_to(presetButtons[ind], urlEditText, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    else if (ind == 5 || ind == 10) lv_obj_align_to(presetButtons[ind], presetButtons[ind-5], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    else lv_obj_align_to(presetButtons[ind], presetButtons[ind-1], LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_add_style(presetButtons[ind], &style_wp, LV_PART_MAIN);
    lv_obj_set_user_data(presetButtons[ind], (void*)ind);
    lv_obj_add_event_cb(presetButtons[ind], presetClickAction, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(presetButtons[ind], presetLongAction, LV_EVENT_LONG_PRESSED, NULL);
    //lv_obj_add_event_cb(presetButtons, presetAction, LV_EVENT_VALUE_CHANGED, NULL);
    presetLabels[ind] = lv_label_create(presetButtons[ind]);
    lv_label_set_text(presetLabels[ind], settings.presets[ind].name);      
    lv_label_set_long_mode(presetLabels[ind], LV_LABEL_LONG_WRAP);
    lv_obj_set_width(presetLabels[ind], lv_obj_get_content_width(presetButtons[ind]));
    lv_obj_set_style_text_align(presetLabels[ind], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(presetLabels[ind], LV_ALIGN_CENTER, 0, 0);      
  }
}

void showPlaylistBtn(bool yesno) {
  if (urlEditText && playlistBtn) {
    if (yesno) {
      lv_obj_set_size(urlEditText, lv_obj_get_content_width(presetsWindow) - 45, 36);
      lv_obj_set_hidden(playlistBtn, false);  
    } else {
      lv_obj_set_size(urlEditText, lv_obj_get_content_width(presetsWindow), 36);
      lv_obj_set_hidden(playlistBtn, true);
    }
  }
}

//Playlist button clicked
static void playlistBtnAction(lv_event_t * event) {
  if (playlistWindow){
    showPresets(false);
    lv_obj_set_hidden(playlistWindow, false);
  }
}

void showPresets(bool yesno) {
  for (int ind = 0; ind < NUM_PRESETS; ind++) {
    lv_obj_set_hidden(presetButtons[ind], !yesno);
  }
  lv_obj_set_hidden(urlEditText, !yesno);
  lv_obj_set_hidden(playlistBtn, !yesno);    
}

//Read preset names from settings onto the buttons
void updatePresets() {
  for (int ind = 0; ind < NUM_PRESETS; ind++) {
    if (presetLabels[ind]) 
      lv_label_set_text(presetLabels[ind], settings.presets[ind].name);      
  }  
}

//Remember a long press
bool lastPresetLong = false;
void presetLongAction(lv_event_t * event) {
  if (!tabViewScrolling) lastPresetLong = true;
}

//Preset button was activated - save or load based on long press
void presetClickAction(lv_event_t * event) {
  lv_obj_t * btn = lv_event_get_target(event);
  int ind = (int)lv_obj_get_user_data(btn);
  if (!tabViewScrolling) {
    if (lastPresetLong) savePreset(ind);
    else loadPreset(ind);
  }
  lastPresetLong = false;
}

//Save out a preset to settings
void savePreset(uint16_t index) {
  settings.presets[index].mode = settings.mode;
  if (settings.mode == MODE_DAB) strncpy(settings.presets[index].name, settings.dabChannel, 34);
  else if (settings.mode == MODE_WEB) strncpy(settings.presets[index].name, webStationName, 34);
  else if (settings.mode == MODE_FM) {
    if (strlen(fmStationName)) sprintf(settings.presets[index].name, "%s FM %.1f", fmStationName, dabFrequency / 1000.0);
    else sprintf(settings.presets[index].name, "FM %.1f", dabFrequency / 1000.0);
  }
  serial.printf("> Save preset %d as %s: %s\r\n", index, modeString[settings.mode], settings.presets[index].name);
  writeSettings();
  updatePresets();
}

//Load in a preset from settings
void loadPreset(uint16_t index) {
  if (strcmp(settings.presets[index].name, "<Empty>") == 0) return;  //Empty..
  uint8_t mode = settings.presets[index].mode;
  char * data = settings.presets[index].name;
  serial.printf("> Load preset %d [%s]: %s\r\n", index, modeString[mode], data);
  if (mode == MODE_DAB) strncpy(settings.dabChannel, data, 34);
  else if (mode == MODE_WEB) strncpy(searchStationName, data, 34);
  else if (mode == MODE_FM) settings.dabFM = atof(strrchr(data, ' ')+1) * 1000.0;
  settings.mode = mode;
  lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_ON);
  lv_dropdown_set_selected(modeList, mode);      //Set the id of selected option
  writeSettings();
  setRadioMode();
}

//Show the keyboard when edit text is clicked
void editTextAction(lv_event_t * event) {
  if (keyBoard == NULL) {
    keyboardShow(urlEditText);
    lv_obj_add_event_cb(keyBoard, keyboardPresetKeyAction, LV_EVENT_ALL, NULL);
  }
}

//Change the text in the URL editor to suit mode
void updateUrlEditText() {
  if (urlEditText) {
    //Appearance
    lv_obj_t * label = lv_textarea_get_label(urlEditText);
    if (settings.mode == MODE_WEB) {    
      lv_obj_clear_state(urlEditText, LV_STATE_DISABLED);
      lv_obj_set_style_border_color(urlEditText, lv_color_hex(0xFF7F50), LV_PART_MAIN);
      lv_textarea_set_one_line(urlEditText, true);
      lv_obj_set_width(label, LV_SIZE_CONTENT);
      lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    } else {
      lv_obj_add_state(urlEditText, LV_STATE_DISABLED);
      lv_obj_set_style_border_color(urlEditText, lv_color_hex(0xC0C0C0), LV_PART_MAIN);
      lv_obj_set_width(label, lv_obj_get_content_width(urlEditText));
      lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL);
    }
    //Content
    char str[64] = "";
    if (settings.mode == MODE_FM) {
      sprintf(str, "FM://%.3f", settings.dabFM / 1000.0);
      lv_textarea_set_text(urlEditText, str);    
    } else if (settings.mode == MODE_DAB) {
      sprintf(str, "DAB://%s", settings.dabChannel);
      lv_textarea_set_text(urlEditText, str);    
    } else if (settings.mode == MODE_WEB) {
      lv_textarea_set_text(urlEditText, settings.server);  
      lv_obj_scroll_to(urlEditText, 0, 0, LV_ANIM_ON);
    } else if (settings.mode == MODE_SD) {
      lv_textarea_set_text(urlEditText, sdFileName);  
    } else if (settings.mode == MODE_LINE) {
      lv_textarea_set_text(urlEditText, "LINE://IN");  
    } else lv_textarea_set_text(urlEditText, "");
  }
}

//Keyboard OK or Cancel on URL edit
void keyboardPresetKeyAction(lv_event_t * event) {
  uint32_t res = lv_event_get_code(event);
  if(res == LV_EVENT_READY || res == LV_EVENT_CANCEL){
    keyboardHide(true);
    if(res == LV_EVENT_READY) {
      const char* url = lv_textarea_get_text(urlEditText);
      serial.print("> Connecting to: ");
      serial.println(url);
      if (strncmp(settings.server, url, 255) != 0) {
        strncpy(settings.server, url, 255);
        settings.server[255] = '\0';
        writeSettings();
      }  
      lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_ON);
      closeStream();
      connectToHost(settings.server);
    }
    else if (res == LV_EVENT_CANCEL){
      lv_textarea_set_text(urlEditText, settings.server);  
      lv_obj_scroll_to(urlEditText, 0, 0, LV_ANIM_ON);
    }
  }
}

//Bring up the keyboard to a given textarea
void keyboardShow(lv_obj_t * textarea) {
  if(keyBoard == NULL) {
    keyBoard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(keyBoard, lv_obj_get_content_width(lv_scr_act()), lv_obj_get_content_height(lv_scr_act()) / 2);
    lv_obj_align_to(keyBoard, textarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_set_x(keyBoard, 0);
    lv_obj_update_layout(keyBoard);
    lv_keyboard_set_textarea(keyBoard, textarea);
    lv_obj_add_style(keyBoard, &style_kb, LV_PART_MAIN | LV_PART_ITEMS);
    lv_obj_add_style(keyBoard, &style_kb_rel, LV_STATE_FOCUSED | LV_PART_ITEMS);
    lv_obj_add_style(keyBoard, &style_kb_pr, LV_STATE_PRESSED | LV_PART_ITEMS);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, keyBoard);
    lv_anim_set_values(&a, LV_VER_RES, lv_obj_get_y(keyBoard));
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_ready_cb(&a, keyboardShownAction);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_time(&a, 300);
    lv_anim_start(&a);
  }
}

//Ensure keyboard is fully on screen by scrolling the parent
void keyboardShownAction(lv_anim_t * a) {
  lv_obj_scroll_to_view(keyBoard, LV_ANIM_ON);
}

//Slide the keyboard back off the screen
void keyboardHide(bool animated) {
  if (keyBoard == NULL) return;    //already hidden
  if (animated) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, keyBoard);
    lv_anim_set_values(&a, lv_obj_get_y(keyBoard), LV_VER_RES);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_ready_cb(&a, keyboardHiddenAction);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_time(&a, 300);
    lv_anim_start(&a);
  } else {
    lv_obj_del(keyBoard);
    keyBoard = NULL;
  }
}

//Keyboard is gone, destroy it
void keyboardHiddenAction(lv_anim_t * a) {
  lv_obj_del(keyBoard);
  keyBoard = NULL;
}

//=======================================================================================
//Playlist window

static lv_obj_t * playlistMainList;
static lv_obj_t * playlistHeaderLabel;

void createPlaylistWindow(lv_obj_t * parent) {
  playlistWindow = lv_win_create(parent, 28);
  playlistHeaderLabel = lv_win_add_title(playlistWindow, "Playlist");
  lv_obj_set_size(playlistWindow, lv_obj_get_content_width(parent), lv_obj_get_content_height(parent));
  lv_obj_set_pos(playlistWindow, 0, 0);
  lv_obj_add_style(playlistWindow, &style_win, LV_PART_MAIN);
  lv_obj_t * win_content = lv_win_get_content(playlistWindow);
  lv_obj_add_style(win_content, &style_win, LV_PART_MAIN);
  lv_obj_set_hidden(playlistWindow, true);
  
  lv_obj_t * reload_btn = lv_win_add_btn(playlistWindow, LV_SYMBOL_REFRESH, 33);           /*Add close button and use built-in close action*/
  lv_obj_add_style(reload_btn, &style_wp, LV_PART_MAIN);  
  lv_obj_add_style(reload_btn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(reload_btn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_add_event_cb(reload_btn, playlistWindow_reload_action, LV_EVENT_CLICKED, NULL);

  lv_obj_t * clear_btn = lv_win_add_btn(playlistWindow, LV_SYMBOL_TRASH, 33);           /*Add close button and use built-in close action*/
  lv_obj_add_style(clear_btn, &style_wp, LV_PART_MAIN);  
  lv_obj_add_style(clear_btn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(clear_btn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_add_event_cb(clear_btn, playlistWindow_clear_action, LV_EVENT_CLICKED, NULL);

  lv_obj_t * shuffle_btn = lv_win_add_btn(playlistWindow, LV_SYMBOL_SHUFFLE, 33);           /*Add close button and use built-in close action*/
  lv_obj_add_style(shuffle_btn, &style_wp, LV_PART_MAIN);  
  lv_obj_add_style(shuffle_btn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(shuffle_btn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_add_event_cb(shuffle_btn, playlistWindow_shuffle_action, LV_EVENT_CLICKED, NULL);

  lv_obj_t * close_btn = lv_win_add_btn(playlistWindow, LV_SYMBOL_CLOSE, 33);           /*Add close button and use built-in close action*/
  lv_obj_add_style(close_btn, &style_wp, LV_PART_MAIN);  
  lv_obj_add_style(close_btn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(close_btn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_add_event_cb(close_btn, playlistWindow_close_action, LV_EVENT_CLICKED, NULL);

  playlistMainList = lv_list_create(win_content);
  lv_obj_set_style_bg_opa(playlistMainList, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_update_layout(win_content);
  lv_obj_set_size(playlistMainList, lv_obj_get_content_width(win_content), lv_obj_get_content_height(win_content));
  lv_obj_set_pos(playlistMainList, 0, 0);
  //lv_obj_set_scrollbar_mode(dabStationList, LV_SCROLLBAR_MODE_OFF);
}


static void playlistWindow_close_action(lv_event_t * event) {
  lv_obj_set_hidden(playlistWindow, true);
  showPresets(true);
}

static void playlistWindow_clear_action(lv_event_t * event) {
  sdStop();
  clearPlaylist();
  settings.playlistIndex = 0;
  writeSettings();
}

static void playlistWindow_shuffle_action(lv_event_t * event) {
  serial.println("> Shuffling playlist..");
  srand(millis());
  int N = lv_obj_get_child_cnt(playlistMainList);
  for (int i = 0; i < N-1; ++i)
  {
    lv_obj_t* btn1 = lv_obj_get_child(playlistMainList, i);
    int j = rand() % (N-i) + i;
    lv_obj_t* btn2 = lv_obj_get_child(playlistMainList, j);
    lv_obj_swap(btn1, btn2);
  }
  writePlaylist();
  settings.playlistIndex = 1;
  writeSettings();
  playPlaylistFile();    
}

static void playlistWindow_reload_action(lv_event_t * event) {
  settings.playlistIndex = 1;
  loadPlaylist(PLAYLIST_PATH);
  writeSettings();
  playPlaylistFile();    
}

void updatePlaylistHeader() {
  char str[64];
  sprintf(str, "Playlist (%d of %d)", settings.playlistIndex, (int)lv_obj_get_child_cnt(playlistMainList));
  lv_label_set_text(playlistHeaderLabel, str);  
}

//Clear playlist and also free the entry's attached userdata
void clearPlaylist() {
  for (uint16_t i = 0; i < lv_obj_get_child_cnt(playlistMainList); i++) {
    lv_obj_t* child = lv_obj_get_child(playlistMainList, i);
    void * data = lv_obj_get_user_data(child);
    if (data) {
      free(data);
      data = NULL;
    }
  }
  lv_obj_clean(playlistMainList);
}

//Add an entry to the station list
lv_obj_t * addPlaylistButton(const char* text, void* data, int datalen) {
  lv_obj_t * list_btn;
  if (!playlistMainList) return NULL;
  list_btn = lv_list_add_btn(playlistMainList, LV_SYMBOL_AUDIO, text);
  lv_obj_add_event_cb(list_btn, playlistAction, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(list_btn, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_add_style(list_btn, &style_halfopa, LV_PART_MAIN);
  lv_obj_add_style(list_btn, &style_listsel, LV_STATE_CHECKED);
  if (data) {
    void* ptr = malloc(datalen);
    memcpy(ptr, data, datalen);
    lv_obj_set_user_data(list_btn, ptr);
  }
  return list_btn;
}

void loadPlaylist(const char * file) {
  serial.println("> Reading playlist..");
  lv_fs_file_t f;
  if (fs_err(lv_fs_open(&f, file, LV_FS_MODE_RD), "> Error opening playlist!")) return;
  int lbi = 0;
  char linebuf[256];
  char name[35];
  int count = 0;
  lv_obj_t * current = NULL;
  uint32_t read_num;
  char ch;
  clearPlaylist();
  while (1) {
    lv_refr_now(display);
    if (fs_err(lv_fs_read(&f, &ch, 1, &read_num), "> Error reading playlist!")) return;
    if (read_num == 0) break;
    if (ch != '\r' && ch != '\n' ) linebuf[lbi++] = ch;
    if (ch == '\n' || lbi == 255) {
      linebuf[lbi] = 0;
      if (linebuf[0] == '#') {
        char * ptr = strchr(linebuf, ',');
        if (ptr) strncpy(name, ptr+1, 34);
        else strncpy(name, &linebuf[1], 34);
        name[34] = '\0';
      } else {
        lv_obj_t * list_btn = addPlaylistButton(name, linebuf, lbi+1);
        if (count++ == settings.playlistIndex - 1) current = list_btn;
      }
      lbi = 0;
    }
  }
  if (current) listSetSelect(current);
  lv_fs_close(&f);
  serial.println("> Read playlist OK");
}

void writePlaylist() {
  lv_fs_file_t f;
  char str[260];
  uint32_t byteswrote;
  serial.println("> Rewriting playlist..");
  //Build playlist
  removePlaylist();
  if (fs_err(lv_fs_open(&f, PLAYLIST_PATH, LV_FS_MODE_WR), "Open Playlist")) return;
  for (uint16_t i = 0; i < lv_obj_get_child_cnt(playlistMainList); i++) {
    lv_obj_t* child = lv_obj_get_child(playlistMainList, i);
    const char * name = lv_list_get_btn_text(playlistMainList, child);
    sprintf(str, "#%s\r\n", name);
    if (fs_err(lv_fs_write(&f, str, strlen(str), &byteswrote), "Write Playlist Header")) return;
    const char * path = (const char*)lv_obj_get_user_data(child);
    if (path) {
      sprintf(str, "%s\r\n", path);
      if (fs_err(lv_fs_write(&f, str, strlen(str), &byteswrote), "Write Playlist Entry")) return;
    }
  }  
  lv_fs_close(&f);
}

void playlistAction(lv_event_t * event) {
  lv_obj_t * child = lv_event_get_target(event);
  int index = lv_obj_get_child_id(child);
  listSetSelect(child);
  settings.playlistIndex = index+1;
  writeSettings();
  playPlaylistFile();
}


//=======================================================================================
//---------------------------------------------------
//Main window

lv_obj_t * bufLvlMeter;
lv_meter_indicator_t * bufLvlIndicator;
lv_obj_t * bufStatLbl;
lv_obj_t * spectrumChart;
lv_obj_t * infoContainer;
lv_obj_t * seekDownBtn;
lv_obj_t * seekUpBtn;
lv_obj_t * reloadBtn;
lv_obj_t * loadSpinner;
int specGraphPoints = 14;
lv_chart_series_t * spectrum;
lv_chart_series_t * specPeak;

//Main (middle) info window
void createMainWindow(lv_obj_t * mainWindow) {    

  //---- Info Container ----------------
  
  //The main info container window
  infoContainer = lv_obj_create(mainWindow);
  //lv_cont_set_fit(infoContainer, LV_FIT_NONE);              //Fit the size to the content
  lv_obj_set_size(infoContainer, 308, 86);
  lv_obj_set_pos(infoContainer, 4, 8);
  lv_obj_add_style(infoContainer, &style_groupbox, LV_PART_MAIN);
  lv_obj_clear_flag(infoContainer, LV_OBJ_FLAG_SCROLLABLE);
  //lv_obj_set_click(infoContainer, false);            //Allow page scroll
  lv_obj_set_hidden(infoContainer, true);

  //DAB+ Logo
  dab_img = lv_img_create(infoContainer);
  lv_obj_remove_style_all(dab_img);
  lv_obj_set_pos(dab_img, 4, 5);  
  lv_obj_set_size(dab_img, 80, 60);
  lv_img_set_src(dab_img, &dabLogo);
  //lv_obj_set_hidden(dab_img, true);

  static lv_style_t style_bar;
  lv_style_init(&style_bar);
  lv_style_set_bg_color(&style_bar, lv_color_hex(0xFF6025));
  //Slideshow progress bar
  dab_img_bar = lv_bar_create(infoContainer);
  lv_obj_set_size(dab_img_bar, 80, 4);
  lv_obj_align_to(dab_img_bar, dab_img, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
  //lv_bar_set_anim_time(dab_img_bar, 500);
  lv_bar_set_value(dab_img_bar, 0, LV_ANIM_OFF);
  lv_obj_add_style(dab_img_bar, &style_bar, LV_PART_INDICATOR);
  lv_obj_set_hidden(dab_img_bar, true);

  //Create a style for the buffer level meter
  static lv_style_t style_lmeter;
  lv_style_init(&style_lmeter);
  lv_style_set_bg_opa(&style_lmeter, LV_OPA_0);
  lv_style_set_border_opa(&style_lmeter, LV_OPA_0);

  //Create a buffer level line meter 
  bufLvlMeter = lv_meter_create(infoContainer);
  lv_meter_scale_t * scale = lv_meter_add_scale(bufLvlMeter);
  lv_meter_set_scale_ticks(bufLvlMeter, scale, 31, 2, 8, lv_color_hex(0x505050));
  lv_meter_set_scale_range(bufLvlMeter, scale, 0, 100, 240, 150);
  bufLvlIndicator = lv_meter_add_scale_lines(bufLvlMeter, scale, lv_color_hex(0x04386c), lv_color_hex(0x91bfed), false, 0);
  lv_meter_set_indicator_start_value(bufLvlMeter, bufLvlIndicator, 0);
  lv_meter_set_indicator_end_value(bufLvlMeter, bufLvlIndicator, 0);                       //Set the current value
  lv_obj_add_style(bufLvlMeter, &style_lmeter, LV_PART_MAIN);
  lv_obj_add_style(bufLvlMeter, &style_lmeter, LV_PART_INDICATOR);
  lv_obj_set_pos(bufLvlMeter, 0, 0);                        //Set the x coordinate
  lv_obj_set_size(bufLvlMeter, 86, 86);
  lv_obj_set_hidden(bufLvlMeter, true);

  //Buffer status label
  bufStatLbl = lv_label_create(infoContainer); //First parameters (scr) is the parent
  lv_label_set_text(bufStatLbl, "\n\n");  //Set the text
  lv_obj_align_to(bufStatLbl, bufLvlMeter, LV_ALIGN_CENTER, 2, 0);  //Align below the label
  lv_obj_set_style_text_align(bufStatLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_hidden(bufStatLbl, true);

  static lv_style_t style_biggerfont;
  lv_style_init(&style_biggerfont);
  lv_style_set_text_font(&style_biggerfont, &lv_font_montserrat_20);

  static lv_style_t style_digitalfont;
  lv_style_init(&style_digitalfont);
  lv_style_set_text_color(&style_digitalfont, lv_color_hex(0xFF0000));
  lv_style_set_text_font(&style_digitalfont, &digital);


  //Info labels
  progNameLbl = lv_label_create(infoContainer); //First parameters (scr) is the parent
  lv_label_set_long_mode(progNameLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_size(progNameLbl, 196, LV_SIZE_CONTENT);
  lv_obj_add_style(progNameLbl, &style_biggerfont, LV_PART_MAIN);
  lv_label_set_text(progNameLbl, "");  //Set the text
  lv_obj_align_to(progNameLbl, dab_img, LV_ALIGN_OUT_RIGHT_TOP, 12, 0);  //Align below the label

  //Reload button
  reloadBtn = lv_btn_create(infoContainer);
  lv_obj_set_size(reloadBtn, 30, 26);
  lv_obj_add_style(reloadBtn, &style_wp, LV_PART_MAIN);
  lv_obj_add_style(reloadBtn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(reloadBtn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_align_to(reloadBtn, dab_img, LV_ALIGN_OUT_RIGHT_TOP, 173, 0);
  lv_obj_add_event_cb(reloadBtn, reload_action, LV_EVENT_CLICKED, NULL);
  lv_obj_t * label = lv_label_create(reloadBtn);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(label, LV_SYMBOL_REFRESH);
  lv_obj_set_hidden(reloadBtn, true);

  //Loading spinner
  loadSpinner = lv_spinner_create(infoContainer, 1000, 60);
  lv_obj_set_size(loadSpinner, 35, 35);
  lv_obj_align_to(loadSpinner, dab_img, LV_ALIGN_OUT_RIGHT_TOP, 173, -6);
  lv_obj_set_hidden(loadSpinner, true);

  progNowLbl = lv_label_create(infoContainer); //First parameters (scr) is the parent
  lv_obj_set_size(progNowLbl, 196, LV_SIZE_CONTENT);
  lv_obj_add_style(progNowLbl, &style_bigfont, LV_PART_MAIN);
  lv_label_set_long_mode(progNowLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_align_to(progNowLbl, progNameLbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);  //Align below the label
  lv_label_set_text(progNowLbl, "");  //Set the text
  
  progTimeBar = lv_bar_create(infoContainer);
  lv_obj_set_size(progTimeBar, 196, 12);
  lv_obj_align_to(progTimeBar, progNowLbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  //Align below the label
  lv_bar_set_range(progTimeBar, 0, 100);
  lv_bar_set_value(progTimeBar, 0, LV_ANIM_OFF);
  lv_obj_set_hidden(progTimeBar, true);
  
  progTextLbl = lv_label_create(infoContainer); //First parameters (scr) is the parent
  lv_obj_set_size(progTextLbl, 196, LV_SIZE_CONTENT);
  lv_obj_add_style(progTextLbl, &style_bigfont, LV_PART_MAIN);
  lv_label_set_long_mode(progTextLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_align_to(progTextLbl, progNowLbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);  //Align below the label
  lv_label_set_text(progTextLbl, "");  //Set the text

  fmFreqLbl = lv_label_create(infoContainer); //First parameters (scr) is the parent
  lv_obj_set_pos(fmFreqLbl, -6, 28);  
  lv_obj_set_size(fmFreqLbl, 96, 32);
  lv_label_set_text(fmFreqLbl, "");  //Set the text
  lv_label_set_long_mode(fmFreqLbl, LV_LABEL_LONG_CLIP);
  lv_obj_add_style(fmFreqLbl, &style_digitalfont, LV_PART_MAIN);
  lv_obj_set_hidden(fmFreqLbl, true);

  //---- Page body controls ----------------
  
  //Create a seek button
  seekDownBtn = lv_btn_create(mainWindow);
  lv_obj_set_size(seekDownBtn, 40, 30);   //Set the size
  lv_obj_add_style(seekDownBtn, &style_wp, LV_PART_MAIN);
  lv_obj_add_style(seekDownBtn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(seekDownBtn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_align_to(seekDownBtn, infoContainer, LV_ALIGN_OUT_BOTTOM_LEFT, 20, 20);         //Align next to the slider
  lv_obj_add_event_cb(seekDownBtn, seekDownAction, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(seekDownBtn);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(label, LV_SYMBOL_PREV);
  lv_obj_set_hidden(seekDownBtn, true);

  //Create another seek button
  seekUpBtn = lv_btn_create(mainWindow);
  lv_obj_set_size(seekUpBtn, 40, 30);   //Set the size
  lv_obj_add_style(seekUpBtn, &style_wp, LV_PART_MAIN);
  lv_obj_add_style(seekUpBtn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(seekUpBtn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_align_to(seekUpBtn, seekDownBtn, LV_ALIGN_OUT_RIGHT_MID, 188, 0);         //Align next to the slider
  lv_obj_add_event_cb(seekUpBtn, seekUpAction, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(seekUpBtn);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(label, LV_SYMBOL_NEXT);
  lv_obj_set_hidden(seekUpBtn, true);

  static lv_style_t style_speclines;
  lv_style_init(&style_speclines);
  lv_style_set_line_width(&style_speclines, 6);
  lv_style_set_width(&style_speclines, 5);
  lv_style_set_height(&style_speclines, 5);

  //Spectrum analyser
  spectrumChart = lv_chart_create(mainWindow);                         //Create the chart
  lv_obj_set_size(spectrumChart, 308, 90);   //Set the size
  lv_obj_align_to(spectrumChart, infoContainer, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);                   //Align below the slider
  specPeak = lv_chart_add_series(spectrumChart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_ext_y_array(spectrumChart, specPeak, (lv_coord_t*)spectrumPeakValues);
  spectrum = lv_chart_add_series(spectrumChart, lv_color_hex(0x4070C0), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_ext_y_array(spectrumChart, spectrum, (lv_coord_t*)spectrumValues);
  lv_chart_set_type(spectrumChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(spectrumChart, specGraphPoints);
  lv_chart_set_range(spectrumChart, LV_CHART_AXIS_PRIMARY_Y, 0, 31);
  lv_chart_set_div_line_count(spectrumChart, 0, 0);
  lv_obj_add_style(spectrumChart, &style_groupbox, LV_PART_MAIN);           //Apply the new style
  lv_obj_add_style(spectrumChart, &style_speclines, LV_PART_ITEMS);           //Apply the new style
  lv_obj_add_style(spectrumChart, &style_speclines, LV_PART_INDICATOR);           //Apply the new style
  lv_obj_set_hidden(spectrumChart, true);
}

//Reload button clicked
static void reload_action(lv_event_t * event) {
  openStream();
}


//bodge..easier migration from lvgl 6 to 8
void lv_obj_set_hidden(lv_obj_t * obj, bool isHidden) {
  if (obj == NULL) return;
  if (isHidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

//Put up an animated spinner while waiting
void showLoadSpinner() {
  if (progNameLbl) lv_obj_set_size(progNameLbl, 154, 25);
  lv_obj_set_hidden(reloadBtn, true);
  lv_obj_set_hidden(loadSpinner, false);
}

// Show the reload button if connection issues
void showReloadBtn() {
  if (progNameLbl) lv_obj_set_size(progNameLbl, 154, 25);
  lv_obj_set_hidden(loadSpinner, true);
  lv_obj_set_hidden(reloadBtn, false);
}
    
//Hide the spinner and reload buttons
void hideWebControls() {
  if (progNameLbl) lv_obj_set_size(progNameLbl, 196, 25);
  lv_obj_set_hidden(loadSpinner, true);
  lv_obj_set_hidden(reloadBtn, true);
}

//Object visibility matrix - shows/hides the widgets to suit the mode and function
uint8_t oldMainWindowIndex = 4; //we started on the terminal
void setObjectVisibility() {
  //tabView
  bool allHidden = (currentFunction != FUNC_TA);
  bool stationsHidden = (currentFunction != FUNC_TA || settings.mode == MODE_FM || settings.mode == MODE_LINE);
  lv_obj_set_hidden(settingsWindow, allHidden);
  lv_obj_set_hidden(presetsWindow, allHidden);
  lv_obj_set_hidden(stationsWindow, stationsHidden);
  mainWindowIndex = allHidden?0:stationsHidden?1:2;
  if (oldMainWindowIndex != mainWindowIndex) {
    oldMainWindowIndex = mainWindowIndex;
    lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_OFF);              //Start on the terminal
  }  
  //main window
  lv_obj_set_hidden(modeList, currentFunction != FUNC_TA);
  lv_obj_set_hidden(functionLbl, currentFunction == FUNC_TA);
  lv_obj_set_hidden(infoContainer, currentFunction != FUNC_TA || settings.mode == MODE_LINE);
  lv_obj_set_hidden(dab_img, currentFunction != FUNC_TA || settings.mode != MODE_DAB);
  lv_obj_set_hidden(dab_img_bar, currentFunction != FUNC_TA || settings.mode != MODE_DAB);
  lv_obj_set_hidden(bufLvlMeter, currentFunction != FUNC_TA || (settings.mode != MODE_WEB && settings.mode != MODE_SD));
  lv_obj_set_hidden(bufStatLbl, currentFunction != FUNC_TA || (settings.mode != MODE_WEB && settings.mode != MODE_SD));
  lv_obj_set_hidden(fmFreqLbl, currentFunction != FUNC_TA || settings.mode != MODE_FM);
  lv_obj_set_hidden(seekDownBtn, currentFunction != FUNC_TA || settings.mode != MODE_FM);
  lv_obj_set_hidden(seekUpBtn, currentFunction != FUNC_TA || settings.mode != MODE_FM);
  lv_obj_set_hidden(progTimeBar, currentFunction != FUNC_TA || settings.mode != MODE_SD);  
  lv_obj_set_hidden(progTextLbl, currentFunction != FUNC_TA || settings.mode == MODE_SD);  
  lv_obj_set_hidden(spectrumChart, vsUsingFlac || currentFunction != FUNC_TA || (settings.mode != MODE_WEB && settings.mode != MODE_SD));
  //stations window
  lv_obj_set_hidden(dabStationList, currentFunction != FUNC_TA || (settings.mode != MODE_DAB && settings.mode != MODE_WEB));
  lv_obj_set_hidden(browserWin, currentFunction != FUNC_TA || settings.mode != MODE_SD);  
  //settings window
  lv_obj_set_hidden(wifiContainer, currentFunction != FUNC_TA || settings.mode != MODE_WEB);
  lv_obj_set_hidden(dabContainer, currentFunction != FUNC_TA || settings.mode != MODE_DAB);
  lv_obj_set_hidden(vsContainer, currentFunction != FUNC_TA || (settings.mode != MODE_WEB && settings.mode != MODE_SD));
  if (vsContainer) {
    if (settings.mode == MODE_WEB) lv_obj_align_to(vsContainer, wifiContainer, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);         //Align next to the slider
    else lv_obj_align_to(vsContainer, mainContainer, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
  }
  //Presets window
  lv_obj_set_hidden(playlistWindow, true);      //Always hide the playlist
  showPresets(true);                            // and bring up the presets
  showPlaylistBtn(currentFunction == FUNC_TA && settings.mode == MODE_SD);
}

//Main mode change entry - clears all state and resets peripherals as required
// Sets up and starts function and mode based on current settings
void setRadioMode() {
  serial.print("> Set function: ");
  serial.print(functionString[currentFunction]);
  if (currentFunction == FUNC_TA) {    
    serial.print(", mode: ");
    serial.println(modeString[settings.mode]);
  } else serial.println("");
  //Clear state and stop any current activities
  clearProgLbl();
  setObjectVisibility();
  clearStations();
  updateSTMOLabel(STMO_NONE);
  closeStream();
  dabStop();
  sdStop();
  //Switch to new function
  if (currentFunction == FUNC_TA) {    
    //Switch to new mode
    updateUrlEditText();
    initDab();          //Reset MOT slideshow
    if (settings.mode == MODE_DAB) startDab();
    else if (settings.mode == MODE_FM) startFM();
    else if (settings.mode == MODE_WEB) {
      //Problem with the MP3 decoder? try resolve it
      if (!VSFound && !(VSFound = vs.begin())) {
        errorContinue("VS1053 MP3 Codec not responding!");        
      } else if ((WLANFound = resetWLAN())) {// || (WLANFound = resetWLAN())) {
        serial.println("> ESP8266 WiFi Found.");
        useFlacDecoder(false);  //Get spectrum back
        readWebStations();
        wlanConnect();
        if (!strlen(settings.server))
          strcpy(settings.server, "http://radio1.internode.on.net:8000/296");
        connectToHost(settings.server); 
      } else {
        if (progTextLbl) lv_label_set_text(progTextLbl, " " LV_SYMBOL_WARNING " ESP8266 Module Fail!");
        serial.println("> ESP8266 Not Detected!");
      }
    }
    else if (settings.mode == MODE_SD) {
      if (!VSFound && !(VSFound = vs.begin())) {
        errorContinue("VS1053 MP3 Codec not responding!");            
      }
      if (progNameLbl) lv_label_set_text(progNameLbl, LV_SYMBOL_STOP " Stopped");
      if (progNowLbl) lv_label_set_text(progNowLbl, LV_SYMBOL_LEFT " Choose files from the list to the left.. ");
      resumePlaylist();
    }
    else if (settings.mode == MODE_LINE) startVSbypass(DAB_LINEIN2);
  } else if (currentFunction == FUNC_FM) {
    setSigStrengthLbl("");
    realignTopLabels();
    if (functionLbl) lv_label_set_text(functionLbl, LV_SYMBOL_AUDIO " Analog FM Radio");
  } else if (currentFunction == FUNC_AM) {
    setSigStrengthLbl("");
    realignTopLabels();
    if (functionLbl) lv_label_set_text(functionLbl, LV_SYMBOL_AUDIO " Analog AM Radio");
  } else if (currentFunction == FUNC_PB) {
    setSigStrengthLbl("");
    if (functionLbl) lv_label_set_text(functionLbl, LV_SYMBOL_PLAY " Cassette Playback");
  }
}

void clearProgLbl() {
  if (progNameLbl) lv_label_set_text(progNameLbl, "");
  if (progTextLbl) lv_label_set_text(progTextLbl, "");
  if (progNowLbl) lv_label_set_text(progNowLbl, "");  
}

//Mode dropdown selection made
void dabModeAction(lv_event_t * event) {
  uint16_t opt = lv_dropdown_get_selected(lv_event_get_target(event));      //Get the id of selected option
  if (opt != settings.mode) {
    settings.mode = opt; 
    setRadioMode();
    writeSettings();
  }
}

//Digital FM seek button actions
static void seekDownAction(lv_event_t * event) {
  dabFMSearch(0);
}
static void seekUpAction(lv_event_t * event) {
  dabFMSearch(1);
}



//==========================================================================
// Debug terminal

#define TERMINAL_LOG_LENGTH  2047        //Characters
static char txt_log[TERMINAL_LOG_LENGTH + 1];
lv_obj_t * term_label;

static void createTerminalWindow(lv_obj_t *win) {
  static lv_style_t style_bg;
  lv_style_init(&style_bg);
  lv_style_set_bg_color(&style_bg, lv_color_make(0x30, 0x30, 0x30));
  lv_style_set_bg_grad_color(&style_bg, lv_color_black());
  lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
  lv_obj_add_style(win, &style_bg, LV_PART_MAIN);
  
  static lv_style_t style1;
  lv_style_init(&style1);
  lv_style_set_text_color(&style1, lv_color_white());    //Make the window's content responsive
  //Create a label for the text of the terminal
  term_label = lv_label_create(win);
  lv_label_set_long_mode(term_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(term_label, 0, 0);
  lv_obj_set_width(term_label, lv_obj_get_content_width(win));
  lv_obj_set_height(term_label, LV_SIZE_CONTENT);
  lv_label_set_text_static(term_label, txt_log);               //Use the text array directly
  lv_obj_add_style(term_label, &style1, LV_PART_MAIN);
}

//Keeps the terminal buffer at less than TERMINAL_LOG_LENGTH
//Returns index of insertion point in txt_log
int trimTerminalBuffer(int newTextLength) {
  int bufferLength = strlen(txt_log);
  //If the text become too long 'forget' the oldest lines
  if(bufferLength + newTextLength > TERMINAL_LOG_LENGTH) {
    uint16_t new_start;
    for(new_start = 0; new_start < bufferLength; new_start++) {
      if(txt_log[new_start] == '\n') {
        //If there is enough space break
        if(new_start >= newTextLength) {
          //Ignore line breaks
          while(txt_log[new_start] == '\n' || txt_log[new_start] == '\r') new_start++;
          break;
        }
      }
    }

    // If it wasn't able to make enough space on line breaks
    // simply forget the oldest characters
    if(new_start == bufferLength) {
      new_start = bufferLength - (TERMINAL_LOG_LENGTH - newTextLength);
    }
    //Move the remaining text to the beginning
    uint16_t j;
    for(j = new_start; j < bufferLength; j++) {
      txt_log[j - new_start] = txt_log[j];
    }
    bufferLength = bufferLength - new_start;
    txt_log[bufferLength] = '\0';
  }
  return bufferLength;
}

//Update the cached terminal contents from the main loop
bool invalidateTerminal = false;
void terminalHandle() {
  if (term_label && invalidateTerminal) {
    lv_label_set_text_static(term_label, txt_log);
    //Scroll to end
    lv_obj_update_layout(terminalWindow);
    lv_obj_scroll_to_y(terminalWindow, TERMINAL_LOG_LENGTH, LV_ANIM_OFF);
    invalidateTerminal = false;
  }
}

//Custom serial class also prints to the terminal
//mySerial declared in config.h
size_t mySerial::write(uint8_t ch) {
  if (ch >= 0) {
    Serial.write(ch);
    if (ch != '\r') {
      int insert_at = trimTerminalBuffer(1);
      txt_log[insert_at] = ch;
      txt_log[insert_at + 1] = '\0';
      invalidateTerminal = true;
      if (InSetup && ch == '\n') {
        terminalHandle();
        lv_task_handler();  
      }
    }
  }
  return 1;
}

//=============================================================================================
//System monitor

#define CPU_LABEL_COLOR     "FF0000"
#define MEM_LABEL_COLOR     "007FFF"
#define PERF_CHART_POINT_NUM     100

#define VOLT_LABEL_COLOR     "FF0000"
#define AMP_LABEL_COLOR     "007FFF"
#define BATT_CHART_POINT_NUM     100

#define REFR_TIME    500

static lv_obj_t * perf_chart;
static lv_chart_series_t * cpu_ser;
static lv_chart_series_t * mem_ser;
static lv_obj_t * info_label;
static lv_obj_t * cpu_label;
static lv_obj_t * mem_label;
static lv_obj_t * batt_chart;
static lv_chart_series_t * volt_ser;
static lv_chart_series_t * amp_ser;
static lv_obj_t * batt_label;
static lv_obj_t * volt_label;
static lv_obj_t * amp_label;
static lv_timer_t * sysmonTimer;

void createSysmonWindow(lv_obj_t *scr) {
  lv_coord_t hres = lv_obj_get_content_width(scr);
  lv_coord_t vres = lv_obj_get_content_height(scr);
  sysmonTimer = lv_timer_create(sysmonTimerAction, REFR_TIME, NULL);

  static lv_style_t style_speclines;
  lv_style_init(&style_speclines);
  lv_style_set_line_width(&style_speclines, 4);
  lv_style_set_width(&style_speclines, 3);
  lv_style_set_height(&style_speclines, 3);

  //Create a battery chart with two data lines
  batt_chart = lv_chart_create(scr);
  lv_obj_set_size(batt_chart, hres - 20, vres / 2);
  lv_obj_set_pos(batt_chart, 10, 10);
  //lv_obj_set_click(chart, false);
  lv_chart_set_point_count(batt_chart, BATT_CHART_POINT_NUM);
  lv_chart_set_range(batt_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_type(batt_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(batt_chart, 0, 0);
  lv_obj_add_style(batt_chart, &style_groupbox, LV_PART_MAIN);           //Apply the new style
  lv_obj_add_style(batt_chart, &style_speclines, LV_PART_ITEMS);           //Apply the new style
  lv_obj_add_style(batt_chart, &style_speclines, LV_PART_INDICATOR);           //Apply the new style
  volt_ser =  lv_chart_add_series(batt_chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
  amp_ser =  lv_chart_add_series(batt_chart, lv_color_hex(0x007FFF), LV_CHART_AXIS_PRIMARY_Y);

  uint16_t i;
  for(i = 0; i < BATT_CHART_POINT_NUM; i++) {
      lv_chart_set_next_value(batt_chart, volt_ser, 0);
      lv_chart_set_next_value(batt_chart, amp_ser, 0);
  }

  //Create a label for the details of Memory and CPU usage
  volt_label = lv_label_create(scr);
  lv_label_set_recolor(volt_label, true);
  lv_obj_add_style(volt_label, &style_bigfont, LV_PART_MAIN);           //Apply the new style
  lv_obj_align_to(volt_label, batt_chart, LV_ALIGN_TOP_LEFT, 5, 5);

  //Create a label for the details of Memory and CPU usage
  amp_label = lv_label_create(scr);
  lv_label_set_recolor(amp_label, true);
  lv_obj_add_style(amp_label, &style_bigfont, LV_PART_MAIN);           //Apply the new style
  lv_obj_align_to(amp_label, batt_chart, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  
  //Create a label for the details of Memory and CPU usage
  batt_label = lv_label_create(scr);
  lv_label_set_recolor(batt_label, true);
  lv_obj_add_style(batt_label, &style_bigfont, LV_PART_MAIN);           //Apply the new style
  lv_obj_align_to(batt_label, batt_chart, LV_ALIGN_OUT_BOTTOM_LEFT, 4, 4);

  //Create a chart with two data lines
  perf_chart = lv_chart_create(scr);
  lv_obj_set_size(perf_chart, hres - 20, vres / 2);
  lv_obj_align_to(perf_chart, batt_label, LV_ALIGN_OUT_BOTTOM_LEFT, -4, 20);
  //lv_obj_set_click(chart, false);
  lv_chart_set_point_count(perf_chart, PERF_CHART_POINT_NUM);
  lv_chart_set_range(perf_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_type(perf_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(perf_chart, 0, 0);
  lv_obj_add_style(perf_chart, &style_groupbox, LV_PART_MAIN);           //Apply the new style
  lv_obj_add_style(perf_chart, &style_speclines, LV_PART_ITEMS);           //Apply the new style
  lv_obj_add_style(perf_chart, &style_speclines, LV_PART_INDICATOR);           //Apply the new style
  cpu_ser =  lv_chart_add_series(perf_chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
  mem_ser =  lv_chart_add_series(perf_chart, lv_color_hex(0x007FFF), LV_CHART_AXIS_PRIMARY_Y);

  for(i = 0; i < PERF_CHART_POINT_NUM; i++) {
      lv_chart_set_next_value(perf_chart, cpu_ser, 0);
      lv_chart_set_next_value(perf_chart, mem_ser, 0);
  }

  //Create a label for the details of Memory and CPU usage
  cpu_label = lv_label_create(scr);
  lv_label_set_recolor(cpu_label, true);
  lv_obj_add_style(cpu_label, &style_bigfont, LV_PART_MAIN);           //Apply the new style
  lv_obj_align_to(cpu_label, perf_chart, LV_ALIGN_TOP_LEFT, 5, 5);

  //Create a label for the details of Memory and CPU usage
  mem_label = lv_label_create(scr);
  lv_label_set_recolor(mem_label, true);
  lv_obj_add_style(mem_label, &style_bigfont, LV_PART_MAIN);           //Apply the new style
  lv_obj_align_to(mem_label, perf_chart, LV_ALIGN_BOTTOM_LEFT, 5, -5);
  
  //Create a label for the details of Memory and CPU usage
  info_label = lv_label_create(scr);
  lv_label_set_recolor(info_label, true);
  lv_obj_add_style(info_label, &style_bigfont, LV_PART_MAIN);           //Apply the new style
  lv_obj_align_to(info_label, perf_chart, LV_ALIGN_OUT_BOTTOM_LEFT, 4, 4);
}

extern unsigned long _heap_start;
extern unsigned long _heap_end;
extern char *__brkval;

int freeram() {
  return (char *)&_heap_end - __brkval;
}
int totalram() {
  return (char *)&_heap_end - (char *)&_heap_start;
}

static void sysmonTimerAction(lv_timer_t * param) {
    char buf_long[256];

    //Add the voltage and current data to the chart
    lv_chart_set_next_value(batt_chart, volt_ser, battPercent);
    int amps = (filtered_current - 2050) / 4.1;
    lv_chart_set_next_value(batt_chart, amp_ser, amps + 50);

    //volt label
    sprintf(buf_long, "%s%s CHARGE: %d%%",
            LV_TXT_COLOR_CMD,
            CPU_LABEL_COLOR,
            battPercent);
    lv_label_set_text(volt_label, buf_long);
    //amp label
    sprintf(buf_long, "%s%s CURRENT: %d%%",
            LV_TXT_COLOR_CMD,
            MEM_LABEL_COLOR,
            amps);
    lv_label_set_text(amp_label, buf_long);

    sprintf(buf_long, "Voltage: %.2fV, Current: %d",
            battVoltage,
            (int)filtered_current); 
    lv_label_set_text(batt_label, buf_long);

    //Get CPU and memory information 
    uint8_t cpu_busy = 100 - lv_timer_get_idle();
    uint8_t mem_used_pct = 0;
#if  LV_MEM_CUSTOM == 0
    lv_mem_monitor_t mem_mon;
    lv_mem_monitor(&mem_mon);
    mem_used_pct = mem_mon.used_pct;
#else
    mem_used_pct = ((totalram() - freeram()) * 100) / totalram();    
#endif
    
    //Add the CPU and memory data to the chart
    lv_chart_set_next_value(perf_chart, cpu_ser, cpu_busy);
    lv_chart_set_next_value(perf_chart, mem_ser, mem_used_pct);

    //Refresh the and windows
    //CPU label
    sprintf(buf_long, "%s%s CPU: %d%%",
            LV_TXT_COLOR_CMD,
            CPU_LABEL_COLOR,
            cpu_busy);
    lv_label_set_text(cpu_label, buf_long);
    //Memory label
    sprintf(buf_long, "%s%s MEMORY: %d%%",
            LV_TXT_COLOR_CMD,
            MEM_LABEL_COLOR,
            mem_used_pct);
    lv_label_set_text(mem_label, buf_long);

#if LV_MEM_CUSTOM == 0            
    sprintf(buf_long, "Total: %.1fK, Free: %.1fK, Frag: %d%%",
            (int)mem_mon.total_size / 1024.0,
            (int)mem_mon.free_size / 1024.0, 
            (int)mem_mon.frag_pct);
#else
    sprintf(buf_long, "Total: %.1fK, Used: %.1fK, Free: %.1fK",
            totalram() / 1024.0,
            (totalram() - freeram()) / 1024.0, 
            freeram() / 1024.0);
#endif
    lv_label_set_text(info_label, buf_long);
}


//===============================================================================================
