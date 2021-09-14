//LVGL 8.0.2 driver system for Teensy 4
// lv_fs_size() is an additional function not found in LVGL 
// (remove lvgl_obj_swap for future versions..)
// *Uses a File object pool because I cant work out how to
//  return a File pointer from a function without the 
//  local File object itself going out of scope.

#define MAXFILES  20      //File objects in the pool (maximum number of open files)

static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static lv_fs_drv_t sd_drv; 

//Display driver
void initLVGL()  {
  //Start TFT display.
  tft.begin();
  tft.setRotation( 3 );
  tft.fillScreen(ILI9341_BLACK);
  digitalWrite( TFT_BL, HIGH );   //Backlight on
  //Hook up LVGL
  lv_init();
#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif
  lv_disp_draw_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);
  /*Initialize the display*/
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LV_HOR_RES_MAX;
  disp_drv.ver_res = LV_VER_RES_MAX;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &disp_buf;
  display = lv_disp_drv_register(&disp_drv);
  /*Initialize the graphics library's tick*/
  guiTimer.begin(lv_tick_handler, LVGL_TICK_PERIOD * 1000);
}

//Input device driver
void inputDriverInit() {
  lv_indev_drv_init(&indev_drv);      /*Basic initialization*/
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_input_read;
  lv_indev_drv_register(&indev_drv);
}

// File system driver
void fileSystemInit() {
  serial.println("> Start Filesystem Driver.");
  lv_fs_drv_init(&sd_drv);                     /*Basic initialization*/  
  sd_drv.letter = 'C';                         /*An uppercase letter to identify the drive */
  sd_drv.open_cb = sd_open;                 /*Callback to open a file */
  sd_drv.close_cb = sd_close;               /*Callback to close a file */
  sd_drv.read_cb = sd_read;                 /*Callback to read a file */
  sd_drv.write_cb = sd_write;               /*Callback to write a file */
  sd_drv.seek_cb = sd_seek;                 /*Callback to seek in a file (Move cursor) */
  sd_drv.tell_cb = sd_tell;                 /*Callback to tell the cursor position  */  
  sd_drv.dir_close_cb = sd_dir_close;
  sd_drv.dir_open_cb = sd_dir_open;
  sd_drv.dir_read_cb = sd_dir_read;
  lv_fs_drv_register(&sd_drv);                 /*Finally register the drive*/
}

//----------------------------------------------------
// Driver layer

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
  Serial.print(buf);
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
//  spiLock = true; 
  if (!ScreenSaverActive) tft.writeRect(area->x1,area->y1,area->x2-area->x1+1,area->y2-area->y1+1,(uint16_t*)color_p);
//  spiLock = false;
  lv_disp_flush_ready(disp); /* tell lvgl that flushing is done */
}

/* Interrupt driven periodic handler */
static void lv_tick_handler(void)
{
  lv_tick_inc(LVGL_TICK_PERIOD);
}

//My touchscreen is noisy and needs extra conditioning
void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data)
{
  static uint8_t touchCount = 0;
  static int16_t px = 0, py = 0;
  //spiLock = true; 
  TS_Point p = ts.getPoint();
  bool touch = ts.touched() && (p.z > 600);
  //spiLock = false; 
/*  
    Serial.print(" ("); Serial.print(p.x);
    Serial.print(", "); Serial.print(p.y);
    Serial.println(")");
*/  
  if (touch) {
    ScreenSaverInteraction();
    if (touchCount < 2) {
      touchCount++;
      touch = false;
    }
  } else touchCount = 0;
  // Scale from ~0->4000 to tft.width using the calibration #'s
  data->point.x = constrain(map((px + p.x) / 2, TS_MINX, TS_MAXX, 0, tft.width()), 0, LV_HOR_RES_MAX);
  data->point.y = constrain(map((py + p.y) / 2, TS_MINY, TS_MAXY, 0, tft.height()), 0, LV_VER_RES_MAX);
  data->state = touch? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  px = p.x;
  py = p.y;
}

bool wasTouched = false;
void ScreenSaverHandle() {
  if (!ScreenSaverPending) {
    if (millis() > ScreenSaverTimer + SCREENSAVER_TIMEOUT) {
      ScreenSaverPending = true;
      lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_ON);    
    }
  } else if (ScreenSaverActive) {
    TS_Point p = ts.getPoint();
    bool touch = ts.touched() && (p.z > 600);
    if (!touch && wasTouched) 
      ScreenSaverInteraction();
    wasTouched = touch;
  }
}

void ScreenSaverInteraction() {
  if (ScreenSaverPending) {
    ScreenSaverPending = false;
    if (ScreenSaverActive) {
      ScreenSaverActive = false;
      wasTouched = false;
      lv_obj_invalidate(lv_scr_act());
    }
  }
  ScreenSaverTimer = millis();  
}

//----------------------------------------------------
// SD access

//File object pool
File fpPool[MAXFILES];
bool fpUsed[MAXFILES];      //File object used

//Find a free File object in the pool
// Returns a file number (index into the pool, starting at 1)
// returns 0 if out of files
int assignFileNum() {
  for (int n = 0; n < MAXFILES; n++) {
    if (!fpUsed[n]) {
      fpUsed[n] = true;
      return n+1;
    }
  }
  serial.print("> Out of files!");
  return 0;
}

//Get the File object for a given file number
File * getFile(int n) {
  return &fpPool[n-1];
}

//Return a File object to the pool
void discardFile(int n) {
  fpUsed[n-1] = false;
}



/**
 * Open a file from the PC
 * @param file_p pointer to a FILE* variable
 * @param fn name of the file.
 * @param mode element of 'fs_mode_t' enum or its 'OR' connection (e.g. FS_MODE_WR | FS_MODE_RD)
 * @return LV_FS_RES_OK: no error, the file is opened
 *         any error from lv_fs_res_t enum
 */

static void* sd_open(lv_fs_drv_t *drv, const char * fn, lv_fs_mode_t mode)
{
    int flags = 0;

    if(mode == LV_FS_MODE_WR) flags = O_READ | O_WRITE | O_CREAT;
    else if(mode == LV_FS_MODE_RD) flags = O_READ;
    else if(mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) flags = O_READ | O_WRITE | O_CREAT;

    /*Make the path relative to the current directory (the projects root folder)*/
    //char buf[256];
    //sprintf(buf, "/%s", fn);
    //spiLock = true;
    int fno = assignFileNum();
    fpPool[fno-1] = SD.open(fn, flags);
    if (SD.sdfs.sdErrorCode()) {
      SD.sdfs.errorPrint(&Serial);
    }
    return (void*)fno;
}


uint32_t lv_fs_size(lv_fs_file_t * file) {
  return getFile((int)(file->file_d))->size();
}

/**
 * Close an opened file
 * @param file_p pointer to a FILE* variable. (opened with lv_ufs_open)
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv__fs_res_t enum
 */
static lv_fs_res_t sd_close(lv_fs_drv_t *drv, void * file_p)
{
    File * fp = getFile((int)file_p);        /*Just avoid the confusing casings*/
    //spiLock = true; 
    fp->close();
    //spiLock = false;
    discardFile((int)file_p);
    if (SD.sdfs.sdErrorCode()) {
      SD.sdfs.errorPrint(&Serial);
    }
    return LV_FS_RES_OK;
}

/**
 * Read data from an opened file
 * @param file_p pointer to a FILE variable.
 * @param buf pointer to a memory block where to store the read data
 * @param btr number of Bytes To Read
 * @param br the real number of read bytes (Byte Read)
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv__fs_res_t enum
 */
static lv_fs_res_t sd_read(lv_fs_drv_t *drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    File * fp = getFile((int)file_p);        /*Just avoid the confusing casings*/
    //spiLock = true; 
    *br = fp->read(buf, btr);
    //spiLock = false;
    if (SD.sdfs.sdErrorCode()) {
      SD.sdfs.errorPrint(&Serial);
    }
    return LV_FS_RES_OK;
}
/*
void printSdError() {
  if (SD.sdfs.sdErrorCode()) {
    if (SD.sdfs.sdErrorCode() == SD_CARD_ERROR_CMD0) {
      Serial.println("No card, wrong chip select pin, or wiring error?");
    }
    Serial.print("SD error: ");
    printSdErrorSymbol(pr, sdErrorCode());
    pr->print(F(" = 0x"));
    pr->print(sdErrorCode(), HEX);
    pr->print(F(",0x"));
    pr->println(sdErrorData(), HEX);
  } else if (!Vol::fatType()) {
    pr->println(F("Check SD format."));
  }
}*/

/**
 * Write into a file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable
 * @param buf pointer to a buffer with the bytes to write
 * @param btr Bytes To Write
 * @param br the number of real written bytes (Bytes Written). NULL if unused.
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static lv_fs_res_t sd_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw) {
    File * fp = getFile((int)file_p);        /*Just avoid the confusing casings*/
    //spiLock = true; 
    *bw = fp->write(buf, btw);
    //spiLock = false;
    if (SD.sdfs.sdErrorCode()) {
      SD.sdfs.errorPrint(&Serial);
    }
    return LV_FS_RES_OK;
}

/**
 * Set the read write pointer. Also expand the file size if necessary.
 * @param file_p pointer to a FILE* variable. (opened with lv_ufs_open )
 * @param pos the new position of read write pointer
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv__fs_res_t enum
 */
static lv_fs_res_t sd_seek(lv_fs_drv_t *drv, void * file_p, int32_t pos, lv_fs_whence_t whence)
{
    File * fp = getFile((int)file_p);        /*Just avoid the confusing casings*/
    //spiLock = true; 
    if (whence == LV_FS_SEEK_SET)
      fp->seek(pos);
    else if (whence == LV_FS_SEEK_CUR)
      fp->seek(pos + fp->position());
    else if (whence == LV_FS_SEEK_END)
      fp->seek(pos + fp->size());
    //spiLock = false;
    if (SD.sdfs.sdErrorCode()) {
      SD.sdfs.errorPrint(&Serial);
    }
    return LV_FS_RES_OK;
}

/**
 * Give the position of the read write pointer
 * @param file_p pointer to a FILE* variable.
 * @param pos_p pointer to to store the result
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv__fs_res_t enum
 */
static lv_fs_res_t sd_tell(lv_fs_drv_t *drv, void * file_p, uint32_t * pos_p)
{
    File * fp = getFile((int)file_p);        /*Just avoid the confusing casings*/
    //spiLock = true; 
    *pos_p = fp->position();
    //spiLock = false;
    if (SD.sdfs.sdErrorCode()) {
      SD.sdfs.errorPrint(&Serial);
    }
    return LV_FS_RES_OK;
}

/**
 * Initialize a 'fs_read_dir_t' variable for directory reading
 * @param drv pointer to a driver where this function belongs
 * @param dir_p pointer to a 'fs_read_dir_t' variable
 * @param path path to a directory
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
//File dp; 
static void * sd_dir_open (lv_fs_drv_t * drv, const char *path)
{
  if (strlen(path) == 0) path = "/";
  //Serial.print("--Open directory : ");
  //Serial.println(path);
  int fno = assignFileNum();
  fpPool[fno-1] = SD.open(path);
  if (!fpPool[fno-1]) Serial.println("Error opening directory!");
  if (SD.sdfs.sdErrorCode()) {
    SD.sdfs.errorPrint(&Serial);
  }
  return (void*)fno;
}

/**
 * Read the next filename form a directory.
 * The name of the directories will begin with '/'
 * @param drv pointer to a driver where this function belongs
 * @param dir_p pointer to an initialized 'fs_read_dir_t' variable
 * @param fn pointer to a buffer to store the filename
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static lv_fs_res_t sd_dir_read (lv_fs_drv_t * drv, void * dir_p, char *fn)
{
  File * dp = getFile((int)dir_p); 
  //Serial.print("--Read Directory Entry:");
  File entry =  dp->openNextFile();
  if (!entry) strcpy(fn, "");
  else {
    char * str = fn;
    if (entry.isDirectory()) *str++ = '/';
    strcpy(str, entry.name());
    entry.close(); 
  }
  //Serial.println(fn);
  if (SD.sdfs.sdErrorCode()) {
    SD.sdfs.errorPrint(&Serial);
  }
  return LV_FS_RES_OK;
}

/**
 * Close the directory reading
 * @param drv pointer to a driver where this function belongs
 * @param dir_p pointer to an initialized 'fs_read_dir_t' variable
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static lv_fs_res_t sd_dir_close (lv_fs_drv_t * drv, void * dir_p)
{
  File * dp = getFile((int)dir_p);
  //Serial.println("--Close Directory");
  dp->close(); 
  discardFile((int)dir_p);
  if (SD.sdfs.sdErrorCode()) {
    SD.sdfs.errorPrint(&Serial);
  }
  return LV_FS_RES_OK;
}

bool fs_err(lv_fs_res_t err, const char * msg) {
  if (err == LV_FS_RES_OK) return false;
  serial.print(msg);
  serial.print(": ");
  if (err == LV_FS_RES_INV_PARAM) serial.println("Invalid Parameter");
  else if (err == LV_FS_RES_NOT_EX) serial.println("Drive doesn't exist");
  else if (err == LV_FS_RES_HW_ERR) serial.println("Hardware error");
  else if (err == LV_FS_RES_NOT_IMP) serial.println("Not implemented");
  else if (err == LV_FS_RES_NOT_IMP) serial.println("Unknown");
  else serial.printf("Error: %d\n", err);
  return true;
}

//Can be removed in later versions of lvgl
void lv_obj_swap(lv_obj_t * obj1, lv_obj_t * obj2)
{
    LV_ASSERT_OBJ(obj1, MY_CLASS);
    LV_ASSERT_OBJ(obj2, MY_CLASS);

    lv_obj_t * parent = lv_obj_get_parent(obj1);
    lv_obj_t * parent2 = lv_obj_get_parent(obj2);

    uint_fast32_t index1 = lv_obj_get_child_id(obj1);
    uint_fast32_t index2 = lv_obj_get_child_id(obj2);

    parent->spec_attr->children[index1] = obj2;
    parent2->spec_attr->children[index2] = obj1;
    lv_event_send(parent, LV_EVENT_CHILD_CHANGED, obj2);
    lv_event_send(parent2, LV_EVENT_CHILD_CHANGED, obj1);

    lv_obj_invalidate(parent);
    if( parent != parent2) {
        lv_obj_invalidate(parent2);
    }

    //lv_group_swap_obj(obj1, obj2);
}
