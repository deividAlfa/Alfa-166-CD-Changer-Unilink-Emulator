/*
 * unilink_log.c
 *
 *  Created on: 15 dic. 2024
 *      Author: David
 */

#include "unilink_log.h"
#include "serial.h"


#ifdef UNILINK_LOG_ENABLE


void unilinkLogUpdate(unilink_SPImode_t mode){
  uint8_t *s;

  if(mode==mode_rx){
    unilink.logRx=1;
    s=(uint8_t*)unilink.rxData;
    unilink.logSize=unilink.rxSize;
  }
  else{
    unilink.logTx=1;
    s=(uint8_t*)unilink.txData;
    unilink.logSize=unilink.txSize;
  }

  for(uint8_t i=0;i<unilink.logSize;i++){
    unilink.logData[i]= *s++;
  }
}
void unlinkLogTimestamp(void){
  uint32_t now=HAL_GetTick(), h, m, s, ms;
  ms = now%1000;
  now /= 1000;

  h=now/3600;
  now -= h*3600UL;

  m=now/60;
  now -= m*60;

  s=now;

  iprintf("%02lu:%02lu:%02lu.%03lu    ", h, m, s, ms);
  fflush(stdout);
}

void unilinkLog(void){
  static bool b_str;
  uint8_t size,count=0,i=0;
  char str[128];
  size=unilink.logSize;
  if(unilink.logTx){
    str[i++]='#';
    str[i++]=' ';
    unilink.logTx=0;
  }
  else if(unilink.logRx){
#ifndef UNILINK_PASSIVE_MODE
    str[i++]='<';
    str[i++]=' ';
#endif
    unilink.logRx=0;
  }
  else{
#ifdef UNILINK_PASSIVE_MODE                                  // Detect slave breaks in PASSIVE_MODE mode
    static breakState_t state;
    switch(state){
      case break_wait_data_low:
      {
        if(isDataLow()){
          slaveBreak.dataTime=8;                  // Wait for DATA line 8mS in low state
          state++;
        }
        break;
      }
      case break_wait_data_low_time:
      {
        if(slaveBreak.dataTime){
          if(isDataHigh()){
            state=break_wait_data_low;                      // If DATA goes high before the timer expires, reset state
          }
        }
        else{
          state++;
        }
        break;
      }
      case break_wait_data_high:
      {
        if(isDataHigh()){                      // Wait for DATA high for 3ms
          slaveBreak.dataTime=3;
          state++;
        }
        break;
      }
      case break_wait_data_high_time:
      {
        if(slaveBreak.dataTime){                  // Wait for DATA line 3mS in high state
          if(isDataLow()){
            state=break_wait_data_low;                      // If DATA goes low before the timer expires, reset to state 0
          }
        }
        else{
          slaveBreak.dataTime=3;                    // Load timer with 3ms
          state++;
        }
        break;
      }
      case break_wait_data_setlow:
      {
        if(isDataLow()){                            // If data goes low within time
          slaveBreak.dataTime=2;                    // Load timer with 2ms
          state=break_wait_data_keeplow;                 // Break started
        }
        else if(!slaveBreak.dataTime){    // Timer expired,slave didn't toggle data low
          state=break_wait_data_low;                      // Reset status
          putString("S? ");
          b_str=1;
        }
        break;
      }
      case break_wait_data_keeplow:
      {
        if(!slaveBreak.dataTime){              // Timer expired, data low end, break executed
          state=break_wait_data_low;                      // Reset status
          if(b_str){
            putString("\r\n");
            b_str=0;
          }
#ifdef UNILINK_LOG_TIMESTAMP
          unlinkLogTimestamp();
#endif
          putString("BREAK\r\n");
        }
        else if(isDataHigh()){
          state=break_wait_data_low;                      // If DATA goes high before the timer expires, invalid break
          putString("S! ");
          b_str=1;
        }
      }
      default:
        break;
    }
#endif
    return;
  }

  if(b_str){
    putString("\r\n");
    b_str=0;
  }
#ifdef UNILINK_LOG_TIMESTAMP
  unlinkLogTimestamp();
#endif

  //  Unilink frame structure
  //  char data[]= "00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00";
  //                0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
  //                rad tad m1  m2  c1  d1  d2  d3  d4  d5  d6  d7  d8  d9  c2
  //                [             ] [c] [                                ] [c]  [0]   // 13 byte cmd (16 byte transfer)
  //                [             ] [c] [            ] [c] [0]                        // 8 byte cmd  (11 byte transfer)
  //                [             ] [c] [0]                                           // 4 byte cmd  (6 byte transfer)

  // Print frame
  while(count<(size-1)){
  #ifdef UNILINK_LOG_DETAILED
    if(count==0){
      str[i++]='[';
    }
#endif
    str[i++]=hex2ascii((unilink.logData[count])>>4);
    str[i++]=hex2ascii((unilink.logData[count])&0x0f);
    count++;
#ifdef UNILINK_LOG_DETAILED
    if( count<parity1 || (count>(unilink_short-1) && (count<(size-2)))){
#else
    if(count<size){
#endif
        str[i++]=' ';
    }
    #ifdef UNILINK_LOG_DETAILED
    if(    (count==parity1) || (size>unilink_short && count==d1) ||         // Print fields between brackets [x]
        (size==unilink_medium && count==parity2) ||
        (size==unilink_long && count==parity2_L) ){
      str[i++]=']';
      str[i++]='[';
    }

    if(count==(size-1)){
      str[i++]=']';
    }
#endif
  }
#ifndef UNILINK_LOG_DETAILED
  str[i++]='\r';
  str[i++]='\n';
#endif

  str[i]=0;
  putString(str);

  // Print info
#ifdef UNILINK_LOG_DETAILED                                   // Add separator for the comments

    /*
  if(size<unilink_medium){
    putString("                                    ");      // For Wifi terminal app
  }
  else if(size<unilink_long){
    putString("      ");
  }
  else{
    putString("    ");
  }
*/                                          // For normal use
  if(size<unilink_medium){
    putString("                 ");
  }
  if(size<unilink_long){
    putString("                   ");
  }
  else{
    putString("    ");
  }
  switch(unilink.logData[cmd1]){
    case cmd_status:
      putString("STATUS: ");

      switch(unilink.logData[cmd2]){
        case 0x00:
          putString("Playing\r\n");
          break;

        case 0x20:
          putString("Changed CD\r\n");
          break;

        case 0x21:
          putString("Seeking\r\n");
          break;

        case 0x40:
          putString("Changing CD\r\n");
          break;

        case 0x80:
          putString("idle\r\n");
          break;

        case 0xC0:
          putString("Ejecting\r\n");
          break;

        case 0xF0:
          putString("2nd level seek pressed\r\n");
          break;

        case 0xFA:
          putString("2nd level seek released\r\n");
          break;

        default:
          putString("???\r\n");
        }
      break;

    case cmd_busRequest:
      putString("MASTER REQUEST: ");
      switch(unilink.logData[cmd2]){
      case cmd_busReset:
        putString("Bus reset\r\n");
        break;

      case cmd_anyone:
        putString("Anyone?\r\n");
        break;

      case cmd_anyoneSpecial:
        putString("Anyone special?\r\n");
        break;

      case cmd_appointEnd:
        putString("Appoint end\r\n");
        break;

      case cmd_timePollEnd:
        putString("Time poll end\r\n");
        break;

      case cmd_timePoll:
        putString("Time poll\r\n");
        break;

      case cmd_slavePoll:
        putString("Slave poll\r\n");
        break;

      default:
        putString("???\r\n");
      }
      break;

    case cmd_cfgfrommaster:
      putString("CONFIG FROM MASTER: ");
      switch(unilink.logData[cmd2]){
        case 0x01:
          putString("Tell slave about assigned ID\r\n");
          break;

        default:
          putString("???\r\n");
      }
      break;

    case cmd_keyoff:
      putString("KEY OFF: ");
      switch(unilink.logData[cmd2]){
        case 0x00:
          putString("Released\r\n");
          break;

        default:
          putString("???\r\n");
      }
      break;

    case cmd_play:
      putString("Play\r\n");
      break;

    case cmd_fastFwd:
      putString("Fast Forward\r\n");
      break;

    case cmd_fastRwd:
      putString("Fast Rewind\r\n");
      break;

    case cmd_repeat:
      putString("Toggle Repeat:");
      putString(unilink.logData[cmd2]&0x10 ? "ON":"OFF");
      putString("\r\n");
      break;

    case cmd_shuffle:
      putString("Togggle Shuffle:");
      putString(unilink.logData[cmd2]&0x10 ? "ON":"OFF");
      putString("\r\n");
      break;

    case cmd_intro:
      putString("Toggle Intro:");
      putString(unilink.logData[cmd2]&0x10 ? "ON":"OFF");
      putString("\r\n");
      break;

    case cmd_intro_end:
      putString("Intro end\r\n");
      break;

    case cmd_cfgchange:
    {
      putString("CFG CHANGE -");
      if(unilink.logData[d1] & 0x80){
        putString(" Mute:");
        putString(unilink.logData[d1] & 0x40 ? "ON" : "OFF");
      }
      if(unilink.logData[d1] & 0x20){
        putString(" Loudness:");
        putString(unilink.logData[d1] & 0x10 ? "ON" : "OFF");
      }
      if(unilink.logData[d2] & 0x80){
        putString(" Phone mute:");
        putString(unilink.logData[d2] & 0x40 ? "ON" : "OFF");
      }
      putString("\r\n");
      break;
    }
    case cmd_switch:
      putString("Switch to ");
      putString(unilink.logData[cmd2]==0x10 ? "CD?" : "???");
      putString("\r\n");
      break;

    case cmd_textRequest:
      putString("REQUEST FIELD: ");
      switch(unilink.logData[cmd2]){
        case 0x95:
          putString("MAGAZINE INFO\r\n");
          break;

        case 0x97:
          putString("DISC INFO\r\n");
          break;

        default:
          putString("???\r\n");
      }
      break;

    case cmd_power:
      putString("POWER: ");
      switch(unilink.logData[cmd2]){
        case cmd_pwroff:
          putString("Go Idle\r\n");
          break;

        case cmd_pwron:
          putString("Wake from Idle\r\n");
          break;

        default:
          putString("???\r\n");
      }
      break;

    case 0x88:
      putString("From master to group ID after Anyone special\r\n");
      break;

    case cmd_anyoneResp:
      putString("Send slave ID\r\n");
      break;

    case cmd_cartridgeinfo:
      putString("CARTRIDGE INFO: ");
      switch(unilink.logData[cmd2]){
        case 0x40:
        {
          char s[4] = { '0'+(unilink.logData[d4]>>4), '\r', '\n', 0 };
          putString("Slot empty for disc ");
          putString(s);
          break;
        }
        case 0x80:
          putString("Magazine inserted\r\n");
          break;

        case 0xC0:
          putString("No magazine\r\n");
          break;

        default:
          putString("???\r\n");
      }
      break;

    case cmd_time:
    {
      char s[5] = {0};

      putString("TIME: ");
      if(unilink.logData[cmd2]!=0)
        putString("??? ");

      putString("Disc:");
      s[0]='0'+(unilink.logData[d4]>>4);
      putString(s);
      putString(" Track:");
      s[0]=(unilink.logData[d1]>>4) == 0xF ? '0' : '0'+(unilink.logData[d1]>>4);
      s[1]='0'+(unilink.logData[d1]&0xF);
      s[2]=' ';
      putString(s);

      s[0]=(unilink.logData[d2]>>4) == 0xF ? '0' : '0'+(unilink.logData[d2]>>4);
      s[1]='0'+(unilink.logData[d2]&0xF);
      s[2]=':';
      putString(s);

      s[0]='0'+(unilink.logData[d3]>>4);
      s[1]='0'+(unilink.logData[d3]&0xF);
      s[2]='\r';
      s[3]='\n';
      putString(s);
      break;
    }
    case cmd_magazine:
      putString("MAGAZINE INFO: ");
      putString("CD1:");
      putString(unilink.logData[cmd2]&0x10 ? "Y":"N");
      putString(" CD2:");
      putString(unilink.logData[cmd2]&0x20 ? "Y":"N");
      putString(" CD3:");
      putString(unilink.logData[cmd2]&0x40 ? "Y":"N");
      putString(" CD4:");
      putString(unilink.logData[cmd2]&0x80 ? "Y":"N");
      putString(" CD5:");
      putString(unilink.logData[cmd2]&0x01 ? "Y":"N");
      putString(" CD6:");
      putString(unilink.logData[cmd2]&0x02 ? "Y":"N");
      putString("\r\n");
      break;

    case cmd_discinfo:
    {
      char s[6] = {0};
      putString("DISC INFO: ");
      putString("Disc:");
      s[0]='0'+(unilink.logData[d4]>>4);
      s[1]=' ';
      putString(s);

      putString("Tracks:");
      s[0]='0'+(unilink.logData[d1]>>4);
      s[1]='0'+(unilink.logData[d1]&0xF);
      s[2]=' ';
      putString(s);

      s[0]='0'+(unilink.logData[d2]>>4);
      s[1]='0'+(unilink.logData[d2]&0xF);
      s[2]='m';
      s[3]=':';
      putString(s);

      s[0]='0'+(unilink.logData[d3]>>4);
      s[1]='0'+(unilink.logData[d3]&0xF);
      s[2]='s';
      s[3]='\r';
      s[4]='\n';
      putString(s);
      break;
    }
    case cmd_dspdiscchange:
    {
      char s[4] = { '0'+(unilink.logData[d4]>>4), '\r', '\n', 0 };
      putString("To master after changing to disc ");
      putString(s);
      break;
    }
    case cmd_goto:
    {
      char s[5] = {0};
      putString("CHANGE TO: ");

      putString("Disc:");
      s[0]='0'+(unilink.logData[cmd2]&0xF);
      s[1]=' ';
      putString(s);

      putString("Track:");
      s[0]='0'+(unilink.logData[d1]>>4);
      s[1]='0'+(unilink.logData[d1]&0xF);
      s[2]='\r';
      s[2]='\n';
      putString(s);
      break;
    }
    case cmd_source:
    {
      char s[4] = {0};
      putString("Switch source from ");
      s[0]='0'+(unilink.logData[d1]>>4);
      s[1]='0'+(unilink.logData[d1]&0xF);
      putString(s);
      putString(" to ");
      s[0]='0'+(unilink.logData[cmd2]>>4);
      s[1]='0'+(unilink.logData[cmd2]&0xF);
      putString(s);
      putString("\r\n");
      break;
    }
    default:
      putString("???\r\n");
  }
#endif
  // Finish
}
#endif

char hex2ascii(uint8_t hex){
  hex&=0x0f;
  if(hex<10){
    return('0'+hex);
  }
  else{
    return('7'+hex);
  }
  return ' ';
}

uint8_t bcd2hex(uint8_t bcd){
  uint8_t tmp;
  tmp=((bcd&0xF0)>>4)*10;
  tmp+=(bcd&0x0F);
  return tmp;
}

uint8_t hex2bcd(uint8_t hex){
  uint8_t tmp;
  tmp=(hex/10)<<4;
  tmp+=(hex%10);
  return tmp;
}
