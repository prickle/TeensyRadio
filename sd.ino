
volatile int32_t vsDecodeTime = 0;
volatile uint16_t vsByterate = 0;
String metaArtist, metaAlbum, metaTitle, metaYear;
lv_fs_file_t filePlaying;             //Currently open file now playing
uint32_t fileSize = 0;                //Size of current file
bool playingMusic = false;            //music now playing flag
volatile bool endOfTrack = false;     //end-of-track flag when file finished and buffer still playing
bool fileIsFlac = false;              //bitrate is treated differently for flacs, so remember
//------------------------------------------------------------
//File player

//Play a single file
boolean playFile(const char *file) {
  while (streamStarted) {   //Still streaming?
    lv_refr_now(display);      //wait to finish closing connection
    wlanHandle();
    dab.handle();
  }
  strncpy(sdFileName, file, LV_FS_MAX_PATH_LENGTH-1);
  sdFileName[LV_FS_MAX_PATH_LENGTH-1] = '\0';
  serial.print("> SD Playing file:");
  serial.println(sdFileName);
  updateUrlEditText();
  if (isPlaylistFile(sdFileName)) {
    settings.playlistIndex = 1;
    loadPlaylist(sdFileName);
    writeSettings();
    playPlaylistFile();
    return false;
  }
  sdStop();
  if (fs_err(lv_fs_open(&filePlaying, sdFileName, LV_FS_MODE_RD), "SD Open File")) return false;
  fileSize = lv_fs_size(&filePlaying);
  //serial.print("filesize:");
  //serial.println(fileSize);
  //Clear metadata storage
  metaAlbum = "";
  metaArtist = "";
  metaTitle = file;   //Default with no metadata - show the filename
  metaYear = "";
  //Check for metadata, read if understood and jump if appropriate
  uint32_t dataPointer = 0;
  if (isFlacFile(sdFileName)) {
    fileIsFlac = true;
    useFlacDecoder(true);             //Load up the FLAC decoder
    flac_metaJumper(&filePlaying);    //no dataPointer - VS wants to see FLAC headers
    streamType = TYPE_FLAC;
  } else {
    fileIsFlac = false;
    useFlacDecoder(false);            //Reload the spectrum analyser
    if (isMP4File(sdFileName)) {
      dataPointer = mp4_metaJumper(&filePlaying, fileSize);
      streamType = TYPE_AAC;
    } else if (isMP3File(sdFileName)) {
      dataPointer = mp3_ID3Jumper(&filePlaying);
      streamType = TYPE_MP3;
    } else if (isOGGFile(sdFileName)) {     //TODO OGG metadata?
      streamType = TYPE_OGG;
    } else if (isWMAFile(sdFileName)) {     //TODO WMA metadata?
      streamType = TYPE_WMA;
    } else {
      serial.println("> Not a known filetype!");
      if (progNameLbl) lv_label_set_text(progNameLbl, file); 
      if (progNowLbl) lv_label_set_text(progNowLbl, LV_SYMBOL_WARNING " File type not known!"); 
      lv_fs_close(&filePlaying);
      return false;
    }
  }
  //Show the metadata
  if (progNameLbl) lv_label_set_text(progNameLbl, ((String)LV_SYMBOL_AUDIO + " - " + metaArtist + " - " + metaTitle + " -").c_str()); 
  String alb = (String)"Album: " + metaAlbum;
  if (metaYear != "") alb += (String)" (" + metaYear + ")";
  if (progNowLbl) lv_label_set_text(progNowLbl, alb.c_str()); 
  //Reset state
  playingMusic = true;
  endOfTrack = false;
  buffering = false;
  //Prepare for playback
  lv_fs_seek(&filePlaying, dataPointer, LV_FS_SEEK_SET);
  startVSDecoding();
  //sdPlayerHandle() takes over from here
  return true;
}

void playPlaylistFile() {
  if (settings.playlistIndex) {
    lv_obj_t* child = lv_obj_get_child(playlistMainList, settings.playlistIndex-1);
    if (child) {
      listSetSelect(child);
      void * data = lv_obj_get_user_data(child);
      if (data) playFile((const char*)data);
    } else {
      settings.playlistIndex = 0;
      writeSettings();    
    }
  }
  updatePlaylistHeader();
}

void resumePlaylist() {
  if (settings.playlistIndex) {
    loadPlaylist(PLAYLIST_PATH);
    playPlaylistFile();
  }
}

//Stop playback, close the file and turn off decoding
void sdStop() {
  if (playingMusic) {    
    playingMusic = false;  
    lv_fs_close(&filePlaying);
    stopVSDecoding();
    sdSongFinished();
  }
}

//Clear the GUI
void sdSongFinished() {
  if (progNameLbl) lv_label_set_text(progNameLbl, ""); 
  if (progNowLbl) lv_label_set_text(progNowLbl, "");      
  if (progTimeBar) lv_bar_set_value(progTimeBar, 0, LV_ANIM_OFF);
  clearSpectrumChart();
  bytesPerSecond = 0;
}

// Set volume.  Both left and right.
// Input value is 0..100.  100 is the loudest.
// Clicking reduced by using 0xf8 to 0x00 as limits.
volatile uint16_t curVSVolume;
volatile bool reqVolumeChange = false;
void setVSVolume ( uint16_t rvol ) {                // Set bass/treble (4 nibbles)
  int v = map ( rvol, 0, 100, 0xF8, 0x00 ) ;           // 0..100% to one channel
  curVSVolume = (v << 8) | v;
  reqVolumeChange = true;
}

//ST_AMPLITUDE 15:12 Treble Control in 1.5 dB steps (-8..7, 0 = off)
//ST_FREQLIMIT 11:8 Lower limit frequency in 1000 Hz steps (1..15)
//SB_AMPLITUDE 7:4 Bass Enhancement in 1 dB steps (0..15, 0 = off)
//SB_FREQLIMIT 3:0 Lower limit frequency in 10 Hz steps (2..15)
//Format tone controls into VS tone word
uint16_t formatVSTone(int8_t trebAmp, int8_t trebFreq, int8_t bassAmp, int8_t bassFreq) {
  uint16_t value = 0 ;                                  // Value to send to SCI_BASS
  value = (trebAmp - 8) & 0x0F;                 // Shift next nibble in
  value = ( value << 4 ) | (trebFreq & 0x0F);                 // Shift next nibble in
  value = ( value << 4 ) | (bassAmp & 0x0F);                 // Shift next nibble in
  value = ( value << 4 ) | (bassFreq & 0x0F);                 // Shift next nibble in
  return value;   
}

//Set tone as formatted above
volatile uint16_t curVSTone;
volatile bool reqToneChange = false;
void setVSTone ( uint16_t rtone ) {                // Set bass/treble (4 nibbles)
  curVSTone = rtone;
  reqToneChange = true;
}

//Request an update of the decode time
volatile bool reqVSTime = false;
void updateDecodeTime () {                // Read the decode time into vsDecodeTime
  reqVSTime = true;
}

//Request an update of the byte rate
volatile bool reqVSByterate = false;
void updateByterate () {                // Read the decode time into vsDecodeTime
  reqVSByterate = true;
}

//Read the spectrum analyser data
#define SA_BASE 0x1800 /* 0x1380 for VS1011 */ 
void readSpectrum() {
  vs.sciWrite(VS1053_REG_WRAMADDR, SA_BASE+2); /* If VS1011b, one dummy ReadSciReg(SCI_AICTRL3); here*/ 
  spectrumBands = vs.sciRead(VS1053_REG_WRAM); /* If VS1011b, use SCI_AICTRL3 */ 
  vs.sciWrite(VS1053_REG_WRAMADDR, SA_BASE+4); /* If VS1011b, one dummy ReadSciReg(SCI_AICTRL3); here*/ 
  for (int i=0; i<spectrumBands; i++) {
    uint16_t val = vs.sciRead(VS1053_REG_WRAM);
    spectrumValues[i] =  val & 0x3F;/* If VS1011b, use SCI_AICTRL3 */
    spectrumPeakValues[i] =  (val >> 6) & 0x3F; 
  }
  newSpectrum = true;
}

//Timer ISR - feed the VS from the FIFO
// also interact with the VS without interrupt disable
// question - are the functions I'm using ISR safe? I think they may play with interrupt enable
bool nextSong = false;
void playBuffer(void) {
  int bytesread;
  static int specCounter = 0;
  uint8_t buf[VS1053_DATABUFFERLEN];
  if (!VSFound || buffering || (!playingMusic && !streamStarted)) 
    return; // paused or stopped or broken
  // Feed the hungry buffer! :)
  if (vs.readyForData()) {
    // Read some audio data from the buffer
    bytesread = fifo.read(buf, VS1053_DATABUFFERLEN);    
    if (bytesread == 0) {
      if (endOfTrack) {
        playingMusic = false;
        serial.println("End of track.");
        endOfTrack = false;
        nextSong = true;
      } //else serial.println("VS1053 buffer starved!");
    } else vs.playData(buf, bytesread);
  }
  if (!vsUsingFlac && ++specCounter > 50) { // 50(20Hz) to 200 (5Hz)
    readSpectrum();
    specCounter = 0;
  } else if (reqVolumeChange) {
    vs.sciWrite(VS1053_REG_VOLUME, curVSVolume);
    reqVolumeChange = false;    
  } else if (reqToneChange) {
    vs.sciWrite( VS1053_REG_BASS, curVSTone ) ;                  // Volume left and right
    reqToneChange = false;    
  } else if (reqVSTime) {
    uint16_t pmsl = wram_read ( 0x1E27 );
    uint16_t pmsh = wram_read ( 0x1E28 );
    if (pmsl != wram_read(0x1E27) || pmsh != wram_read(0x1E28)) return; //Read twice and compare, discard if different
    vsDecodeTime = (int32_t)(pmsh << 16 | pmsl) / 1000;  //ms
    if (vsDecodeTime <= 0) vsDecodeTime = vs.sciRead(VS1053_REG_DECODETIME); //s 
    reqVSTime = false;    
  } else if (reqVSByterate) {
    vsByterate = wram_read(0x1e05);
    reqVSByterate = false;    
  }   
}

//Transfer data from file to FIFO in 512 byte chunks until FIFO is full
const size_t mp3buffersize = 512;
uint8_t mp3buffer[mp3buffersize];
bool fillMp3Buffer() {
  uint32_t bytesread;
  while (playingMusic && !endOfTrack && fifo.available() >= mp3buffersize) {
    if (fs_err(lv_fs_read(&filePlaying, &mp3buffer, mp3buffersize, &bytesread), "SD Fill Buffer")) { 
      endOfTrack = true;
      lv_fs_close(&filePlaying);
    } else if (bytesread == 0) {
      endOfTrack = true;
      lv_fs_close(&filePlaying);
      //wait for nextSong to trigger when buffer drains
    } else {
      noInterrupts();
      fifo.write(mp3buffer, bytesread);
      interrupts();
      bytesPerSecondCount += bytesread;
    }
    return true;
  }
  return false;
}

//Called from main loop
void sdPlayerHandle() {
  static bool oldFF = false;
  static bool oldRW = false;
  static unsigned long time = millis();
  fillMp3Buffer();                          //Keep the buffer full
  //Once-a-second gui updates
  if (millis() - time > 1000) {
    time = millis();
    if (currentFunction != FUNC_TA || settings.mode != MODE_SD) return;    
    if (playingMusic) {
      //serial.printf("Time: %d:%02d\r\n", vsDecodeTime / 60, vsDecodeTime % 60);
      uint8_t pct = 0;
      if (vsByterate != 0) {
        uint32_t total = (float)fileSize / vsByterate;
        if (fileIsFlac) total /= 4;
        pct = (vsDecodeTime * 100) / total;
        if (pct > 100) pct = 100;
        //serial.printf("Total Time: %d:%02d\r\n", total / 60, total % 60);
      }
      //serial.printf("Finished: %d%%\r\n", pct);
      if (progTimeBar) lv_bar_set_value(progTimeBar, pct, LV_ANIM_OFF);
      //serial.println("--------------------------------");
    }
    updateDecodeTime();
    updateByterate();
  }
  if (currentFunction != FUNC_TA || settings.mode != MODE_SD) return;
  //Buffer drained, start next song or stop here
  if (nextSong) {
    stopVSDecoding();     //buffer's drained, turn off the codec
    strcpy(sdFileName, "<None>");  
    updateUrlEditText();  
    //Trigger the next song in the playlist here.
    if (settings.playlistIndex++ > lv_obj_get_child_cnt(playlistMainList)) 
      settings.playlistIndex = 0;
    writeSettings();
    playPlaylistFile();
    nextSong = false;
  }
  //Skip tracks
  if (buttonFF != oldFF) {
    oldFF = buttonFF;
    if (buttonFF && !buttonRW && settings.playlistIndex < lv_obj_get_child_cnt(playlistMainList)) {
      settings.playlistIndex++;
      writeSettings();
      playPlaylistFile();
    }
  }
  if (buttonRW != oldRW) {
    oldRW = buttonRW;
    if (buttonRW && !buttonFF && settings.playlistIndex > 1) {
      settings.playlistIndex--;
      writeSettings();
      playPlaylistFile();
    }
  }
}


//-----------------------------------------------------------------------
// Metadata decoders

#define DUMP_COLS 16

void hexDump(uint8_t* addr, size_t size) {
    for (size_t row = 0; row < size; row += DUMP_COLS) {
        size_t end = min(row + DUMP_COLS, size);
        for (size_t i = row; i < end; ++i) {
            char str[4];
            sprintf(str, "%02X", (uint8_t)addr[i]);
            Serial.print(str);
            if (i % 2 && i != size-1)
                Serial.print(" ");
        }
        Serial.print("    ");
        for (size_t i = row; i < end; ++i) {
            char str[2] = {(char)addr[i], 0};
            if (str[0] < 32 || str[0] >= 127)
                str[0] = '.';
            Serial.print(str);
        }
        Serial.println("");
    }
} 

//------------------------
// Type checks

boolean isKnownMusicFile(const char * filename) {
  if (isMP3File(filename)) return true;
  if (isMP4File(filename)) return true;
  if (isOGGFile(filename)) return true;
  if (isFlacFile(filename)) return true;
  return false;  
}

boolean isMP3File(const char* fileName) {
  return (strlen(fileName) > 4) && !strcasecmp(fileName + strlen(fileName) - 4, ".mp3");
}

boolean isMP4File(const char* fileName) {
  return ((strlen(fileName) > 4) && (
            !strcasecmp(fileName + strlen(fileName) - 4, ".mp4") ||
            !strcasecmp(fileName + strlen(fileName) - 4, ".m4a") ||
            !strcasecmp(fileName + strlen(fileName) - 4, ".3gp") ||
            !strcasecmp(fileName + strlen(fileName) - 4, ".3g2") ||
            !strcasecmp(fileName + strlen(fileName) - 4, ".aac"))
          );
}

boolean isOGGFile(const char* fileName) {
  return (strlen(fileName) > 5) && !strcasecmp(fileName + strlen(fileName) - 5, ".ogg");
}

boolean isFlacFile(const char* fileName) {
  return (strlen(fileName) > 5) && !strcasecmp(fileName + strlen(fileName) - 5, ".flac");
}

boolean isWMAFile(const char* fileName) {
  return (strlen(fileName) > 4) && !strcasecmp(fileName + strlen(fileName) - 5, ".wma");
}

boolean isPlaylistFile(const char* fileName) {
  return (strlen(fileName) > 4) && !strcasecmp(fileName + strlen(fileName) - 4, ".m3u");
}

bool read32(lv_fs_file_t * file, uint32_t& n) {
  uint32_t bytesread;
  if (fs_err(lv_fs_read(file, &n, 4, &bytesread), "MP4 Read 32")) return false;
  // Comment this out if your machine is big-endian.
  n = n >> 24 | n << 24 | (n >> 8 & 0xff00) | (n << 8 & 0xff0000);
  return true;
}

//------------------------
// MP4 container

//Must free returned string pointers
char * mp4_extractData(lv_fs_file_t * mp4, uint32_t blockSize) {
  uint32_t len;
  uint32_t current;
  uint32_t bytesread;
  char tag[5];
  char * str = NULL;
  if (fs_err(lv_fs_tell(mp4, &current), "MP4 Find home")) return NULL;
  read32(mp4, len);
  if (len <= blockSize) {
    if (fs_err(lv_fs_read(mp4, &tag, 4, &bytesread), "MP4 Read Type")) return NULL;
    tag[bytesread] = 0;
    if (strcmp(tag, "data") == 0) {
      if (fs_err(lv_fs_seek(mp4, 8, LV_FS_SEEK_CUR), "MP4 Seek 1")) return NULL;
      str = (char*)malloc(len - 14);
      if (fs_err(lv_fs_read(mp4, str, len - 15, &bytesread), "MP4 Read Type")) {
        free(str);
        return NULL;    
      }
      str[bytesread] = '\0';
    }
  }
  if(fs_err(lv_fs_seek(mp4, current, LV_FS_SEEK_SET), "MP4 Seek Home")) {
    free(str);
    return NULL; // Put you things away like you found 'em.
  }
  return str;
}

uint32_t mp4_metaJumper(lv_fs_file_t * mp4, uint32_t blockSize) {
  uint32_t current, bytesread;
  char tag[9];
  if (fs_err(lv_fs_tell(mp4, &current), "MP4 Find home")) return 0;
  if (fs_err(lv_fs_read(mp4, tag, 8, &bytesread), "MP4 Read Tag")) return 0;
  if (fs_err(lv_fs_seek(mp4, current, LV_FS_SEEK_SET), "MP4 Seek Home")) return 0;
  //Check MP4 header
  tag[bytesread] = '\0';
  if (bytesread != 8 || strcmp(&tag[4], "ftyp") != 0) {
    serial.println("> MP4 Metadata not found!");
    return 0;
  }
  return mp4_metaRecurse(mp4, blockSize);
}

uint32_t mp4_metaRecurse(lv_fs_file_t * mp4, uint32_t blockSize) {
  uint32_t current, bytesread;
  uint32_t blockPtr = 0;
  uint32_t dataStart = 0;
  char * str;
  if (fs_err(lv_fs_tell(mp4, &current), "MP4 Find home")) return 0;
  for (uint32_t size; read32(mp4, size); ) {
    char type[5];
    if (fs_err(lv_fs_read(mp4, &type, 4, &bytesread), "MP4 Read Type")) return 0;
    type[bytesread] = 0;
    if (strcasecmp(type, "moov") == 0) { mp4_metaRecurse(mp4, size); break; }
    if (strcasecmp(type, "udta") == 0) { mp4_metaRecurse(mp4, size); break; }
    if (strcasecmp(type, "meta") == 0) { mp4_metaRecurse(mp4, size); break; }
    if (strcasecmp(type, "ilst") == 0) { mp4_metaRecurse(mp4, size); break; }
    if (strcasecmp(type, "mdat") == 0) { dataStart = blockPtr; break; }
    if (strcasecmp(type, "\xA9" "alb") == 0) {
      if ((str = mp4_extractData(mp4, size))) {
        metaAlbum = str;
        //Serial.print("Album: ");
        //Serial.println(str);
        free(str);
      }
    }
    if (strcasecmp(type, "\xA9" "art") == 0) {
      if ((str = mp4_extractData(mp4, size))) {
        metaArtist = str;
        //Serial.print("Artist: ");
        //Serial.println(str);
        free(str);
      }
    }
    if (strcasecmp(type, "\xA9" "nam") == 0) {
      if ((str = mp4_extractData(mp4, size))) {
        metaTitle = str;
        //Serial.print("Title: ");
        //Serial.println(str);
        free(str);
      }
    }
    if (strcasecmp(type, "\xA9" "day") == 0) {
      if ((str = mp4_extractData(mp4, size))) {
        metaYear = str;
        //Serial.print("Year: ");
        //Serial.println(str);
        free(str);
      }
    }
    blockPtr += 8;
    if (strlen(type) != 0) {
      if (fs_err(lv_fs_seek(mp4, size - 8, LV_FS_SEEK_CUR), "MP4 Seek 1")) return 0;
      blockPtr += size - 8;
    } else {
      if (fs_err(lv_fs_seek(mp4, -4, LV_FS_SEEK_CUR), "MP4 Seek Back")) return 0;
      blockPtr -= 4;
    }
    if (blockPtr >= blockSize+current) break;
  }
  if(fs_err(lv_fs_seek(mp4, current, LV_FS_SEEK_SET), "MP4 Seek Home")) return 0; // Put you things away like you found 'em.
  return dataStart;
}

void utf16convert(char * buffer) {
  char * in, * out = in = buffer;
  uint16_t val;
  do {
    val = *in++;
    if (val > 0) val += *in++ << 8;
    if (val < 128) *out++ = (char)(val);
  } while (val != 0);
}


//------------------------
// MP3 ID3

uint32_t mp3_ID3Jumper(lv_fs_file_t * mp3) {
  char tag[5];
  char buffer[128];
  uint32_t headerSize = 0;
  uint32_t current;
  uint32_t bytesread;
  if (!mp3) return 0;
  if (fs_err(lv_fs_tell(mp3, &current), "MP3 Find Home")) return 0;
  if (fs_err(lv_fs_seek(mp3, 0, LV_FS_SEEK_SET), "MP3 Seek 1")) return 0;
  if (fs_err(lv_fs_read(mp3, tag, 3, &bytesread), "MP3 Read Tag")) return 0;
  tag[3] = '\0';
  if (strcmp(tag, "ID3") != 0) {
    serial.println("> ID3 Metadata not found!");
    return 0;
  }
  if (fs_err(lv_fs_seek(mp3, 6, LV_FS_SEEK_SET), "MP3 Seek 2")) return 0;
  if (fs_err(lv_fs_read(mp3, &buffer, 4, &bytesread), "MP3 Read Length")) return 0;
  headerSize = (buffer[0] << 21) + (buffer[1] << 14) + (buffer[2] << 7) + buffer[3];
//  Serial.print("MP3, header size: ");
//  Serial.println(headerSize);
  uint32_t pos = 10;
  uint8_t found = 0;
  while (pos < headerSize && found < 4) {
    uint32_t taglen = 0;
    if(fs_err(lv_fs_read(mp3, &tag, 4, &bytesread), "MP3 Read Tag")) return 0;
    tag[4] = '\0';
//    Serial.print("MP3 found tag: ");
//    Serial.println(tag);
    pos += bytesread;
    if(fs_err(lv_fs_read(mp3, &buffer, 4, &bytesread), "MP3 Read Tag Length")) return 0;
    taglen = (65536 * (buffer[0] * 256 + buffer[1])) + (buffer[2] * 256 + buffer[3]);
//    Serial.print("MP3 tag length: ");
//    Serial.println(taglen);
    pos += bytesread;
    uint8_t enc;
    if(fs_err(lv_fs_read(mp3, &buffer, 2, &bytesread), "MP3 Skip Padding")) return 0;
    if(fs_err(lv_fs_read(mp3, &enc, 1, &bytesread), "MP3 Read Encoding")) return 0;
    pos += bytesread;
    //stripHtml(buffer);
    if (strcmp(tag, "TIT2") == 0) {
      if(fs_err(lv_fs_read(mp3, &buffer, taglen - 1, &bytesread), "MP3 Read Title Data")) return 0;
      buffer[taglen - 1] = '\0';
      if (enc == 1 || enc == 2) utf16convert(buffer);
      metaTitle = buffer;
      //serial.print("Title: ");
      //serial.println(buffer);
      found++;
    } else if (strcmp(tag, "TPE1") == 0 || strcmp(tag, "TPE2") == 0) {
      if(fs_err(lv_fs_read(mp3, &buffer, taglen - 1, &bytesread), "MP3 Read Artist Data")) return 0;
      buffer[taglen - 1] = '\0';
      if (enc == 1 || enc == 2) utf16convert(buffer);
      metaArtist = buffer;
      //serial.print("Artist: ");
      //serial.println(buffer);
      found++;
    } else if (strcmp(tag, "TALB") == 0) {
      if(fs_err(lv_fs_read(mp3, &buffer, taglen - 1, &bytesread), "MP3 Read Album Data")) return 0;
      buffer[taglen - 1] = '\0';
      if (enc == 1 || enc == 2) utf16convert(buffer);
      metaAlbum = buffer;
      //serial.print("Album: ");
      //serial.println(buffer);
      found++;
    } else if (strcmp(tag, "TYER") == 0) {
      if(fs_err(lv_fs_read(mp3, &buffer, taglen - 1, &bytesread), "MP3 Read Year Data")) return 0;
      buffer[taglen - 1] = '\0';
      if (enc == 1 || enc == 2) utf16convert(buffer);
      metaYear = buffer;
      //serial.print("Year: ");
      //serial.println(buffer);
      found++;
    } else if (fs_err(lv_fs_seek(mp3, taglen - 1, LV_FS_SEEK_CUR), "MP3 Skip Tag")) return 0;
    pos += taglen - 1;
  }
  if(fs_err(lv_fs_seek(mp3, current, LV_FS_SEEK_SET), "MP3 Seek Home")) return 0; // Put you things away like you found 'em.
  return headerSize;
}

uint32_t flac_metaJumper(lv_fs_file_t * mp3) {
  char tag[5];
  char buffer[256];
  uint32_t blockSize = 0;
  uint32_t current;
  uint32_t bytesread;
  uint8_t type;
  bool last = false;
  if (!mp3) return 0;
  if (fs_err(lv_fs_tell(mp3, &current), "FLAC Find Home")) return 0;
  if (fs_err(lv_fs_seek(mp3, 0, LV_FS_SEEK_SET), "FLAC Seek 1")) return 0;
  if (fs_err(lv_fs_read(mp3, tag, 4, &bytesread), "FLAC Read Tag")) return 0;
  tag[4] = '\0';
  if (strcmp(tag, "fLaC") != 0) {
    serial.println("> fLaC header not found!");
    return 0;
  }
  uint32_t pos = 4;
  while (!last) {  
    if (fs_err(lv_fs_read(mp3, &buffer, 4, &bytesread), "FLAC Read Block Header")) return 0;
    last = (buffer[0] & 0x80) > 0;
    type = buffer[0] & 0x7F;
    blockSize = (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
    if (type == 4) { //vorbis comment
      if (fs_err(lv_fs_read(mp3, buffer, 4, &bytesread), "FLAC Read Vendor Length")) return 0;
      uint32_t len = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
      if (len > 0 && fs_err(lv_fs_seek(mp3, len, LV_FS_SEEK_CUR), "FLAC Skip Vendor")) return 0;
      if (fs_err(lv_fs_read(mp3, buffer, 4, &bytesread), "FLAC Read List Length")) return 0;
      uint32_t listlen = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
      while (listlen--) {
        if (fs_err(lv_fs_read(mp3, buffer, 4, &bytesread), "FLAC Read Comment Length")) return 0;
        len = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
        if (fs_err(lv_fs_read(mp3, buffer, len, &bytesread), "FLAC Read Comment")) return 0;
        buffer[bytesread] = '\0';
        if (strncasecmp(buffer, "TITLE=", 6) == 0) {
          metaTitle = &buffer[6];
          //serial.print("Title: ");
          //serial.println(metaTitle);
        } else if (strncasecmp(buffer, "ALBUM=", 6) == 0) {
          metaAlbum = &buffer[6];
          //serial.print("Album: ");
          //serial.println(metaAlbum);
        } else if (strncasecmp(buffer, "ARTIST=", 7) == 0) {
          metaArtist = &buffer[7];
          //serial.print("Artist: ");
          //serial.println(metaArtist);
        } else if (strncasecmp(buffer, "DATE=", 5) == 0) {
          metaYear = &buffer[5];
          if (metaYear.length() > 4) 
            metaYear = metaYear.substring(0, 4);
          //serial.print("Year: ");
          //serial.println(metaYear);
        }
      }
    } else if (fs_err(lv_fs_seek(mp3, blockSize, LV_FS_SEEK_CUR), "FLAC Skip Tag")) return 0;
    pos += blockSize + 4;
  } 
  if(fs_err(lv_fs_seek(mp3, current, LV_FS_SEEK_SET), "MP3 Seek Home")) return 0; // Put you things away like you found 'em.
  return pos;
}
