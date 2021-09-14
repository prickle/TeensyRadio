//TeensyRadio
// Digital Boombox v0.9
// see config.h for notes

// Nick Metcalfe 2019-2021 covid lockdown project
// Teensy 4.0 600Mhz - Teensyduino 1.54
// Using lvgl 8.0.2

#include <lvgl.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>
#include <XPT2046_Touchscreen.h>
#include <HardwareSerial.h>
#include <Adafruit_VS1053.h>
#include <ILI9341_t3n.h>
#include "ILI9341_t3n_font_Arial.h"
#include <TimeLib.h>
#include <MonkeyBoard.h>
#include <TJpg_Decoder.h>

#include "config.h"
#include "fifo.h"
#include "logo.h"

//Control and program ESP8266 through regular USB serial
#define ESP8266_PASSTHROUGH   0   //Set 0 for normal operation, 1 for ESP passthrough
#define ESP8266_BOOTLOADER    0   //when passthrough, set 0 for AT commands, 1 for bootloader

//If the controller is disconnected from the boombox,
// set this true to have it still do stuff
bool disconnected = false;

//Hardware drivers --------------------------------------------------------------------------

//VS1053 MP3 decoder
Adafruit_VS1053 vs = Adafruit_VS1053(VS_RST, VS_CS, VS_DCS, VS_DRQ);

//Resistive touchscreen
XPT2046_Touchscreen ts(TOUCH_CS);

//LCD TFT Display
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO);
//ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCK, TFT_MISO);

// Create an IntervalTimer object 
IntervalTimer guiTimer;     //lvgl
IntervalTimer playTimer;    //FIFO feeder

//Streaming circular buffer
Fifo fifo;

//The Monkeyboard DAB module
MonkeyBoard dab = MonkeyBoard(&DAB_SERIAL);

//Show an error message and carry on
void errorContinue(const char * message) {
  char str[256] = " " LV_SYMBOL_WARNING " - ";
  strncpy(&str[7], message, 248);
  if (progTextLbl) lv_label_set_text(progTextLbl, str);
  serial.print("> ");
  serial.println(message);
}

//Show an error message and set the fatal flag
void errorHalt(const char * message) {
  errorContinue(message);
  startupError = true;
}

void setup() {
  //Setup IO pins
  io_setup();

  //Start Serial and SD card here
  Serial.begin( SERIAL_BAUD_RATE );
  SDFound = SD.begin(SDCARD_CS);
  
  //Read EEPROM
  readSettings();

  //Start GUI ASAP to avoid blank screen
  serial.println("> Digital Boombox v0.9 Nick Metcalfe 9/2021.");
  serial.println("> Starting LittleVGL GUI.");
  initLVGL();
  screenInit();

  //File storage init
  serial.print("> SD Card Setup: ");
  if (!SDFound) errorHalt("No SD Card!");
  else serial.println("Found Card.");

  //Wait for serial
  serial.print("> Checking for USB: ");
  if (!Serial) {
    if (progNowLbl) lv_label_set_text(progNowLbl, "Checking for USB connection..");
    while(!Serial && millis() < 1000) delay(5);
    if (progNowLbl) lv_label_set_text(progNowLbl, "");
  }
  if (Serial) serial.println("Found.");
  else serial.println("None.");


  //Start touchscreen
  serial.print("> Touchscreen Setup: ");
  if (!ts.begin()) errorHalt("Couldn't start touchscreen controller!");
  else serial.println("OK.");
  ts.setRotation(1);
  inputDriverInit();

  //Start VS1053 MP3 Decoder module
  if (!(VSFound = vs.begin())) errorContinue("VS1053 MP3 Codec Fail!"); // initialise the music player
  else {
    serial.println("> VS1053 Setup OK.");
    prepareVS();
  }

  //Kick off the ESP8266 serial passthrough here
  if(ESP8266_PASSTHROUGH) {
    if ((WLANFound = resetWLAN())) {
      serial.println("> ESP8266 WiFi Found.");    
      espPassthru(ESP8266_BOOTLOADER);
    } else serial.println("> ESP8266 WiFi not responding!");
  }
  
  //Start DAB+ module
  if (!(DABFound = dab.begin())) errorContinue("DAB+ Radio Module Fail!");
  else serial.println("> DAB+ Radio Setup OK.");
  prepareDab();   //set callbacks and allocate memory

  //Set RTC
  setSyncProvider((getExternalTime)Teensy3Clock.get);
  if (timeStatus()!= timeSet) {
    serial.println("> Unable to sync with the RTC!");
  } else {
    serial.print("> RTC time: ");
    digitalClockDisplay();
  }

  //Hook up lvgl file system to SD card
  if (SDFound) {
    fileSystemInit();
    browserPopulateList();          //kick off the file browser
    //loadPlaylist(PLAYLIST_PATH);
  }

  //Restore settings to controls  
  dab.SetVolume(settings.dabVolume);
  setDabEQ(settings.dabEQ);
  setVSVolume(settings.vsVolume);
  setVSTone(settings.vsTone);
  serial.InSetup = false;
  if (disconnected) {
    currentFunction = FUNC_TA;
    setRadioMode();
  }
  serial.println("> Setup complete - entering main loop.");
  ScreenSaverTimer = millis();

}


//-----------------------------------------------------------
void loop() { 
  lv_task_handler();
  updateTuning();
  wlanHandle();
  dab.handle();
  ScreenSaverHandle();
  terminalHandle();
  io_handle();
  sdPlayerHandle();
  passwordHandle();
}

//The RTC needs work - not yet incoporated
void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  serial.print(":");
  if(digits < 10)
    serial.print('0');
  serial.print(digits);
}
void digitalClockDisplay() {
  // digital clock display of the time
  serial.print(hour());
  printDigits(minute());
  printDigits(second());
  serial.print(" ");
  serial.print(day());
  serial.print(" ");
  serial.print(month());
  serial.print(" ");
  serial.print(year()); 
  serial.println(); 
}

// -- BEWARE! --
//Magic - if searchStationName contains a string it will be found in station names
// and the station's URL transferred to settings.server
// otherwise the default behaviour is to search for the URL in settings.server
// searchStationName is reset after the search to revert to default behaviour.
// This magic is used by the Presets mechanism to match by station name whereas
// normal startup matches by URL. This allows sensible operation with SD card removed.
 
int readWebStations() {
  serial.println("> Reading WebRadio playlist..");
  lv_fs_file_t f;
  if (fs_err(lv_fs_open(&f, WEBSTATIONS_PATH, LV_FS_MODE_RD), "> Error opening playlist!")) return 0;
  int lbi = 0;
  char linebuf[256];
  char name[35];
  int count = 0;
  lv_obj_t * previous = NULL;
  uint32_t read_num;
  char ch;
  bool search = strlen(searchStationName);
  bool found = false;
  while (1) {
    lv_refr_now(display);
    if (fs_err(lv_fs_read(&f, &ch, 1, &read_num), "> Error reading playlist!")) return 0;
    if (read_num == 0) break;
    if (ch != '\r' && ch != '\n' ) linebuf[lbi++] = ch;
    if (ch == '\n' || lbi == 255) {
      linebuf[lbi] = 0;
      //Serial.println(linebuf);
      if (linebuf[0] == '#') {
        char * ptr = strchr(linebuf, ',');
        if (ptr) {
          strncpy(name, ptr+1, 34);
          name[34] = '\0';
          if (search && strcmp(name, searchStationName) == 0) found = true;
        }
      }
      if (strncasecmp("http", linebuf, 4) == 0) {
        lv_obj_t * list_btn = addStationButton(name, webRadioListAction, linebuf, lbi+1);
        if (!search && strncmp(linebuf, settings.server, 255) == 0) found = true;
        if (found) {
          serial.println("> Selected station found.");
          if (search && strncmp(linebuf, settings.server, 255) != 0) {
            strncpy(settings.server, linebuf, 255);
            settings.server[255] = '\0';
            writeSettings();
          } else {
            strncpy(webStationName, name, 34);
            webStationName[34] = '\0';
          }
          previous = list_btn;
          found = false;
        }
        count++;
      }
      lbi = 0;
    }
  }
  if (previous) listSetSelect(previous);
  serial.printf("> Read %d list entries OK.\r\n", count);
  lv_fs_close(&f);
  searchStationName[0] = '\0';
  return count;
}
