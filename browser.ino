static bool browserSelectingMode;
static lv_obj_t * browserMainList;
static lv_obj_t * browserSelectBtn = NULL;
static lv_obj_t * browserCancelBtn = NULL;
static lv_obj_t * browserWinTitle;

//Current full directory path, no trailing slash
// Start in the music directory
static char browserCurrentDir[LV_FS_MAX_PATH_LENGTH] = MUSIC_PATH;

//make the window
void createFileBrowserWindow(lv_obj_t * page) {
  /* Create a window to use as the action bar */
  browserWin = lv_win_create(page, 28);
  lv_obj_set_size(browserWin, lv_obj_get_content_width(page), lv_obj_get_content_height(page));
  lv_obj_add_style(browserWin, &style_win, LV_PART_MAIN);
  browserWinTitle = lv_win_add_title(browserWin, "File Manager");
  lv_obj_t * up_btn = lv_win_add_btn(browserWin, LV_SYMBOL_UP, 33);
  lv_obj_add_event_cb(up_btn, browserGoUp, LV_EVENT_CLICKED, NULL);
  lv_obj_add_style(up_btn, &style_wp, LV_PART_MAIN);  
  lv_obj_add_style(up_btn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(up_btn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_t * home_btn = lv_win_add_btn(browserWin, LV_SYMBOL_HOME, 33);
  lv_obj_add_style(home_btn, &style_wp, LV_PART_MAIN);  
  lv_obj_add_style(home_btn, &style_bigfont_orange, LV_PART_MAIN);
  lv_obj_add_style(home_btn, &style_bigfont_orange, LV_PART_SELECTED);
  lv_obj_add_event_cb(home_btn, browserGoHome, LV_EVENT_CLICKED, NULL);
  lv_obj_t * win_content = lv_win_get_content(browserWin);
  lv_obj_add_style(win_content, &style_win, LV_PART_MAIN);
  
  /* Create the file list */
  browserMainList = lv_list_create(win_content);
  //lv_obj_add_style(browserMainList, &style_win, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(browserMainList, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_update_layout(win_content);
  lv_obj_set_size(browserMainList, lv_obj_get_content_width(win_content), lv_obj_get_content_height(win_content));
  lv_obj_set_pos(browserMainList, 0, 0);
  lv_obj_set_hidden(browserWin, true);
}

//Tick clicked, multi-select accepted
static void browserDoneSelecting(lv_event_t * event) {
  lv_fs_file_t f;
  bool tabViewShifted = false;
  //Build playlist
  removePlaylist();
  if (fs_err(lv_fs_open(&f, PLAYLIST_PATH, LV_FS_MODE_WR), "Open Playlist")) return;
  char filepath[LV_FS_MAX_PATH_LENGTH+1];
  for (uint16_t i = 0; i < lv_obj_get_child_cnt(browserMainList); i++) {
    lv_obj_t* child = lv_obj_get_child(browserMainList, i);
    if (lv_obj_get_state(child) & LV_STATE_CHECKED) {
      if (!tabViewShifted) {
        lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_ON);
        tabViewShifted = true;
      }
      const char *filename = lv_list_get_btn_text(browserMainList, child);
      bool is_dir = (bool)lv_obj_get_user_data(child);
      strncpy(filepath, browserCurrentDir, LV_FS_MAX_PATH_LENGTH-1);
      filepath[LV_FS_MAX_PATH_LENGTH-1] = '\0';
      appendFileName(filepath, filename);   
      if(is_dir) {
        //recurse into directories
        playlistRecurseDirectory(&f, filepath);
      } else {
        //add single file
        addPlaylistEntry(&f, filepath);      
      }
    }
  }  
  lv_fs_close(&f);  
  playFile(PLAYLIST_PATH);
  browserSetSelectingMode(false);
}

//Cross clicked
static void browserCancelSelecting(lv_event_t * event) {
  browserSetSelectingMode(false);
}

//toggle multi-select mode
static void browserSetSelectingMode(bool mode) {
  if(browserSelectingMode != mode) {
    if(mode) {
      browserSelectBtn = lv_win_add_btn(browserWin, LV_SYMBOL_OK, 33);
      lv_obj_add_style(browserSelectBtn, &style_wp, LV_PART_MAIN);  
      lv_obj_add_style(browserSelectBtn, &style_bigfont_orange, LV_PART_MAIN);
      lv_obj_add_style(browserSelectBtn, &style_bigfont_orange, LV_PART_SELECTED);
      lv_obj_add_event_cb(browserSelectBtn, browserDoneSelecting, LV_EVENT_CLICKED, NULL);
      browserCancelBtn = lv_win_add_btn(browserWin, LV_SYMBOL_CLOSE, 33);
      lv_obj_add_style(browserCancelBtn, &style_wp, LV_PART_MAIN);  
      lv_obj_add_style(browserCancelBtn, &style_bigfont_orange, LV_PART_MAIN);
      lv_obj_add_style(browserCancelBtn, &style_bigfont_orange, LV_PART_SELECTED);
      lv_obj_add_event_cb(browserCancelBtn, browserCancelSelecting, LV_EVENT_CLICKED, NULL);
    } else {
      if(browserSelectBtn) {
        lv_obj_del(browserSelectBtn);
        browserSelectBtn = NULL;
      }
      if(browserCancelBtn) {
        lv_obj_del(browserCancelBtn);
        browserCancelBtn = NULL;
      }
      browserClearSelections();
    }
  }
  browserSelectingMode = mode;
}

//Remove all selections from file browser
void browserClearSelections() {
  for (uint16_t i = 0; i < lv_obj_get_child_cnt(browserMainList); i++) {
    lv_obj_t* child = lv_obj_get_child(browserMainList, i);
    lv_obj_clear_state(child, LV_STATE_CHECKED);
  }  
}

//Back out a directory level
static void browserGoUp(lv_event_t * event) {
  if(strlen(browserCurrentDir) == 2) { /* drive letter plus : */
    /* We cannot go up further */
    return;
  }
  //serial.printf("Current directory: %s\n", browserCurrentDir);
  char *last_slash = strrchr(browserCurrentDir, '/');
  if (last_slash != NULL) {
    *last_slash = 0;
    browserPopulateList();
  }
}

static void browserGoHome(lv_event_t * event) {
  strcpy(browserCurrentDir, MUSIC_PATH);
  browserPopulateList();
}

//File entry selected
static void browserFileListButtonEvent(lv_event_t * event) {
  lv_obj_t * list_btn = lv_event_get_target(event);
  if(lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    if(browserSelectingMode) {
      if(lv_obj_get_state(list_btn) & LV_STATE_CHECKED) {
        lv_obj_clear_state(list_btn, LV_STATE_CHECKED);
      } else
        lv_obj_add_state(list_btn, LV_STATE_CHECKED);
      return;
    }
    
    /* Get some basic information about the file */
    const char *filename = lv_list_get_btn_text(browserMainList, list_btn);
    bool is_dir = (bool)lv_obj_get_user_data(list_btn);
    
    /* Only change to new directories */
    if(is_dir) {
      uint32_t len = strnlen(browserCurrentDir, LV_FS_MAX_PATH_LENGTH);
      strncat(browserCurrentDir, "/", LV_FS_MAX_PATH_LENGTH - len - 1);
      len++;
      strncat(browserCurrentDir, filename, LV_FS_MAX_PATH_LENGTH - len - 1);
      browserPopulateList();  
    } else {
      //Play single file
      lv_obj_add_state(list_btn, LV_STATE_CHECKED);
      lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_ON);
      char fname[LV_FS_MAX_PATH_LENGTH];
      strncpy(fname, browserCurrentDir, LV_FS_MAX_PATH_LENGTH-1);
      uint32_t len = strnlen(fname, LV_FS_MAX_PATH_LENGTH-1);
      strncat(fname, "/", LV_FS_MAX_PATH_LENGTH - len - 1);
      strncat(fname, filename, LV_FS_MAX_PATH_LENGTH - len - 2);
      browserClearSelections();
      playFile(fname);
    }    
    /* The list button is deleted now */
  } else if(lv_event_get_code(event) == LV_EVENT_LONG_PRESSED) {
    browserSetSelectingMode(true);
    if (lv_obj_get_state(list_btn) & LV_STATE_CHECKED) {
      lv_obj_clear_state(list_btn, LV_STATE_CHECKED);      
    } else lv_obj_add_state(list_btn, LV_STATE_CHECKED);
  }
}

//Read a directory from disk into the file browser window
static void browserPopulateList(void)
{
  static char fn[LV_FS_MAX_FN_LENGTH];
  lv_fs_dir_t dir;
  bool is_dir;
  if (browserWinTitle) lv_label_set_text(browserWinTitle, browserCurrentDir);
  if (fs_err(lv_fs_dir_open(&dir, browserCurrentDir), "Browser Open Directory")) return;
  
  browserSetSelectingMode(false);
  lv_obj_clean(browserMainList);
  fn[0] = 0;
  do {
    lv_fs_dir_read(&dir, fn);
    if(fn[0] != 0) {
      is_dir = (fn[0] == '/');
      const char* symbol = LV_SYMBOL_DIRECTORY;
      if (!is_dir) {
        if (isMP3File(fn)) symbol = LV_SYMBOL_AUDIO;
        else symbol = LV_SYMBOL_FILE;  
      }
      lv_obj_t * list_btn = lv_list_add_btn(browserMainList, symbol, &fn[is_dir]);
      lv_obj_add_style(list_btn, &style_halfopa, LV_PART_MAIN);
      lv_obj_add_style(list_btn, &style_listsel, LV_STATE_CHECKED);
      lv_obj_set_user_data(list_btn, (void*)is_dir);
      lv_obj_add_event_cb(list_btn, browserFileListButtonEvent, LV_EVENT_ALL, NULL);
    } else break;
  } while(1);
  lv_fs_dir_close(&dir);
}

//------------------------------------------------------------
//Playlist file management

//Remove (delete) playlist
void removePlaylist() {
  if (SD.exists(PLAYLIST_SDPATH)) {
    Serial.println("> Removing old playlist");
    SD.remove(PLAYLIST_SDPATH);  
  }
}

//Add a playlist entry to an open file
void addPlaylistEntry(lv_fs_file_t * f, char * path) {
  uint32_t byteswrote;
  if (isKnownMusicFile(path)) {  
    char str[260];
    char name[LV_FS_MAX_PATH_LENGTH] = {0};
    char * pos;
    if ((pos = strrchr(path, '/'))) {
      strncpy(name, pos+1, LV_FS_MAX_PATH_LENGTH - 1);
      name[LV_FS_MAX_PATH_LENGTH-1] = '\0';
    }
    sprintf(str, "#%s\r\n", name);
    if (fs_err(lv_fs_write(f, str, strlen(str), &byteswrote), "Write Playlist Header")) return;
    sprintf(str, "%s\r\n", path);
    if (fs_err(lv_fs_write(f, str, strlen(str), &byteswrote), "Write Playlist Entry")) return;
  }
}

//Append a name to a path string, adding slash as required
void appendFileName(char * path, const char * name) {
  uint32_t len = strnlen(path, LV_FS_MAX_PATH_LENGTH);
  if (name[0] != '/') {
    strncat(path, "/", LV_FS_MAX_PATH_LENGTH - len - 1);
    len++;
  }
  strncat(path, name, LV_FS_MAX_PATH_LENGTH - len - 1);
}

//Recursive directory tree traversal
// Builds a .m3u playlist from known audio files
static void playlistRecurseDirectory(lv_fs_file_t * f, char * orig_path) {
  char fn[LV_FS_MAX_PATH_LENGTH];
  char path[LV_FS_MAX_PATH_LENGTH];
  lv_fs_dir_t dir;
  Serial.print("Recurse directory :");
  Serial.println(orig_path);
  if (fs_err(lv_fs_dir_open(&dir, orig_path), "Playlist Recurse Directory")) return;
  do {
    lv_refr_now(display);
    fn[0] = 0;
    lv_fs_dir_read(&dir, fn);
    if(fn[0] != 0) {
      bool is_dir = (fn[0] == '/');
      strncpy(path, orig_path, LV_FS_MAX_PATH_LENGTH-1);
      path[LV_FS_MAX_PATH_LENGTH-1] = '\0';
      appendFileName(path, fn);
      if (is_dir) playlistRecurseDirectory(f, path);    
      else addPlaylistEntry(f, path);              
    } else break;
  } while(1);
  lv_fs_dir_close(&dir);
}
