
//Teensyduino modifications:
// for WLAN
// in HardwareSerial5.cpp
// Line 38:  #define SERIAL5_RX_BUFFER_SIZE     4096 // number of incoming bytes to buffer
// Line 67:  IRQ_PRIORITY, 3072, 1024, // IRQ, rts_low_watermark, rts_high_watermark
// ***no** Line 438:  //rts_low_watermark_ = rx_buffer_total_size_ - hardware->rts_low_watermark;
// ***no** Line 439:  //rts_high_watermark_ = rx_buffer_total_size_ - hardware->rts_high_watermark;

// This is to increase the serial buffer's tolerance of ESP8266 CTS latency at high speeds
// also remove or rename Adafruit_VS1053 in teensyduino


// Notes: lna has bfp420+bav99 preamp

//Global variables

bool systemReady = false;
volatile bool buffering = false;     //Filling buffer?
volatile bool streamStarted = false; //Streaming?
volatile bool SDFound = 0;
volatile bool SDActive = 0;
volatile bool WLANFound = 0;
volatile bool WLANActive = 0;
volatile bool DABFound = 0;
volatile bool DABActive = 0;
volatile bool VSFound = 0;
bool startupError = false;

//Storage names
#define SYSTEM_DRV_LETTER "C:"
#define SYSTEM_DIR        "/System"
#define MUSIC_DIR         "/Music"
#define WEBSTATIONS_FILE  "/WebStations.m3u"
#define PLAYLIST_FILE     "/PlayList.m3u"

//Storage paths
#define SYSTEM_PATH       SYSTEM_DRV_LETTER SYSTEM_DIR
#define WEBSTATIONS_PATH  SYSTEM_PATH WEBSTATIONS_FILE
#define MUSIC_PATH        SYSTEM_DRV_LETTER MUSIC_DIR
#define PLAYLIST_PATH     SYSTEM_PATH PLAYLIST_FILE
#define PLAYLIST_SDPATH   SYSTEM_DIR PLAYLIST_FILE

//Screensaver
#define SCREENSAVER_TIMEOUT 30000
unsigned long ScreenSaverTimer;
bool ScreenSaverActive = false;
bool ScreenSaverPending = false;

//Tab control
uint8_t mainWindowIndex = 2;

//LVGL display object
static lv_disp_t * display;

//LVGL objects
static lv_obj_t * fmFreqLbl;
static lv_obj_t * dab_img;
static lv_obj_t * dab_img_bar;
static lv_obj_t * progNameLbl;
static lv_obj_t * progNowLbl;
static lv_obj_t * progTextLbl;
static lv_obj_t * progTimeBar;
static lv_obj_t * sigStrengthLbl;
static lv_obj_t * battChgLbl;
static lv_obj_t * dabStationList;
static lv_obj_t * dabScanLabel;
static lv_obj_t * stmoLbl;
static lv_obj_t * tabView;
static lv_obj_t * mainContainer;
static lv_obj_t * wifiContainer;
static lv_obj_t * dabContainer;
static lv_obj_t * vsContainer;
static lv_obj_t * browserWin;
static lv_obj_t * playlistBtn;
static lv_obj_t * scanSpinner;

static lv_style_t style_listsel;
static lv_style_t style_halfopa;
static lv_style_t style_wp;
static lv_style_t style_win;  
static lv_style_t style_bigfont_orange;

//DAB Radio
uint8_t currentDabStatus = DAB_STATUS_NONE;

//String storage
char requestedURL[256] = "";
char webStationName[35] = "";
char fmStationName[35] = "";
char searchStationName[35] = "";
char sdFileName[LV_FS_MAX_PATH_LENGTH] = "<None>";

//transfer statistics
int bytesPerSecondCount = 0;
int bytesPerSecond = 0;

//Battery
#define BATT_CHARGE     0
#define BATT_FULL       1
#define BATT_DISCHARGE  2
#define BATT_LOW        3
#define BATT_EMPTY      4

uint8_t battState = BATT_DISCHARGE;
uint8_t battPercent = 0;

// boombox switches and dials state
#define DIAL_START_DELAY  -20
//int tuningDial = DIAL_START_DELAY; //startup delay cycles 
bool buttonFF = false;
bool buttonRW = false;
bool buttonST = true;
bool buttonDUB = false;
bool tapeActive = false;

//VS1053 controls
//uint8_t curvol = 0;   //Current volume
bool vsUsingFlac = false;

//VS1053 Spectrum analyser
volatile lv_coord_t spectrumValues[23];
volatile lv_coord_t spectrumPeakValues[23];
volatile uint8_t spectrumBands = 0;
volatile bool newSpectrum = false;

// lvgl display buffer 
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];
#define LVGL_TICK_PERIOD 5

//Name button action
#define NAMEBTN_RELOAD    0
#define NAMEBTN_PLAYLIST  1

//settings.mode
#define MODE_DAB  0
#define MODE_FM   1
#define MODE_WEB  2
#define MODE_SD   3
#define MODE_LINE 4
//uint8_t currentMode = MODE_DAB;
const char * modeString[] = {"DAB+", "Digital FM", "Web Radio", "SD Card", "Line In" };


//boombox main function switch and tape a, b state bitmap
#define FUNC_NONE 0
#define FUNC_AM   1 //AM
#define FUNC_FM   2 //FM
#define FUNC_TA   3 //TAPE (DAB) B Deck
#define FUNC_PB   4 //PLAYBACK A Deck
uint8_t currentFunction = FUNC_NONE;
const char * functionString[] = { "None", "AM Radio", "FM Radio", "B Deck", "A Deck" };

//ENUMs
#define STMO_NONE   0
#define STMO_MONO   1
#define STMO_STEREO 2
#define STMO_SLIDE  3

#define MIME_UNKNOWN  0
#define MIME_AUDIO    1
#define MIME_TEXT     2

//wifi state machine stream state
#define STREAM_GATHER   0
#define STREAM_DATALEN  1
#define STREAM_DATA     2
#define STREAM_IP       3
#define STREAM_MAC      4
#define STREAM_RSSI     5

#define TYPE_NONE   0
#define TYPE_MP3    1
#define TYPE_AAC    2
#define TYPE_FLAC   3
#define TYPE_OGG    4
#define TYPE_WMA    5
uint8_t streamType = TYPE_NONE;
const char * streamTypeString[] = { "", "MP3", "AAC", "FLAC", "OGG", "WMA" };


//wifi state machine lan state
#define WLAN_UNINIT       0
#define WLAN_SETMODE      1
#define WLAN_IDLE         2
#define WLAN_CONNECTING   3
#define WLAN_GETIP        4
#define WLAN_SETBAUD      5
#define WLAN_SETMUX       6
#define WLAN_CIPSSL       7
#define WLAN_CLOSING      8
#define WLAN_CONNECTED    9
#define WLAN_CIPSTART     10
#define WLAN_CIPSEND      11
#define WLAN_CIPSENDING   12
#define WLAN_REQRSSI      13
#define WLAN_WIFIFIND     14
#define WLAN_WIFISCAN     15
#define WLAN_DISCONNECT   16
const char * wlanStateString[] = { "Uninitialised", "Set Station Mode", "Not Connected", "Connect to AP", 
            "Get IP/MAC", "Set baud rate", "Set connections", "Set secure sockets", "Close host", "Connected", "Open Host",
            "Send Request", "Wait for Host", "Request RSSI", "Find Networks", "Scan Networks", "Disconnecting"};

const char * wlanErrorString[] = { "Unknown", "Connection timeout", "Wrong password", "Cannot find AP",  "Connection failed" };

//EEPROM settings structures
#define EE_MAGIC  0x45

//presets
struct __attribute__ ((packed)) presetObject{
  char name[35] = "<Empty>";  
  uint8_t mode;
};

//wifi creds
struct __attribute__ ((packed)) networkObject{
  char ssid[16];
  char password[32];
};

//EEPROM settings structure
struct __attribute__ ((packed)) settingsObject{
  uint8_t id;
  uint8_t mode;
  uint8_t vsVolume;
  uint16_t vsTone;
  int8_t balance;
  uint8_t dabVolume;
  uint8_t dabEQ;
  char dabChannel[35];
  int32_t dabFM;
  uint16_t playlistIndex;
  char server[256];
  //Credentials
  uint8_t currentNetwork;
  networkObject networks[4];
  //Presets
  presetObject presets[15];
};

//Default settings
settingsObject settings = {
  EE_MAGIC,
  0,          //mode
  90,        //vsVolume
  0x0806,     //vsTone
  0,          //Balance
  16,         //dabVolume
  0,         //dabEQ
  "MMM HARD N HEAVY",         //dabChannel
  105100,      //dabFM
  0,          //playlistIndex (none)
  "http://radio1.internode.on.net:8000/296",         //Server
  0,          //Current Network
  { { "<Empty>", "" }, //password
    { "<Empty>", "" },
    { "<Empty>", "" },
    { "<Empty>", "" }
  },
  {}
};

//DAB slideshow image thumbnail
uint16_t dab_img_data[80*60] = {0};
static lv_img_dsc_t dab_img_dsc = {
    .header = {
      .cf = LV_IMG_CF_TRUE_COLOR,          /*Set the color format*/
      .always_zero = 0,
      .reserved = 0,
      .w = 80,
      .h = 60,
    },
    .data_size = 80 * 60 * LV_COLOR_DEPTH / 8,
    .data = (uint8_t*)dab_img_data,
};

//custom serial device also outputs to onscreen terminal
class mySerial : public Stream {
  public:
    mySerial():Stream(Serial){}
    int read(void) { return Serial.read(); }
    size_t write(uint8_t ch);
    int available(void) { return Serial.available(); }
    int peek(void) { return Serial.peek(); }
    bool InSetup = true;
};
mySerial serial;


//---------------------------------------------------------------------------------

//Teensy hardware
const uint32_t SERIAL_BAUD_RATE = 115200;


//ILI9341 SPI TFT/Touch/SD Pin connections
// On Teensy 4
#define TFT_RST 23
#define TFT_DC 10
#define TFT_CS 9
#define TFT_SCK 13
#define TFT_MISO 12
#define TFT_MOSI 11
#define TFT_BL  15
#define TOUCH_CS 14
#define SDCARD_CS 8

// This is calibration data for the raw touch data to the screen coordinates
// Warning, These are
#define TS_MINX 360
#define TS_MINY 200
#define TS_MAXX 3950
#define TS_MAXY 3900

// VS1053 SPI interface pinout
// uses main SPI bus
#define VS_RST  31
#define VS_CS   2
#define VS_DCS  3
#define VS_DRQ  7

// ESP8266 WLAN
//Remember Teensyduino needs modification..
const int wlanSerialBufferSize = 16384;
uint8_t wlanSerialBuffer[wlanSerialBufferSize];
#define WLAN_SERIAL     Serial5
#define WLAN_SERIAL_BAUD_INIT 74880
#define WLAN_SERIAL_BAUD 115200
#define WLAN_BAUD       (115200 * 20) //20 is too unsafe with LVGL8?
#define WLAN_FLOWCONTROL 3 //0:disable, 1:enable RTS, 2:enable CTS, 3:both
#define CONNECTION_TIMEOUT 4

#define WLAN_RESET      19
#define WLAN_ENABLE     18  //CH_PD
#define WLAN_CTS        4   //GPIO13
#define WLAN_RTS        6   //GPIO15
#define WLAN_GPIO0      5   //Shared with IR_SND !

// Monkeyboard DAB radio
#define DAB_SERIAL     Serial7
