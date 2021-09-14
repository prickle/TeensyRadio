//No long periods with interrupts disabled please
// Hardware serial RX buffer can overflow very quickly.
// Missed bytes manifest as a spew of random ascii and loss of audio sync :(

enum webDataMode_t { INIT = 1, HEADER = 2, DATA = 4,
                  METADATA = 8, PLAYLISTINIT = 16,
                  PLAYLISTHEADER = 32, PLAYLISTDATA = 64,
                  TEXT = 128
                } ;        // State for datastream

webDataMode_t    webDataMode ;                                // State of datastream
int              webMetadataInterval = 0 ;                             // Number of databytes between metadata
int              webBitrate ;                                 // Bitrate in kb/sec
bool             webChunkedTransfer = false ;                         // Station provides chunked transfer
int              webChunkCount = 0 ;                          // Counter for chunked transfer
int              webMetaCountdown ;                               // Counter databytes before metadata
int              webMetaCount ;                               // Number of bytes in metadata
char             webMetaLine[256] ;                                // Readable line in metadata
//char             currentMetaLine[256] ;                           // Last readable line in metadata
char             webPlaylistURL[256] ;                                // The URL of the specified playlist
int8_t           webPlaylistEntry = 0 ;                        // Nonzero for selection from playlist

//#define WLAN_ECHO


#ifdef WLAN_ECHO
class Wlan : public HardwareSerial {
  public:
    Wlan():HardwareSerial(WLAN_SERIAL){}
    virtual inline int read(void) {
      int ch = WLAN_SERIAL.read();
      if (ch >= 0 && echo) Serial.write(ch);
      return ch;
    }
    virtual inline size_t write(uint8_t ch) { return WLAN_SERIAL.write(ch); }
    virtual inline int available(void) { return WLAN_SERIAL.available(); }
    virtual inline bool attachRts(uint8_t pin) { return WLAN_SERIAL.attachRts(pin); }
    virtual inline bool attachCts(uint8_t pin) { return WLAN_SERIAL.attachCts(pin); }
    virtual inline void addMemoryForRead(void *buffer, size_t length) { return WLAN_SERIAL.addMemoryForRead(buffer, length); }
    int echo;
};
Wlan wlan;
#else
#define wlan  WLAN_SERIAL
#endif



//Enter ESP8266 Bootloader
void espPassthru(bool bootloader) {
  tft.fillScreen(ILI9341_BLACK);
  if (!WLANFound) {
    tft.setTextColor(ILI9341_YELLOW);
    tft.setFont(Arial_18);
    tft.setCursor(16, 40);
    tft.print("ESP8266 not responding!");
  }

  if (bootloader) {
    //Reset ESP
    digitalWrite(WLAN_RESET, LOW);
    delay(15);
    digitalWrite(WLAN_ENABLE, HIGH);
    digitalWrite(WLAN_RESET, HIGH);
    WLAN_SERIAL.begin(WLAN_SERIAL_BAUD_INIT);
    //The ESP - Bootloader waits for LOW on WLAN_GPIO0
    digitalWrite(WLAN_GPIO0, LOW);
  }
  
  tft.setTextColor(ILI9341_GREEN);
  tft.setFont(Arial_24);
  tft.setCursor(40, 100);
  if (bootloader)
    tft.print("ESP Flash Mode");
  else 
    tft.print("ESP AT Control");
  tft.setFont(Arial_12);
  tft.setCursor(80, 156);
  tft.print("DTR");
  tft.drawCircle(60, 160, 6, ILI9341_RED);
  tft.setCursor(80, 186);
  tft.print("RTS");
  tft.drawCircle(60, 190, 6, ILI9341_RED);
  while(1) {
    int dtr = Serial.dtr();
    int rts = Serial.rts();
  
    if (dtr) tft.fillCircle(60, 160, 6, ILI9341_RED);
    else tft.drawCircle(60, 160, 6, ILI9341_RED);
    if (rts) tft.fillCircle(60, 190, 6, ILI9341_RED);
    else tft.drawCircle(60, 190, 6, ILI9341_RED);
  
    //Data Transfer
    while (Serial.available()){
      WLAN_SERIAL.write(Serial.read());
    }
  
    while (WLAN_SERIAL.available()){
      Serial.write( WLAN_SERIAL.read() );
    }
  }
}

//------------------------------------------------------------------------------
// actual web stuff begins

//index of selected network in settings
uint8_t wifiNetwork = 0;

//my network info as reported by esp8266
char myIP[20] = "";
char myMAC[20] = "";

//webserver response tags
const char content_type[] = "content-type:";
const char content_length[] = "content-length:";
const char transfer_encoding[] = "transfer-encoding:";

//icecast tags of interest
const char icy_description[] = "icy-description:";
const char icy_genre[] = "icy-genre:";
const char icy_name[] = "icy-name:";
const char icy_url[] = "icy-url:";
const char icy_br[] = "icy-br:";
const char icy_metaint[] = "icy-metaint:";
#define SMETA(icytag,sdata) if (strncmp(icytag, linebuf, strlen(icytag)) == 0) \
                                  { strncpy(sdata, linebuf + strlen(icytag), sizeof(sdata)-1); }

//icecast server stream data storage
struct streamData {
  char description[128];
  char genre[128];
  char name[128];
  char url[128];
  int samplerate;
} streamData = {{0}, {0}, {0}, {0}};
bool newStreamData = false;
bool newMetaData = false;

const char *httpHeader = "HTTP/1";
//const char *ipdHeader = "+IPD,1";
//asynchronous stream control
bool reqNewServer = false;
bool reqOpenStream = false;
bool reqCloseStream = false;
bool reqRSSI = false;
bool reqNewNetwork = false;
bool reqWifiScan = false;
bool reqDisconnect = false;
bool reqUpdateStat = false;

//Main webradio parser state
int streamState = 0;                          //state machine

//Wireless LAN state
int wlanState = WLAN_UNINIT;
int getWlanState() {
  if (wlanState == WLAN_REQRSSI)        //ignore REQRSSI state (too noisy)
    return WLAN_CONNECTED;
  return wlanState;
}

int wifiConnAttempts = 0;
bool collectingAPEntry = false;
bool cwlapComplete = false;
char apEntry[128];

//Current server information storage
char radioServer[256];
int radioPort;
char radioStream[256];
char radioRequest[512] = {0};
bool hostIsPlaylist = false;
bool hostIsSecure = false;

bool isWifiConnected() {
  return wlanState >= WLAN_CONNECTED;
}


//Action from stations list
static void webRadioListAction(lv_event_t * event)
{
  lv_obj_t * obj = lv_event_get_target(event);
  strncpy(webStationName, lv_list_get_btn_text(dabStationList, obj), 34);
  webStationName[34] = '\0';
  serial.printf("> Clicked: %s\n", webStationName);
  char* url = (char*)lv_obj_get_user_data(obj);
  listSetSelect(obj);
  serial.println(url);
  lv_tabview_set_act(tabView, mainWindowIndex, LV_ANIM_ON);
  closeStream();
  if (strncmp(settings.server, url, 255) != 0) {
    strncpy(settings.server, url, 255);
    settings.server[255] = '\0';
    writeSettings();
  }  
  updateUrlEditText();
  connectToHost(url);
}

bool resetWLAN() {
  WLAN_SERIAL.begin(WLAN_SERIAL_BAUD);

  pinMode(WLAN_RESET, OUTPUT);
  pinMode(WLAN_ENABLE, OUTPUT);
  pinMode(WLAN_CTS, OUTPUT);
  pinMode(WLAN_GPIO0, OUTPUT);

  digitalWrite(WLAN_CTS, LOW);
  digitalWrite(WLAN_ENABLE, LOW);

  //Reset ESP
  digitalWrite(WLAN_RESET, LOW);
  delay(15);
  digitalWrite(WLAN_ENABLE, HIGH);
  digitalWrite(WLAN_RESET, HIGH);

  //The ESP - disable bootloader
  digitalWrite(WLAN_GPIO0, HIGH);

  //300ms to give "ready"
  unsigned long timer = millis();
  bool ret = false;
  char line[256];
  int lbi = 0;
  while (millis() < timer + 300 && !ret) {
    lv_refr_now(display);
    if (WLAN_SERIAL.available()) {
      char ch = WLAN_SERIAL.read();
      if (ch != '\r' && ch != '\n' && lbi < 255) line[lbi++] = ch;
      if (ch == '\n') {
        line[lbi] = '\0';
        if (strstr(line, "ready")) ret = true;
        lbi = 0;
      }
    }
  }
  if (ret) {
    //Reset all state
    wlanState = WLAN_UNINIT;
    if (streamStarted) {
      streamStarted = false;
      stopVSDecoding();
    }
    streamState = 0;
    reqNewServer = false;
    reqOpenStream = false;
    reqCloseStream = false;
    reqRSSI = false;
    reqNewNetwork = false;
    reqWifiScan = false;
    reqDisconnect = false;
    reqUpdateStat = false;
    collectingAPEntry = false;
    cwlapComplete = false;
  }
  return ret;
}

//Start the WIFI network and connect to AP
//Starts by setting the Access Point mode (Station) then connects to chosen network
void wlanConnect() {
  if (wlanState == WLAN_UNINIT) {
    wlan.println("AT+CWMODE_CUR=1");
    wlanState = WLAN_SETMODE;
  }
  if (!isWifiConnected())
    wlanConnect(settings.currentNetwork);
#ifdef WLAN_ECHO
  wlan.echo = true;
#endif
}

//just connect to a given network
void wlanConnect(uint8_t network){
  wifiConnAttempts = 0;
  wifiNetwork = network;
  reqNewNetwork = true;
}

//Called to request the signal strength icon to update
void wlanRSSI(){
  reqRSSI = true;
}

//Called to request a wifi scan
uint8_t wifiAPCount = 0;
void startWifiScan(){
  reqWifiScan = true;
}

//Connect to a requested URL
void connectToHost(const char * host) {
  strncpy(requestedURL, host, 255);
  requestedURL[255] = '\0';
  reqNewServer = true;
}

//used to start or restart the current server's stream
// call to attempt reload
void openStream() {     
  reqOpenStream = true;
}

//close the current server and stop streaming 
void closeStream() {
  reqCloseStream = true;
}

void wlanDisconnect() {
  if (wlanState != WLAN_IDLE) reqDisconnect = true;
}

//-----------------------------------------------------------------
//internal routines
bool strEndsWith( const char *string , const char *end) {
  string = strrchr(string, end[0]);
  if( string != NULL )
    return( !strcmp(string, end) );
  return false;
}

//Parse and split a URL then try open the stream
void parseHost(char * host) {
  char buf[256] = "";      //host address - Host without extension and portnumber
  char getreq[256] = "";   //GET request
  char ext[256] = "";      //extension - May be like "/mp3" in "skonto.ls.lv:8002/mp3"
  strncpy (buf, host, 255); //don't alter passed in host string 
  char * addr = buf;       //pointer to address
  //strip http
  if (strncmp(addr, "http://", 7) == 0) addr += 7;      // http://? remove
  if ((hostIsSecure = (strncmp(addr, "https://", 8) == 0))) addr += 8;      // https://? remove
  // In the URL there may be an extension
  char * inx = strchr(addr, '?');
  if ( inx > 0 ) {                                   // Is there an extension?
    strncpy(getreq, inx, 255);
    getreq[255] = '\0';
    *inx = '\0';
  }
  // see if this is a playlist
  hostIsPlaylist = ( strEndsWith (addr,  ".m3u" ) || strEndsWith (addr, ".m3u8" ) ); // Is it an m3u playlist?
  if (hostIsPlaylist)  {
    strncpy(webPlaylistURL, addr, 255);
    webPlaylistURL[255] = '\0';
    if ( webPlaylistEntry == 0 ) webPlaylistEntry = 1 ;             // First entry to play? Yes, set index
    serial.printf ( "> Playlist requested, looking for entry %d\r\n", webPlaylistEntry ) ;
  }
  // In the URL there may be an extension
  inx = strchr(addr, '/');                      // Search for begin of extension
  if ( inx > 0 ) {                                    // Is there an extension?
    strncpy(ext, inx, 255);            // Yes, change the default
    ext[255] = '\0';
    *inx = '\0';                          // Host without extension
  }
  // In the URL there may be a portnumber
  radioPort = hostIsSecure?443:80 ;                           // Port number for host
  inx = strchr(addr,  ':');                      // Search for separator
  if ( inx > 0 ) {                                  // Portnumber available?
    radioPort = atoi(inx + 1);     // Get portnumber as integer
    *inx = '\0';                   // Host without portnumber
  }
  //Save radio server address
  strncpy(radioServer, addr, 255);
  strncpy(radioStream, ext, 255);
  strncat(radioStream, getreq, 255);
  //Construct a request
  strcpy(radioRequest, "GET ");
  strcat(radioRequest, radioStream);
  strcat(radioRequest, " HTTP/1.0\r\nHOST:");
  strcat(radioRequest, radioServer); 
  strcat(radioRequest, "\r\nUser-Agent:Teensyradio\r\nicy-metadata:1\r\nicy-reset:1\r\nicy-br:128\r\n\r\n");
  //connect to server and send the request
  openStream();
}

//(Re)start the webradio parser
void initRadioReader() {
  if (hostIsPlaylist)
    webDataMode = PLAYLISTINIT ;                       // Yes, start in PLAYLIST mode
  else webDataMode = INIT ;                                 // Start default in metamode
}

//Main webradio parser 
// it is told how many bytes to read from the stream 
// it interprets the stream and fills the fifo buffer
// this is emptied by the playBuffer ISR in the SD player
// it returns how many bytes were actually read in this pass
int readRadio(int cntData) {
  const size_t lbsize = 1024;
  static char linebuf[lbsize] = {0};
  static unsigned lbi;
  static unsigned mp3bufidx;
  const size_t mp3bufsize = 256;
  static uint8_t mp3buf[mp3bufsize];
  static uint16_t  playlistcnt ;                       // Counter to find right entry in playlist
  static bool      ctseen;                    // First line of header seen or not
  static uint8_t   contentType;
  static int       LFcount ;                           // Detection of end of header
  //static bool      firstchunk = true ;                 // First chunk as input
  static bool      firstmetabyte ;                     // True if first metabyte (counter)
  static bool      textResponseShown ;                     // True if server response has been shown
  static int32_t   contentLength = -1;
  
  char ch;
  
  //Initialise
  // begin! clear all persistent internal state
  if ( webDataMode == INIT )                              // Initialize for header receive
  {
    ctseen = false ;                                   // Contents type not seen yet
    contentType = MIME_UNKNOWN;
    webMetadataInterval = 0 ;                                      // No metaint found
    LFcount = 0 ;                                      // For detection end of header
    webBitrate = 0 ;                                      // Bitrate still unknown
    webDataMode = HEADER ;                                // Handle header
    webMetaLine[0] = '\0' ;                                    // No metadata yet
    contentLength = -1;
    firstmetabyte = false ;                            // Not the first
    lbi = 0;
    mp3bufidx = 0;
    textResponseShown = false;
    webChunkedTransfer = false ;                                 // Assume not chunked
    memset((void*)&streamData, 0, sizeof(streamData));
  }

  //Parse Response Header
  // the server has started communicating with us. 
  // check the header for sanity and look for important information
  if (webDataMode == HEADER) {
    //digitalWrite(WLAN_CTS, LOW);                          //make sure data flowing
    while ((cntData > 0) && (wlan.available() > 0) ) {    //stuff for us in the stream?
      yield();
      cntData--;
      ch =  wlan.read();                  //read the stream from the esp8266
      //build a line
      if (ch != '\r' && ch != '\n' && lbi < lbsize - 1) {
        LFcount = 0;            //reset empty line feed counter
        linebuf[lbi++] = ch;
      }
      //got a full line
      if (ch == '\n') {
        LFcount++;                //count possibly empty line feeds
        linebuf[lbi] = 0;         //null terminate the line
        //Start looking at header lines presented by the server
        if (strncasecmp(httpHeader, linebuf, 6) == 0) {
          if (!strstr(linebuf, "200")) { //not OK
            serial.print("> Host: ");
            serial.println(linebuf);
            if (progTextLbl) {
              char str[265];
              sprintf(str, LV_SYMBOL_WARNING " %s", linebuf);
              lv_label_set_text(progTextLbl, str);
            }
          }
        }
        if (strncasecmp(content_type, linebuf, strlen(content_type)) == 0) {          //MIME type
          ctseen = true ;                              // Yes, remember seeing this
          if (strstr(linebuf, "audio")) {
            contentType = MIME_AUDIO;
            if (strstr(linebuf, "mpeg")) streamType = TYPE_MP3;
            if (strstr(linebuf, "aac")) streamType = TYPE_AAC;
          }
          else if (strstr(linebuf, "text")) contentType = MIME_TEXT;
          else contentType = MIME_UNKNOWN;
        }
        //icecast stream info
        else SMETA(icy_description, streamData.description)
        else SMETA(icy_genre, streamData.genre)
        else SMETA(icy_name, streamData.name)
        else SMETA(icy_url, streamData.url)
        else if (strncasecmp(icy_metaint, linebuf, strlen(icy_metaint)) == 0) 
          webMetadataInterval = strtol(linebuf + strlen(icy_metaint), NULL, 10);
        else if (strncasecmp(icy_br, linebuf, strlen(icy_br)) == 0) 
          webBitrate = strtol(linebuf + strlen(icy_br), NULL, 10);
        //server-specific stuff
        else if (strncasecmp(content_length, linebuf, strlen(content_length)) == 0) 
          contentLength = strtol(linebuf + strlen(content_length), NULL, 10);
        else if (strncasecmp(transfer_encoding, linebuf, strlen(transfer_encoding)) == 0)
        {
          // Station provides chunked transfer
          if (strncasecmp("chunked", linebuf + strlen(linebuf) - 7,  7) == 0) {
            webChunkedTransfer = true ;                           // Remember chunked transfer mode
            webChunkCount = 0 ;                           // Expect chunkcount in DATA
            serial.println("> Chunked data transfer");
          }
        }
        //double-LF end of server response. switch to DATA(audio) or TEXT mode
        else if (lbi < 2 && LFcount == 2 && ctseen) {
          if (contentType == MIME_AUDIO) {
            webDataMode = DATA ;                              // Expecting data now
            webMetaCountdown = webMetadataInterval ;                          // Number of bytes before first metadata
            serial.printf ( "> Audio stream found: Bitrate is %d"     // Show bitrate
                       ", metaint is %d, content length is %d\r\n",                  // and metaint
                       webBitrate, webMetadataInterval, contentLength ) ;
            //Fill in the UI
            if (contentLength != 0) newStreamData = true;
            reqUpdateStat = true;
          } else if (contentType == MIME_TEXT) {
            //huh? the server wants to say something
            serial.println ( "> Text stream found.");
            webDataMode = TEXT ;                              // Expecting data now
          } else {
            serial.println("> Unknown MIME type?"); //dunno
            if (progTextLbl) lv_label_set_text(progTextLbl, " " LV_SYMBOL_WARNING " - Unknown data type!");  //Set the text
          }
          //Prepare to collect the rest of the server's response
          lbi = 0;          
          streamStarted = true;
          break;
        }
        lbi = 0;
      }
    }
  }

  //Fill FIFO with a new packet
  // music data is coming. give it to the Codec
  if ( webDataMode == DATA ) {
    //SETLED(0, 1);
    //Buffer nearly empty, stop the music to let it refill
    if (!buffering && fifo.used() < 4096) {
      buffering = true; //low water
      reqUpdateStat = true;
      serial.println("> Buffering..");
    }
    //Buffer has filled enough, restart the music
    if (buffering && fifo.used() > 16384) {
      buffering = false;
      reqUpdateStat = true;
      serial.println("> Begin Playing..");
    }
    //music data is waiting. read it into fifo
    while ((cntData > 0) && (wlan.available()) && (fifo.available() >= mp3bufsize)) {      
      yield();
      //read chunks into the intermediate buffer
      cntData--;
      mp3buf[mp3bufidx++] = wlan.read();
      //dump complete chunks into FIFO
      if (mp3bufidx >= mp3bufsize) {
        mp3bufidx = 0;    
        //noInterrupts();
        fifo.write(mp3buf, mp3bufsize);
        //interrupts();
        bytesPerSecondCount += mp3bufsize;
      }
      //check for end of datablock if stream has metadata
      if ( webMetadataInterval != 0 ) {                               // No METADATA on Ogg streams or mp3 files
        if ( --webMetaCountdown == 0 ) {                         // End of datablock?
          //deal with leftover data in the intermediate buffer if any
          if ( mp3bufidx ) {                                 // Yes, still data in buffer?
            //noInterrupts();
            fifo.write ( mp3buf, mp3bufidx ) ;     // Yes, send to player
            //interrupts();
            mp3bufidx = 0 ;                                 // Reset count
          }
          //look for metadata next round
          webDataMode = METADATA ;
          firstmetabyte = true ;                         // Expecting first metabyte (counter)
          break;
        }
      }
      //reached end of content? server will have closed, so simply connect to the next playlist entry
      if (contentLength >= 0) {
        if (--contentLength == 0 && strlen(webPlaylistURL) != 0) {
          webPlaylistEntry++;
          connectToHost(webPlaylistURL);
          break;
        }
      }
    }
  }

  //look for stream metadata
  if ( webDataMode == METADATA ) {                        // Handle next byte of metadata
    //digitalWrite(WLAN_CTS, LOW);                      //make sure stream is flowing
    //stuff in the stream for us?
    while ((cntData > 0) && (wlan.available() > 0) ) {
      //get a byte
      yield();
      cntData--;
      ch = wlan.read();
      //so this is our first bit of metadata. initialise and make sure we know where to look for more
      if ( firstmetabyte ) {                              // First byte of metadata?
        firstmetabyte = false ;                          // Not the first anymore
        webMetaCount = ch * 16 + 1 ;                         // New count for metadata including length byte
        lbi = 0;
      }
      //accumulate the metadata
      else if (lbi < lbsize - 1)                        //discard first character
        linebuf[lbi++] = (char)ch ;                            // Normal character, put new char in metaline
      //is that it? lets have a look then
      if ( --webMetaCount == 0 ) {
        linebuf[lbi] = 0;
        if (strlen(linebuf) > 0) {                        // Any info present?
          // Isolate the StreamTitle, skip leading quote and null trailing quote if present.
          char * start = strstr(linebuf, "StreamTitle="); 
          char * end = NULL; 
          if (start) {                            //found "StreamTitle="
            start += 12;                          //skip "StreamTitle="
            char * quot = strchr(start, '\'');
            if (quot) {                           //found leading quote
              start = quot + 1;                   //skip it
              end = strchr(start, ';');          //look for trailing semicolon
              if (end) {
                *end = '\0';                     //truncate trailing semicolon
                end = strrchr(start, '\'');          //look for trailing quote
                if (end) *end = '\0';               //truncate trailing qoute
              }
            }
          } else start = linebuf;                 //no? show the whole thing..
          stripHtml(start);
          //Check for changes .. compared to prevent repeating metadata
          if (strncmp(webMetaLine, start, 255) != 0) {
            strncpy(webMetaLine, start, 255);
            webMetaLine[255] = '\0';
            newMetaData = true;
          }
        }
        if ( strlen(linebuf) == lbsize - 1 ) {                 // buffer tried to overflow?
          serial.println( "> Metadata block too long! Skipping all Metadata from now on." ) ;
          newMetaData = false;
          webMetadataInterval = 0 ;                                  // Probably no metadata
          webMetaLine[0] = '\0' ;                                // Clear metaline
        }
        webMetaCountdown = webMetadataInterval ;                            // Reset data count
        mp3bufidx = 0 ;                                  // Reset buffer count
        webDataMode = DATA ;                                // Expecting data
        break;
      }
    }
  }

  //this isn't music data? some kind of message from the server is coming through, let's have a look
  if ( webDataMode == TEXT ) {                   // Read header
    //digitalWrite(WLAN_CTS, LOW);          //get your flow on
    //build incoming data into lines
    while ((cntData > 0) && (wlan.available() > 0) ) {
      yield();
      cntData--;
      ch = wlan.read();
      if (ch != '\r' && ch != '\n' && lbi < lbsize - 1) {
        LFcount = 0;
        linebuf[lbi++] = ch;
      }
      //got a full line?
      if (ch == '\n') {
        LFcount++;
        linebuf[lbi] = 0;
        lbi = 0;
        //remove any html and send it to the UI for examination
        stripHtml(linebuf);
        serial.printf ( "] %s\r\n", linebuf);                // Show line
        if ( !textResponseShown && strlen(linebuf) > 0) {          
          char str[256] = " " LV_SYMBOL_WARNING " - ";
          strncpy(&str[7], linebuf, 248);
          str[255] = '\0';
          if (progTextLbl) lv_label_set_text(progTextLbl, str);  //Set the text
          textResponseShown = true;
        }
      }
    }
  }

  //alternate entry point to the radio parser  
  //so the URL indicated we would find a playlist.
  // what we will do is read it in, go through the entries and play them one by one
  // this handles nested playlists
  // off we go!
  if ( webDataMode == PLAYLISTINIT ) {                     // Initialize for receive .m3u file
    // We are going to use metadata to read the lines from the .m3u file
    lbi = 0 ;                                    // Prepare for new line
    LFcount = 0 ;                                      // For detection end of header
    webDataMode = PLAYLISTHEADER ;                        // Handle playlist data
    playlistcnt = 1 ;                                  // Reset for compare
    //serial.println ( "> Read from playlist" ) ;
  }
  
  //Header response from the server
  // we arent paying much attention here, just interested in the body
  if ( webDataMode == PLAYLISTHEADER ) {                   // Read header
    //digitalWrite(WLAN_CTS, LOW);
    while ((cntData > 0) && (wlan.available() > 0) ) {
      yield();
      cntData--;
      ch = wlan.read();
      if (ch != '\r' && ch != '\n' && lbi < lbsize - 1) {
        LFcount = 0;
        linebuf[lbi++] = ch;
      }
      if (ch == '\n') {
        LFcount++;
        linebuf[lbi] = 0;
        lbi = 0;
        //may as well share the blah blah
        //serial.printf ( "] %s\r\n", linebuf);                // Show playlistheader
        if ( LFcount == 2 ) {
          //done here, moving on
          webDataMode = PLAYLISTDATA ;                      // Expecting data now
          break;
        }
      }
    }
  }

  //right, give us the playlist entries
  if ( webDataMode == PLAYLISTDATA )                      // Read next byte of .m3u file data
  {
    //digitalWrite(WLAN_CTS, LOW);          //keep streaming please..
    bool foundEntry = false;              //if entry was found
    //stuff for us? line by line please
    while ((cntData > 0) && (wlan.available() > 0) ) {
      yield();
      cntData--;
      ch = wlan.read();
      if ( ( ch < 0x7F ) &&                               // Ignore unprintable characters
           ( ch != '\r' ) &&                              // Ignore CR
           ( ch != '\n' ) &&
           ( ch != '\0' ) &&                                  // Ignore NULL
           ( lbi < lbsize - 1 )) {                              // Don't overflow
        linebuf[lbi++] = ch;
      }
      //got a full line, see what it is
      if ( ch == '\n' ) {                             // Linefeed ?
        linebuf[lbi] = 0;
        //tell the world!
        //serial.print("] ");                   // Show playlistheader
        //serial.println(webMetaLine) ;
        if ( lbi > 5 ) {                     // Skip short lines
          //here is the name of the entry. nice to know
          if ( strstr( linebuf, "#EXTINF:" ) >= 0 ) {     // Info?
            if ( webPlaylistEntry == playlistcnt ) {            // Info for this entry?
              char * inx = strchr(linebuf, ',') ;             // Comma in this line?
              if ( inx > 0 ) {
                // Show artist and title if present in metadata
                serial.printf ( "> Stream Title: %s\r\n", &inx[1]) ;
              }
            }
          }
          if ( linebuf[0] != '#' ) {              // Commentline? ignore.
            // Now we have an URL for a .mp3 file or stream or maybe another playlist.  
            // so it the right one?  look for the webPlaylistEntry'th entry
            serial.printf ( "> Entry %d in playlist found.\r\n", playlistcnt) ;
            if ( webPlaylistEntry == playlistcnt  )
            {
              foundEntry = true;
              if (strchr(linebuf, '/') == 0) { //must be relative?
                char temp[256];
                strncpy(temp, requestedURL, 255);   //copy in the current URL
                char* ptr = strrchr(temp, '/');     //strip last branch
                *(++ptr) = '\0';                    //leave the trailing slash and null terminate for strlen
                strncpy(ptr, linebuf, 255 - strlen(temp));  //add our entry to the end
                strcpy(linebuf, temp);              //and put it back in linebuf, temp is discarded
              }
              connectToHost(linebuf);                        // Connect to it
            }
            playlistcnt++ ;                                  // Next entry in playlist
          }
        }
        lbi = 0;
      } 
    }
    //End of playlist
    if (!foundEntry) {
      webPlaylistURL[0] = '\0';
    }
  }

  return cntData;
}


//esp8266 AT command set parser
void readStream(void) {
  static unsigned cntData = 0;  
  static unsigned cntIP = 0;  
  static unsigned cntMAC = 0;  
  static char linebuf[64];
  static unsigned lbi = 0;

  //Inside +IPD data block
  if (streamState == STREAM_DATA) {
    cntData = readRadio(cntData);
    if (cntData == 0) {             //End of data block
#ifdef WLAN_ECHO
      wlan.echo = true;
#endif
      streamState = STREAM_GATHER;  // Start gathering responses
      if (wlanState == WLAN_CIPSENDING) {
        wlanState = WLAN_CONNECTED;   // Switch parser back to idle state
      }
    }
  }

  // - Parse response to AT Command
  else while ( wlan.available()) {
    char ch = wlan.read();
    if (ch != '\r' && ch != '\n')
       linebuf[lbi++] = ch;

    // Inside +IPD data length
    if (streamState == STREAM_DATALEN) {
      if (ch == ':') {
        //End of data length, data block begins
        lbi = 0;                      //reset line buffer
        streamState = STREAM_DATA;    //read the data block
#ifdef WLAN_ECHO
        wlan.echo = false;
#endif
        break;                        //stop reading stream
      }
      else if (ch < '0' || ch > '9') {
        //Error, reset
        lbi = 0;
        streamState = STREAM_GATHER;
        break;
      }
      //Accumulate data length
      else cntData = cntData * 10 + (ch - '0');
    }

    // Gathering ESP8266 responses, looking for incomplete lines
    else if (streamState == STREAM_GATHER) {
      //Got response to CIPSEND command, "> "
      if (wlanState == WLAN_CIPSEND && lbi == 2 && strncmp(linebuf, "> ", 2) == 0) {
        //Start the codec here, before streaming begins
        startSong();                    
        wlan.print(radioRequest);     //Send request
        wlanState = WLAN_CIPSENDING;   // Switch parser to sending request state
      }
      //Got data block header
      else if (lbi == 7 && strncmp(linebuf, "+IPD,1,", 7) == 0) {
        cntData = 0;                    //Reset data block length
        streamState = STREAM_DATALEN;   // Start reading data length
      }
      //Got responses to CIFSR command
      else if (lbi == 14 && strncmp(linebuf, "+CIFSR:STAIP,\"", 14) == 0) {
        cntIP = 0;
        streamState = STREAM_IP;        //Switch to ip address reading mode
      }
      else if (lbi == 15 && strncmp(linebuf, "+CIFSR:STAMAC,\"", 15) == 0) {
        cntMAC = 0;
        streamState = STREAM_MAC;       //Switch to mac address reading mode
      }
    }

    // Reading CIFSR IP address
    else if (streamState == STREAM_IP) {
      if (ch == '"') {
        myIP[cntIP] = 0;
        streamState = STREAM_GATHER;
      } else if (cntIP < 19) myIP[cntIP++] = ch;
    }

    // Reading CIFSR MAC address
    else if (streamState == STREAM_MAC) {
      if (ch == '"') {
        myMAC[cntMAC] = 0;
        streamState = STREAM_GATHER;
      } else if (cntMAC < 19) myMAC[cntMAC++] = ch;
    }
    
    //End of line
    if (ch == '\n' || lbi == 63) {
      linebuf[lbi] = 0;
      //Full line gathered from esp8266 AT protocol. 
      if (lbi > 0) {

        //Remove stray "> " from in front of responses
        if (strncmp("> ", linebuf, 2) == 0 && strlen(linebuf) > 3) {
          memmove(&linebuf[2], linebuf, 62);
        }
        //collecting AP entry
        if (collectingAPEntry) {
          collectingAPEntry = false;
          int ind = strlen(apEntry);
          if (ind + strlen(linebuf) < 128)
            strcpy(&apEntry[ind], linebuf);
          wifiScanEntry(wifiAPCount, apEntry);
        }
        //Connected report
        else if (strncmp("1,CONNECT", linebuf, 9) == 0) {
          serial.println("> Connected to server.");
        }
        //Connection closed
        else if (strncmp("1,CLOSED", linebuf, 8) == 0) {
          streamStarted = false;
          stopVSDecoding();
          //Reset FIFO
          //buffering = true;
          bytesPerSecond = 0;
          //fifo.reset();
          serial.print("> Closing Connection: ");
          wlan.setTimeout(500);
          wlan.findUntil((char*)"OK\r\n", (char*)"");   //Eat possible responses
          serial.println("OK.");
          //Clear GUI
          if (bufLvlMeter) lv_meter_set_indicator_end_value(bufLvlMeter, bufLvlIndicator, 0);    
          clearSpectrumChart();
          if (settings.mode == MODE_WEB) {
            if (progNameLbl) lv_label_set_text(progNameLbl, LV_SYMBOL_CLOSE " Closed.");
            hideWebControls();
            //if (progNowLbl) lv_label_set_text(progNowLbl, "");  //Set the text
            //if (progTextLbl) lv_label_set_text(progTextLbl, "");
            setStmoLbl("");
          }
          if (wlanState >= WLAN_CLOSING) 
            wlanState = WLAN_CONNECTED;
        }
        //Wifi early connection progress report
        else if (strncmp("WIFI CONNECTED", linebuf, 14) == 0) {
          serial.println("> Connecting to WiFi..");
          hideWebControls();
          if (progNameLbl) lv_label_set_text(progNameLbl, LV_SYMBOL_WIFI " Connected.");
        //  wlanState = WLAN_GETIP;
        }
        //Ignore the following responses
        else if (strncmp("SEND OK", linebuf, 7) == 0) {}
        else if (strncmp("WIFI GOT IP", linebuf, 11) == 0) {}
        else if (strncmp("> ", linebuf, 2) == 0) {}
        else if (strncmp("Recv ", linebuf, 5) == 0) {}
        //IP report
        else if (strncmp("+CIFSR:STAIP", linebuf, 12) == 0) {
          serial.printf ("> Got IP: %s\r\n", myIP); 
        }
        //MAC report
        else if (strncmp("+CIFSR:STAMAC", linebuf, 13) == 0) {
          serial.printf ("> Got MAC: %s\r\n", myMAC); 
        }
        //RSSI report
        else if (strncmp(linebuf, "+CWJAP_CUR:", 11) == 0) {
          char* rssiStr;
          if ((rssiStr = strrchr(linebuf, ','))) {
            int val = constrain((atoi(rssiStr + 1) + 100) * 2, 0, 100);
            char s[25] = {0};
            sprintf(s, LV_SYMBOL_WIFI " %d%%", val);
            setSigStrengthLbl(s);
          }
        }
        //WIFI AP entry
        else if (strncmp(linebuf, "+CWLAP:", 7) == 0) {
          wifiAPCount++;
          strncpy(apEntry, linebuf, 127);
          if (!strchr(linebuf, ')')) collectingAPEntry = true;
          else wifiScanEntry(wifiAPCount, apEntry);
        }
        //ESP8266 command acknowledgement
        else if (strncmp("OK", linebuf, 2) == 0) {
          
          //Response to AT+CWMODE_CUR (set wifi station mode)
          if (wlanState == WLAN_SETMODE) {
            wlanState = WLAN_IDLE;    //Ready to connect
          
          //Response to AT+CWJAP_CUR=... (connect to AP)
          } else if (wlanState == WLAN_CONNECTING) {
            wlan.println("AT+CIFSR");
            wlanState = WLAN_GETIP;
          
          //Response to AT+CIFSR (get IP and MAC)
          } else if (wlanState == WLAN_GETIP) {
            wlan.printf("AT+UART_CUR=%d,8,1,0,%d\r\n", WLAN_BAUD, WLAN_FLOWCONTROL);
            wlanState = WLAN_SETBAUD;

          //Response to AT+UART_CUR (set serial port)
          } else if (wlanState == WLAN_SETBAUD) {
            serial.printf("> Changing baud rate to %.1fMbps..\r\n", WLAN_BAUD / 1000000.0);
            delay(30);
            wlan.begin(WLAN_BAUD);
            wlan.addMemoryForRead(wlanSerialBuffer, wlanSerialBufferSize);
            if (WLAN_FLOWCONTROL & 1) wlan.attachRts(WLAN_CTS);
            if (WLAN_FLOWCONTROL & 2) wlan.attachCts(WLAN_RTS);
            NVIC_SET_PRIORITY(IRQ_LPUART8, 16);
            wlan.setTimeout(1000);
            wlan.println("AT+CIPMUX=1");
            wlanState = WLAN_SETMUX;

          //Response to AT+CIPMUX (set channel mux)
          } else if (wlanState == WLAN_SETMUX) {
            //wlan.println("AT+CIPSSLCCONF?");
            wlanState = WLAN_CONNECTED;
          //} else if (wlanState == WLAN_CIPSSL) {
            //if (progNameLbl) lv_label_set_text(progNameLbl, LV_SYMBOL_WIFI " WiFi Connected.");
           // wlanState = WLAN_CONNECTED;

          //Response to AT+CIPSTART (connect to server)
          } else if (wlanState == WLAN_CIPSTART) {
            hideWebControls();
            wlan.printf("AT+CIPSEND=1,%d\r\n", strlen(radioRequest));
            wlanState = WLAN_CIPSEND;
          }
          //Response to AT+CIPCLOSE
          else if (wlanState == WLAN_CLOSING) {
            wlanState = WLAN_CONNECTED;    //Idle state
          }

          else if (wlanState == WLAN_DISCONNECT) {
            wlanState = WLAN_IDLE;    //Idle state
          }

          else if (wlanState == WLAN_REQRSSI) {
            wlanState = WLAN_CONNECTED;    //Idle state
          }

          else if (wlanState == WLAN_WIFISCAN) {
            wlanState = WLAN_CONNECTED;    //Idle state
            cwlapComplete = true;
            //WIFI Scan when connected finished..
          }
          else if (wlanState == WLAN_WIFIFIND) {
            wlanState = WLAN_IDLE;    //Idle state
            cwlapComplete = true;
            //WIFI Scan from idle finished..
          }
        }

        //Errors..
        
        //DNS FAIL message..
        else if (strncmp("DNS Fail", linebuf, 8) == 0) {
          if (progNameLbl) lv_label_set_text(progNameLbl, LV_SYMBOL_WARNING " Not found.");
          serial.println("> Server not found.");
        }

        //Error message..
        else if (strncmp("ERROR", linebuf, 5) == 0) {
          if (wlanState == WLAN_CIPSTART || wlanState == WLAN_CIPSEND || wlanState == WLAN_CIPSENDING) {
            wlanState = WLAN_CONNECTED;
            showReloadBtn();
          }
        }
        
        else if (strncmp("+CWJAP:", linebuf, 7) == 0) {
          int error = atoi(&linebuf[7]);
          if (error < 0 && error > 4) error = 0;
          wifiConnectError(error);
        }

        else if (strncmp("WIFI DISCONNECT", linebuf, 15) == 0) {
          //retry is automatic
          if (wlanState == WLAN_CONNECTING) {
            serial.println("> Connect failed. Retry..");
          }
        }

        //Already connected? move on..
        else if (strncmp("ALREADY CONNECTED", linebuf, 18) == 0) {
          wlanState = WLAN_CONNECTED;
        }

        //Fail message..
        else if (strncmp("FAIL", linebuf, 4) == 0) {
          hideWebControls();
          if (progNameLbl) lv_label_set_text(progNameLbl, LV_SYMBOL_WARNING " Not connected.");
        }

        //Unhandled response? print it out
        else if (strncmp("AT+", linebuf, 3) != 0) {
          if (progTextLbl) lv_label_set_text(progTextLbl, linebuf);
          serial.print("--> ");
          serial.println(linebuf);
        }
        lbi = 0;    
      }
      break;
    }
  }
}


//wlan handler - called from main loop
void wlanHandle() {
  static int oldState = -1;
  static unsigned long time = millis();
  static unsigned long time1s = millis();

  //Read AT command input from the ESP
  readStream();

  //State change reporting
  if (oldState != wlanState && wlanState != WLAN_REQRSSI) {   //REQRSSI too noisy
    serial.print("> wifi: ");
    serial.println(wlanStateString[wlanState]);
    setWifiStatus(wlanState);
    oldState = wlanState;
  }

  //---- Commands follow ----

  //Connect to Access Point flag "reqNewNetwork" - only command available from idle
  if (reqNewNetwork && (wlanState == WLAN_IDLE)) {
    showLoadSpinner();
    wlan.printf("AT+CWJAP_CUR=\"%s\",\"%s\"\r\n", settings.networks[wifiNetwork].ssid, settings.networks[wifiNetwork].password);
    wlanState = WLAN_CONNECTING;
    if (progNameLbl) {
      char str[128];
      sprintf(str, LV_SYMBOL_WIFI " Connecting to %s..", settings.networks[wifiNetwork].ssid);
      lv_label_set_text(progNameLbl, str);
    }
    reqNewNetwork = false;
  }

  if ((wlanState == WLAN_CONNECTED || wlanState == WLAN_IDLE) && streamState == STREAM_GATHER) {
    //Request WIFI Scan
    if (reqWifiScan) {
      wlan.println("AT+CWLAP");
      if (wlanState == WLAN_CONNECTED) wlanState = WLAN_WIFISCAN; //scan when connected and return to connected
      if (wlanState == WLAN_IDLE) wlanState = WLAN_WIFIFIND; //scan from idle and return to idle
      wifiAPCount = 0; 
      reqWifiScan = false;
    }

    //WIFI Scan finished
    else if (cwlapComplete) {
      cwlapComplete = false;
      wifiScanComplete();
    }
  }
  
  if (wlanState == WLAN_CONNECTED && streamState == STREAM_GATHER) {
    //Request close server
    if (reqCloseStream) {
      //If the stream is started, close it
      if (streamStarted) {
        wlan.println("AT+CIPCLOSE=1"); // Stop any current wificlient connections.
        wlanState = WLAN_CLOSING;
      }
      reqCloseStream = false;
    }
    
    //Open new server
    // Wait for connections to close and no streaming and not already opening a stream
    else if (reqNewServer && !reqCloseStream && !streamStarted && !reqOpenStream) {
      //delay(50);
      reqNewServer = false;
      parseHost(requestedURL);
    }
    
    //Open current stream - internal, triggered by "reqNewServer" action and used for retry
    else if (reqOpenStream) {
      showLoadSpinner();        
      serial.printf ( "> Connect to %s\r\n", radioServer) ;
      if (progNameLbl) lv_label_set_text(progNameLbl, "Open Site..");
      if (progNowLbl) lv_label_set_text(progNowLbl, radioServer);  //Set the text
      wlan.printf("AT+CIPSTART=1,\"%s\",\"%s\",%d,%d\r\n", hostIsSecure?"SSL":"TCP", radioServer, radioPort, CONNECTION_TIMEOUT);
      wlanState = WLAN_CIPSTART;
      reqOpenStream = false;
      initRadioReader();
    }

    //Request RSSI
    else if (reqRSSI) {
      wlan.println("AT+CWJAP_CUR?"); 
      wlanState = WLAN_REQRSSI;
      reqRSSI = false;
    }

    else if (reqDisconnect) {
      wlan.println("AT+CWQAP");
      wlanState = WLAN_DISCONNECT;
      reqDisconnect = false;
    }    
  }

  //read out and display any new spectrum data
  if (newSpectrum && spectrumChart) {
    newSpectrum = false;
    if (spectrumBands != specGraphPoints) {
      lv_chart_set_point_count(spectrumChart, spectrumBands);
      specGraphPoints = spectrumBands;
    }
    lv_chart_refresh(spectrumChart);
  }

  //New stream is opened - ICY header streamData info now available
  if (newStreamData) {  
    char genreUrl[256] = "";
    //description
    if (strlen(streamData.description)) strncat(genreUrl, streamData.description, 255);
    //url
    if (strlen(streamData.url)) {
      if (strlen(genreUrl)) strncat(genreUrl, " - ", 255);
      strncat(genreUrl, streamData.url, 255);
    }
    //genre
    if (strlen(streamData.genre)) {
      if (strlen(genreUrl)) strncat(genreUrl, " - ", 255);
      strncat(genreUrl, streamData.genre, 255);
    }
    if (progTextLbl) lv_label_set_text(progTextLbl, genreUrl);
    serial.printf("> Station name: %s\r\n", streamData.name);
    if (progNowLbl) lv_label_set_text(progNowLbl, streamData.name);  //Set the text
    serial.printf("> Station URL: %s\r\n", streamData.url);
    //Keep the station's reported name if no name is associated with this station yet
    if (strlen(webStationName) == 0) {
      strncpy(webStationName, streamData.name, 34);
      webStationName[34] = '\0';
      //Smarter way to handle web station list?
      if (!stationInList(settings.server)) {
        addStationButton(webStationName, webRadioListAction, settings.server, strlen(settings.server));
        writePlaylist();
      }
    }
    newStreamData = false;
  }

  //In-stream metadata available
  if (newMetaData) {
    serial.printf ("> Stream Title: %s\r\n", webMetaLine ) ;         // Show artist and title if present in metadata
    char str[256] = LV_SYMBOL_AUDIO " - " ;
    strncat(str, webMetaLine, 255);
    strncat(str, " -", 255);
    //webMetaLine = (String)LV_SYMBOL_AUDIO + " - " + webMetaLine + " -";            
    if (progNameLbl) lv_label_set_text(progNameLbl, str);  //Set the text    
    newMetaData = false;
  }
  
  //update buffer level meter
  if (millis() - time > 100) {
    time = millis();
    if (bufLvlMeter) lv_meter_set_indicator_end_value(bufLvlMeter, bufLvlIndicator, fifo.used() * 100.0 / (FIFO_SIZE - 4096));    
    //Serial.printf("Buffer:%d KB\r\n", fifo.used() / 1024);
  }

  //update connection statistics
  if (millis() - time1s > 1000) {
    time1s = millis();
    bytesPerSecond = bytesPerSecondCount;
    bytesPerSecondCount = 0;
    reqUpdateStat = true;  
    //digitalClockDisplay();
    wlanRSSI();
  }

  if (reqUpdateStat) {
    printBufStat();
    reqUpdateStat = false;
  }
}

//show connection statistics on UI
void printBufStat() {
    char buf[64] = "";
    const char * type = streamTypeString[streamType];
    if (buffering) sprintf(buf, LV_SYMBOL_DOWNLOAD "\n%dk\n%s", bytesPerSecond / 127, type);
    else sprintf(buf, LV_SYMBOL_PLAY "\n%dk\n%s", bytesPerSecond / 127, type);
    if (bufStatLbl && bufLvlMeter) {
      lv_label_set_text(bufStatLbl, buf);  /*Set the text*/
      lv_obj_align_to(bufStatLbl, bufLvlMeter, LV_ALIGN_CENTER, 0, 4);  //Align below the label
    }
}

void clearSpectrumChart() {
  for (int i=0; i<spectrumBands; i++) {
    spectrumValues[i] =  0;
    spectrumPeakValues[i] =  0; 
  }
  newSpectrum = true;
}

//-------------------------------------------------------------
// handle (strip) some html

#define ELLIPSIS  128

//Decode a subset of: HTML or XML entity or unicode character
// entity string in 'entity'
const char PROGMEM de_unknown_P[] = "xmlReadText: unknown entity %s";
const char PROGMEM de_ununi_P[] = "xmlReadText: unknown HTML unicode %d";
int decodeEntity(char *entity){
  int ch;
  //Match entity by name
  if (strncmp(entity, "nbsp", 4) == 0) ch = ' ';
  else if (strncmp(entity, "quot", 4) == 0) ch = '\"';
  else if (strncmp(entity, "amp", 3) == 0) ch = '&';
  else if (strncmp(entity, "apos", 4) == 0) ch = '\'';
  else if (strncmp(entity, "lt", 2) == 0) ch = '<';
  else if (strncmp(entity, "gt", 2) == 0) ch = '>';
  else if (strncmp(entity, "lsquo", 5) == 0
    || strncmp(entity, "rsquo", 5) == 0)
    ch = '\'';
  else if (strncmp(entity, "sbquo", 5) == 0
    || strncmp(entity, "bdquo", 5) == 0)
    ch = ',';
  else if (strncmp(entity, "ldquo", 5) == 0
    || strncmp(entity, "rdquo", 5) == 0)
    ch = '"';
  else if (strncmp(entity, "ndash", 5) == 0
    || strncmp(entity, "mdash", 5) == 0)
    ch = '-';
  else if (strncmp(entity, "#x", 2) == 0)  //hex character reference
    ch = strtol(&entity[2], NULL, 16);
  else if (strncmp(entity, "#", 1) == 0)  //decimal character reference
    ch = strtol(&entity[1], NULL, 10);
  else ch = '\x90';
  //Check for known unicode symbols
  if (ch > 127) {
    if (ch == 160) ch = ' ';
    else if (ch == 215) ch = '*';
    else if (ch == 8211 || ch == 8212) ch = '-';
    else if (ch == 8216 || ch == 8217 || ch == 8242) ch = '\'';
    else if (ch == 8220 || ch == 8221 || ch == 8243) ch = '\"';
    else if (ch == 8230) ch = ELLIPSIS; //unicode ellipsis
    else ch = '\x90';
  }
  return ch;
} 

//Strip HTML formatting tags from a string in-place
void stripHtml(char *html) {
  char *text = html;
  char *tag = NULL;
  int state = 0;
  int ch;
  char entity[7] = {0};
  char *entity_ptr = NULL;
  char entity_ctr = 0;
  while ((ch = *html++)) {
    if (state == 0) {
      if ( ch == '<') {
        tag = html;
        state = 1;
        ch = 0;
      }
    } else if (state == 1) {
      if (ch == '>') {
        state = 0;
        if (strncasecmp(tag, "p>", 2) == 0 ||
          strncasecmp(tag, "br>", 3) == 0)
          ch = ' ';
        else ch = 0;
      } else ch = 0;
    }
    if (ch && text) {
      //Second pass - decode entities
      if (ch == '&') {
        entity_ptr = entity;
        entity_ctr = 0;
      } else if (entity_ptr != NULL) {
        if (ch == ';') {
          *entity_ptr = 0;
          ch = decodeEntity(entity);
          if (ch == ELLIPSIS) { //Ellipsis
            *text++ = '.';
            *text++ = '.';
            ch = '.';
          }
          entity_ptr = NULL;
        } else if (entity_ctr++ == 7) {
          //Not an entity? Replace it, it may be part of the body.
          u_char count;
          for (count = 0; count < 6; count++) 
            *text++ = entity[count];
          entity_ptr = NULL;
        } else *entity_ptr++ = ch;
      }
      if (entity_ptr == NULL) *text++ = ch;
    }
  }
  *text = 0;
}
