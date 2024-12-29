/*
 * unilink.c
 *
 *  Created on: Dec 22, 2020
 *      Author: David
 *
 *  Credits:
 *
 *  Michael Wolf
 *  https://www.mictronics.de/ (Original AVR Becker Unilink page down)
 *
 *  Sander Huiksen
 *  https://sourceforge.net/projects/alfa166unilink-git/
 *
 */

#include "unilink.h"
#include "unilink_log.h"
#include "i2sAudio.h"
#include "serial.h"
#include "bt.h"

/****************************************************************
 *             VARIABLES                *
 ****************************************************************/
unilink_t       unilink;
slaveBreak_t     slaveBreak;
magazine_t       mag_data;
cdinfo_t         cd_data[_DISCS_];
const uint8_t mag_cd[_DISCS_] = { mag_cd1, mag_cd2, mag_cd3, mag_cd4, mag_cd5, mag_cd6 };


void unilinkColdReset(void);
void unilinkWarmReset(void);
void unilink_handle_led(void);
void unilink_parse(void);
bool unilink_checksum(void);
void unilink_broadcast(void);
void unilink_myid_cmd(void);
void unilink_appoint(void);
void unilink_repeat(bool isOn);
void unilink_shuffle(bool isOn);
uint8_t unilink_auto_status(void);
void unilink_update_status(void);
void unilink_set_status(uint8_t status);
void unilink_add_slave_break(uint8_t command);
void unilink_clear_slave_break_queue(void);
void unilink_handle_slave_break(void);
void unilink_slave_msg(void);
void unilink_tx(uint8_t *msg, volatile uint8_t *dest);
void unilink_data_mode(unilink_DATAmode_t mode);
void unilink_spi_mode(unilink_SPImode_t mode);



void unilink_init(SPI_HandleTypeDef* SPI, TIM_HandleTypeDef* tim){

  unilink.timer    = tim;
  unilink.SPI      = SPI;

  mag_data.status=mag_inserted;
  mag_data.cmd2=0;                        // Clear CDs
  mag_data.CD1_present=1;                 // Set CD1 by default
  cd_data[0].tracks = 88;
  cd_data[0].mins=55;
  cd_data[0].secs=55;
  cd_data[0].inserted=1;
#ifdef BT_SUPPORT
  unilink.track = 50;
#else
  unilink.track = 1;
#endif
  unilink.lastDisc=0;
  unilink.disc = 1;
  //unilink.mag_changed=1;                    // XXX: Required?

  setDataLow();                             // Force DATA Output low (Doesn't affect when GPIO is in SPI mode)

  unilinkColdReset();
  __HAL_TIM_SET_AUTORELOAD(unilink.timer, _BYTE_TIMEOUT_);
  HAL_TIM_Base_Start_IT(unilink.timer);                           // Start timer in IT mode
  unilink.hwinit=1;                          // Set init flag
}

void unilink_handle(void){
  if(unilink.system_off==0 && unilink.timeout>_PWROFF_TIMEOUT_){                      // 10 second without any activity
    putString("No activity timeout, shutting down...\r\n");
#ifdef USB_LOG
    flush_log();                                  // flush log
    removeDrive();
#endif
    setPinLow(PWR_ON_GPIO_Port, PWR_ON_Pin);      // Turn off
    unilink.system_off=1;
  }
  #ifdef UNILINK_LOG_ENABLE
  unilinkLog();
  #endif
  if(unilink.received){
    unilink.received=0;                               // do a parity check of received packet and proceed if OK
    if(unilink_checksum()){                           // If checksum ok...
#ifndef PASSIVE_MODE
      unilink_parse();
#endif
    }
    else{
      unilink.bad_checksum=1;                           // Bad checksum, probably skipped a clock, resync by ignoring further data until master stops sending clocks and triggers a byte timeout.
      putString("BAD CHECKSUM\r\n");
    }
  }
  if(unilink.mag_changed && unilink.masterinit){      // usb was inserted, removed or contents changed
    uint8_t eject=0, insert=0;
    unilink.mag_changed=0;
    unilink.track=1;
    mag_data.cmd2=0;                              // Clear cds in magazine
    unilink.lastDisc=0;
    unilink.disc=0;                               // First cd is 1. Set to 0 to detect if the following loop fails
    for(uint8_t i=0;i<_DISCS_;i++){               // Scan cd data
      if(cd_data[i].inserted){                    // Fidn valid cds
        if(unilink.disc==0){                      // Assign first valid cd
          unilink.disc=i+1;
          unilink.track=1;
          unilink.lastTrack=1;
        }
        mag_data.cmd2 |= mag_cd[i];               // Add cd to magazine
      }
    }
    if(unilink.disc!=0 && mag_data.status==mag_inserted ){        // Found cds, magazine was inserted, eject and insert
      eject=1;
      insert=1;
    }
    else if(unilink.disc!=0 && mag_data.status==mag_removed ){    // Found cds, magazine was removed, insert
      insert=1;
    }
    else if(unilink.disc==0 && mag_data.status==mag_inserted ){   // No cd found, magazine was inserted, eject
      eject=1;
    }
    else if(unilink.disc==0 && mag_data.status==mag_removed ){    // No cd found, magazine was removed, nothing to do
    }

    if(eject){
      if(unilink.disc==0){
        unilink.disc=1;
      }
      unilink_set_status(unilink_ejecting);
      unilink_add_slave_break(cmd_status);
      mag_data.status=mag_removed;
      unilink_add_slave_break(cmd_cartridgeinfo);
      unilink_set_status(unilink_idle);
    }
    if(insert){
      mag_data.status=mag_inserted;
      unilink_add_slave_break(cmd_cartridgeinfo);
      unilink_set_status(unilink_changing);
    }
#ifdef AUDIO_SUPPORT
    if(systemStatus.driveStatus==drive_ready && (systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause))          // Audio playing or paused
        AudioStop();                                                                            // Stop audio
#endif
    unilink_reset_playback_time();
    unilink.play=0;                                 // Don't go into play mode automatically
  }
#ifdef AUDIO_SUPPORT
  if(systemStatus.driveStatus==drive_ready){
    if(unilink.trackChanged){                                                                   // Track changed
      if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause) {       // Audio playing or paused
        AudioStop();                                                                            // Stop audio
      }
      AudioStart();                                                                             // Start audio
      unilink_reset_playback_time();
      unilink.trackChanged = 0;                                                                 // Clear flag
    }
    else{
      if(unilink.status==unilink_playing){
        if(systemStatus.audioStatus!=audio_play){     // Unilink playing, audio paused or stopped
          AudioStart();
        }
      }
      else{
        if(systemStatus.audioStatus==audio_play){     // Unilink stopped, audio playing
          AudioPause();
        }
      }
    }
  }
#endif

#ifndef PASSIVE_MODE
  unilink_handle_slave_break();                     // Handle slave break events
#endif

  unilink_handle_led();                             // Handle activity led
#ifdef BT_SUPPORT
  if(unilink.sec>1){                                // For BT, always resume track 50 after changing
    unilink.track=50;
  }
#endif
}

void unilink_update_magazine(void){
  unilink.mag_changed=1;
}

void unilink_handle_led(void){                      // ACTIVITY LED
  static uint32_t time=0;
#if defined (PASSIVE_MODE) && defined (USB_LOG)     // Passive mode: LED for usb status
  if(HAL_GetTick()>time){
    time=HAL_GetTick();
    togglePin(LED_GPIO_Port,LED_Pin);
    if(systemStatus.driveStatus==drive_ready){
      time+=500;                                    // Drive mounted, slow blink
    }
    else{
      time+=50;                                     // Else, fast blinking
    }
  }
#elif !defined (PASSIVE_MODE)
  if(unilink.masterinit){                           // Device mode: Led for unilink status
    if(unilink.status==unilink_idle){
      setPinLow(LED_GPIO_Port,LED_Pin);             // Idle state and initialized, Led on
    }
    else if(unilink.status==unilink_playing){       // Play state, quick led blinking
      if(HAL_GetTick()-time>100){
        time=HAL_GetTick();
        togglePin(LED_GPIO_Port,LED_Pin);
      }
    }
  }
  else{
    if(!HAL_GPIO_ReadPin(LED_GPIO_Port,LED_Pin)){
      if(HAL_GetTick()-time>20){
        time=HAL_GetTick();                         // If not initialized, make small pulses
        setPinHigh(LED_GPIO_Port,LED_Pin);          // Indicating we're alive but not initialized
      }
    }
    else{
      if(HAL_GetTick()-time>1000){
        time=HAL_GetTick();
        setPinLow(LED_GPIO_Port,LED_Pin);
      }
    }
  }
#endif
}



//  Unilink frame structure
//  char data[]= "00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00";
//                0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
//                rad tad m1  m2  c1  d1  d2  d3  d4  d5  d6  d7  d8  d9  c2
//                [             ] [c] [                                ] [c]  [0]   // 16 byte frame
//                [             ] [c] [            ] [c] [0]                        // 11 byte frame
//                [             ] [c] [0]                                           // 6 byte frame

void unilink_create_msg(uint8_t *msg, volatile uint8_t *dest){
  uint8_t checksum = 0;                                         // local checksum
  uint8_t i = 0;                                                // local index
  uint8_t size;                                                 // msg size

  if(msg[2]>=0xC0){ size=unilink_long; }                        // 16 byte command
  else if(msg[2]>=0x80){ size=unilink_medium; }                 // 11 byte command
  else{ size=unilink_short; }                                   //  6 byte command

  while(i<parity1){
    checksum += msg[i];                                         // copy the first 4 bytes from msg to Tx buffer
    dest[i]=msg[i];                                             // and calculate checksum for it
    i++;
  }
                                                                // i: byte 4: checksum1
  dest[i++] = checksum;                                         // copy checksum1 to Tx buffer
                                                                // i: byte 5: D1
  while(i<(size-2)){                                            // Handle rest of msg
    dest[i] = msg[i-1];                                         // copy from msg to Tx data buffer
    checksum += msg[i-1];                                       // add each byte to checksum2
    i++;
  }

  dest[i++]=checksum;                                           // Store checksum2
  dest[i]=0;                                                    // End of msg always 0

  if(dest==unilink.txData){                                     // If dest is tx buffer
    unilink.txSize=size;                                        // Store Tx size
    unilink_spi_mode(mode_tx);
  }
  else{                                                         // Slave break buffer
    dest[parity2_L+2]=size;                                     // Store Tx size
  }
}

void unilink_parse(void){
  if(unilink.rxData[dst_addr]==addr_broadcast){
    unilink_broadcast();                                            // parse broadcast packets
  }
  else if(unilink.rxData[dst_addr]==unilink.ownAddr){
    unilink_myid_cmd();                                             // parse packets for my ID
  }
  else if((unilink.rxData[dst_addr]&0xF0) == unilink.groupID ){     // Appoint for us?
    unilink_appoint();                                              // do ID appoint procedure
  }
}

void unilink_broadcast(void){                           // BROADCAST COMMANDS
  switch(unilink.rxData[cmd1]){                         // Switch CMD1
    case cmd_busRequest:                                // 0x01 Bus requests (Broadcast)
    {
      switch(unilink.rxData[cmd2]){                     // Switch CMD2
        case cmd_busReset:                              // 0x01 0x00 Bus reset
          unilinkColdReset();
          unilink.busReset = 1;
          unilink.system_off=0;                         // Just in case we were shutting down due a timeout but we got a busReset in time
          setPinHigh(PWR_ON_GPIO_Port, PWR_ON_Pin);     // Turn on
          break;
        case cmd_anyone:                                // 0x01 0x02 Anyone?
          if(!unilink.masterinit && unilink.busReset){
            unilink.appoint = 1;                        // Enable appoint
            uint8_t msg[] = msg_anyoneResp;
            unilink_create_msg(msg,unilink.txData);     // send my device info string
          }
          else{
          }
          break;
        case cmd_appointEnd:
            break;
        default:
            break;
      }
      break;
    }
    case cmd_source:                                    // 0xF0 SRC Source select
      if (unilink.rxData[cmd2]!=unilink.ownAddr){       // check if interface is deselected
        unilink.status = unilink_idle;                  // set idle status on deselect
      }
      break;
    case cmd_power:                                     // 0x87 Power Event
      if(unilink.rxData[cmd2]==cmd_pwroff){             // 0x00 Power off
#ifdef BT_SUPPORT
        BT_Stop();
#endif
        unilink.play=0;
        unilink.powered_on=0;
        unilink_set_status(unilink_idle);               // set idle status on power off
#ifdef AUDIO_SUPPORT
        if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause) {       // Audio playing or paused
          AudioStop();                                                                            // Stop audio
        }
#endif
        unilink_reset_playback_time();
      }
      else if(unilink.rxData[cmd2]==cmd_pwron){         // 0x89 Power on
        unilink.play=0;
        unilink.powered_on=1;
        unilink_set_status(unilink_idle);
      }
      break;
  }
}

void unilink_myid_cmd(void){
  switch(unilink.rxData[cmd1]){                         // Switch CMD1
    case cmd_busRequest:                                // 0x01 Bus requests (for my ID)
    {
      switch(unilink.rxData[cmd2]){
        case cmd_timePoll:                              // 0x01 0x12 Respond to time poll (PONG)
        {
          uint8_t msg[] = msg_status;
          unilink_create_msg(msg,unilink.txData);
          unilink_update_status();
          break;
        }
        case cmd_slavePoll:                             // 0x01 0x13 permission to talk after poll request
        {
          unilink_slave_msg();                          // send slave response
          break;
        }
        default:
          break;
      }
      break;
    }
    case cmd_play:                                      // 0x20 PLAY
    {
      if(!unilink.powered_on || ++unilink.play_cmd_cnt<2){                        // Not powered on, or first request, ignore
        break;                                                                    // The ICS sometimes sends play cmd on poweron but it's no sense, it'll send the request again if it really meant to
      }
      unilink.play_cmd_cnt=0;
      if((mag_data.status!=mag_removed)&&(unilink.status!=unilink_ejecting)){     // If magazine is present and we are not ejecting      //FIXME: Ejecting check might be wrong?
        if(cd_data[unilink.disc-1].inserted){                                     // If current selected disc is valid
          if(unilink.track>cd_data[unilink.disc-1].tracks){                       // If current track is valid
            unilink.track=1;                                                      // Else, reset track
            unilink.lastTrack=1;
          }
          unilink.play=1;
#ifdef BT_SUPPORT
          BT_Play();
#endif
          unilink_set_status(unilink_playing);
          unilink_add_slave_break(cmd_status);
        }
        else{
          unilink.disc=0;
          for(uint8_t i=0;i<_DISCS_;i++){                     // Else, find the first valid disc
            if(cd_data[i].inserted){
              unilink.disc=i+1;
              unilink.lastDisc=i+1;
              unilink.track=1;
              unilink.lastTrack=1;
              unilink.play=1;
              unilink_set_status(unilink_playing);
              unilink_add_slave_break(cmd_status);
              break;
            }
          }
          if(unilink.disc==0){                          // No valid cd was found
            unilink_set_status(unilink_idle);
            unilink_add_slave_break(cmd_status);        // FIXME: Untested, probably wrong or lacking further actions
          }
        }
      }
      break;
    }
    case cmd_switch:                                    // 0x21 TA message start
    {
      if(unilink.rxData[cmd2] == 0x20){
        unilink_set_status(unilink_idle);
      }
      if(unilink.rxData[cmd2] == 0x10){
      }
      break;
    }

    case cmd_fastFwd:                                   // 0x24 Fast Forward
    {
      break;
    }
    case cmd_fastRwd:                                   // 0x25 Fast Reverse
    {
      break;
    }

    case cmd_repeat:                                    // 0x34 Repeat mode change
    {
      switch(unilink.rxData[cmd2]){
        case 0x00:
        {
          //repeat_mode(0);                             // FIXME: Repeat is not implemented
          break;
        }
        case 0x10:
        {
          //repeat_mode(1);
          break;
        }
      }
      unilink_add_slave_break(cmd_cfgchange);           // Add command to queue
      break;
    }
    case cmd_shuffle:                                   // 0x35 Shuffle mode change
    {
      switch(unilink.rxData[cmd2]){
        case 0x00:
        {
          //shuffle_mode(0);                            // FIXME: Shuffle is not implemented
          break;
        }
        case 0x10:
        {
          //shuffle_mode(1);
          break;
        }
      }
      unilink_add_slave_break(cmd_cfgchange);
      break;
    }
    case cmd_intro:                                     // 0x36 Intro mode change
    {
      switch(unilink.rxData[cmd2]){
        case 0x00:
        {
          //intro_mode(0);                            // FIXME: Intro is not implemented
          break;
        }
        case 0x10:
        {
          //intro_mode(1);
          break;
        }
      }
      unilink_add_slave_break(cmd_cfgchange);
      break;
    }
    case cmd_textRequest:                             // 0x84 request for command
    {
      switch(unilink.rxData[cmd2]){
        case cmd_magazine:                            // 0x95 request magazine info
        {
          uint8_t msg[] = msg_magazine;
          unilink_create_msg(msg, unilink.txData);
          break;
        }
        case cmd_discinfo:                            // 0x97 request disc total time and tracks
        {
          if((mag_data.status!=mag_removed)&&(unilink.status!=unilink_ejecting)){
            uint8_t msg[]=msg_discinfo;
            unilink_create_msg(msg,unilink.txData);
          }
          else{
            uint8_t msg[]=msg_discinfo_empty;
            unilink_create_msg(msg,unilink.txData);
          }
          break;
        }
        default:
          break;
      }
      break;
    }
    case cmd_goto:                                    // 0xB0 Direct Disc keys
    {
      unilink.millis=0;
      unilink.sec=0;
      unilink.min=0;
      uint8_t disc=unilink.rxData[cmd2]&0x0F;
      uint8_t track=bcd2hex(unilink.rxData[d1]);
#ifdef BT_SUPPORT
      if(track==unilink.track){
        BT_Prev();
      }
      else if(track<unilink.track){
        for(uint8_t i=0;i<(unilink.track-track);i++)
          BT_Prev();
      }
      else{
        for(uint8_t i=0;i<(track-unilink.track);i++)
          BT_Next();
      }
#endif

      unilink.lastTrack = unilink.track;
      unilink.track = track;
      unilink.trackChanged = 1;
      unilink_clear_slave_break_queue();

      if(unilink.disc != disc){                                   // Disc changed
        unilink.lastDisc = unilink.disc;
        unilink.disc = disc;
        if((mag_data.status!=mag_removed)&&(unilink.status!=unilink_ejecting)){
          if(!cd_data[unilink.disc-1].inserted){                  // If requested disc is not present
            mag_data.status=mag_slot_empty;                       // Set empty slot status
            unilink_add_slave_break(cmd_cartridgeinfo);           // Add empty slot msg
            mag_data.status=mag_inserted;                         // Restore cartridge inserted status
            if(cd_data[unilink.lastDisc-1].inserted){             // Return to previous disc if possible
              unilink.disc=unilink.lastDisc;
            }
            else{                                                 // Else, find first valid disc
              for(uint8_t i=0;i<_DISCS_;i++){
                if(cd_data[i].inserted){
                  unilink.disc = i+1;
                  break;
                }
              }
            }
            if(!cd_data[unilink.disc-1].inserted){                // No discs on system
              unilink.trackChanged = 0;                           // Abort track change
              unilink_set_status(unilink_idle);                   // Idle state
            }
            else{
              unilink.lastDisc=unilink.disc;                      // Update disk
            }
          }
          if(unilink.status != unilink_idle){                     // All good
            unilink_add_slave_break(cmd_discinfo);
            unilink_add_slave_break(cmd_cfgchange);
            unilink_add_slave_break(cmd_dspdiscchange);
            unilink_add_slave_break(cmd_intro_end);
            unilink_set_status(unilink_playing);
            unilink_add_slave_break(cmd_status);
          }
        }
        else{                                                     // ICS requested a disc while we where ejecting? Very unlikely but...
          for(uint8_t i=0;i<_DISCS_;i++){                         // Clear discs
            cd_data[unilink.disc-1].inserted=0;
          }
          mag_data.cmd2=0;
          unilink.status=unilink_ejecting;
          unilink_add_slave_break(cmd_status);
          mag_data.status=mag_removed;
          unilink_add_slave_break(cmd_cartridgeinfo);
          unilink_set_status(unilink_idle);
          unilink_add_slave_break(cmd_status);
          unilink.play=0;
        }
      }
      else{
        unilink_set_status(unilink_playing);                      // Track changed
      }
      break;
    }
    default:
      break;
  }
}

void unilink_appoint(void){                                     // respond to ID appoint
  if((unilink.rxData[cmd1]==0x02) && unilink.appoint){          // check for previous Anyone command
    if ((unilink.rxData[dst_addr]&0xF0)==unilink.groupID){      // check if packet is for my group
      if(!unilink.masterinit){                                  // I have no ID
        unilink.masterinit=1;
        unilink.appoint=0;                                      // Disable appoint
        unilink.ownAddr = unilink.rxData[dst_addr];             // save my new ID
        uint8_t msg[] = msg_anyoneResp;                         // Generate response
        unilink_create_msg(msg,unilink.txData);                 // send my device info string
      }
    }
  }
}

void unilink_update_status(void){
  /*
  if(unilink.statusTimer) return;           // FIXME: Enable if changing the status too fast cause issues
  unilink.statusTimer = 500;                // 500ms, may need tweaking
  */
  switch(unilink.status){
    case unilink_playing:
    case unilink_idle:
      break;
    case unilink_ejecting:
      unilink.status = unilink_idle;
      break;
    case unilink_changing:
      unilink.status = unilink_changed;
      break;
    case unilink_changed:
      if(unilink.play==1){
        unilink.status = unilink_seeking;
      }
      else{
        unilink.status = unilink_idle;
      }
      break;
    case unilink_seeking:
      unilink.status = unilink_playing;
      break;
    default:
      unilink.status = unilink_idle;
      break;
  }
}
void unilink_set_status(uint8_t status){
  unilink.status=status;
}

bool unilink_checksum(void){                  // Check parity of complete Unilink packet
    uint8_t i=0;                              // local byte counter
    uint8_t checksum=0;                       // local checksum

    for(i=0;i<(unilink.rxSize-2);i++){
      if(i==parity1){                         // Short messages will never get here, this is the first checksum for medium or long frame
        if(checksum!=unilink.rxData[parity1]){
          return 0;
        }
      }
      else{
        checksum+=unilink.rxData[i];
      }
    }
    if( checksum!=unilink.rxData[i] || unilink.rxData[i+1]!=0 )  {
      return 0;
    }
    return 1;                                 // Second checksum is invalid, return false
}

void unilinkColdReset(void){
  unilink.masterinit= 0;
  unilink.busReset  = 0;
  unilink.appoint   = 0;                                // We don't want appoint yet (only after "Anyone" command)
  unilink.ownAddr   = addr_reset;                       // Load default address
  unilink.groupID   = addr_reset;                       // Load default address
  unilinkWarmReset();
}

void unilinkWarmReset(void){
  unilink.powered_on= 0;
  unilink.received  = 0;
  unilink.timeout   = 0;
  unilink.rxCount   = 0;
  unilink.txCount   = 0;
  slaveBreak.in     = 0;
  slaveBreak.out    = 0;
  slaveBreak.pending= 0;
  unilink.play      = 0;
  unilink.status    = unilink_idle;
  slaveBreak.msg_state = break_msg_idle;
  unilink_spi_mode(mode_rx);

  #ifdef AUDIO_SUPPORT
  if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause)           // Audio playing or paused
    AudioStop();                                                                            // Stop audio
#endif

  unilink_reset_playback_time();

  if(unilink.ownAddr == addr_reset)
    putString("COLD RESET\r\n");
  else
    putString("WARM RESET\r\n");
}

void unilink_data_mode(unilink_DATAmode_t mode){                    // Set DATA pin mode
  if(mode==mode_SPI){                                               // SPI
    setPinMode(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin, MODE_AF);
  }
  else if(mode==mode_input){                                        // GPIO Input
    setPinMode(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin, MODE_INPUT);
  }
  else{                                                             // GPIO Output (Low)
    setPinMode(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin, MODE_OUTPUT);
  }
}

void unilink_wait_spi_busy(void){                     // Wait until SPI flag busy clears out
  uint32_t t = HAL_GetTick()+2;                       // 2ms timeout should be more than enough
  while(unilink.SPI->Instance->SR & SPI_SR_BSY){      // Wait until Busy is gone
    if(HAL_GetTick() > t){
      putString("SPI Timeout! Resetting system\r\n");
      HAL_Delay(100);                                 // Some time to send the message just in case
      __NVIC_SystemReset();                           // Busy is not going out, we've already tried everything, so reset the system (Should never happen)
    }
  }
}

void unilink_spi_mode(unilink_SPImode_t mode){
  __HAL_SPI_DISABLE(unilink.SPI);
  __HAL_SPI_CLEAR_OVRFLAG(unilink.SPI);
  __HAL_SPI_CLEAR_FREFLAG(unilink.SPI);
  __HAL_SPI_CLEAR_MODFFLAG(unilink.SPI);

  __HAL_SPI_DISABLE_IT(unilink.SPI, (SPI_IT_RXNE | SPI_IT_TXE));
  unilink_data_mode(mode_SPI);                                // Set DATA pin to SPI

  unilink.timeout = 0;
  unilink.bad_checksum=0;
  unilink.rxCount = 0;
  unilink.txCount = 0;

  if((unilink.SPI->State>HAL_SPI_STATE_READY) && (unilink.SPI->State<HAL_SPI_STATE_ERROR)){   // Handler BUSY in any mode (But not error or reset);
    unilink.SPI->State=HAL_SPI_STATE_READY;
  }
  else if((unilink.SPI->Instance->SR & SPI_SR_BSY) || (unilink.SPI->State!=HAL_SPI_STATE_READY)){  // If peripheral is busy (Probably missed some clocks and it's waiting for more), or handler error
    unilink.SPI->State=HAL_SPI_STATE_RESET;

    #ifdef SPI1
    if(unilink.SPI->Instance == SPI1){
      __HAL_RCC_SPI1_FORCE_RESET();                     // Reset SPI1
      unilink_wait_spi_busy();
      __HAL_RCC_SPI1_RELEASE_RESET();                   // Release reset
    }
    #endif
    /*
    #ifdef SPI2                                         // XXX: Only required if SPI module is changed
    if(unilink.SPI->Instance == SPI2){
      __HAL_RCC_SPI2_FORCE_RESET();                     // Reset SPI2
      unilink_wait_spi_busy();
      __HAL_RCC_SPI2_RELEASE_RESET();                   // Release reset
    }
    #endif
    #ifdef SPI3
    if(unilink.SPI->Instance == SPI3){
      __HAL_RCC_SPI3_FORCE_RESET();                     // Reset SPI3
      unilink_wait_spi_busy();
      __HAL_RCC_SPI3_RELEASE_RESET();                   // Release reset
    }
    #endif
    #ifdef SPI4
    if(unilink.SPI->Instance == SPI4){
      __HAL_RCC_SPI4_FORCE_RESET();                     // Reset SPI4
      unilink_wait_spi_busy();
      __HAL_RCC_SPI4_RELEASE_RESET();                   // Release reset
    }
    #endif
    #ifdef SPI5
    if(unilink.SPI->Instance == SPI5){
      __HAL_RCC_SPI5_FORCE_RESET();                     // Reset SPI5
      unilink_wait_spi_busy();
      __HAL_RCC_SPI5_RELEASE_RESET();                   // Release reset
    }
    #endif
    */
    unilink_wait_spi_busy();                            // Check BSY again

    if (HAL_SPI_Init(unilink.SPI) != HAL_OK){ Error_Handler(); }// Re-init SPI
  }

  if(mode == mode_rx){
    unilink.mode = mode_rx;
    SPI_1LINE_RX(unilink.SPI);
    __HAL_SPI_ENABLE_IT(unilink.SPI, (SPI_IT_RXNE | SPI_IT_ERR));
  }
  else{
    unilink.mode = mode_tx;
    SPI_1LINE_TX(unilink.SPI);
    __HAL_SPI_ENABLE_IT(unilink.SPI, (SPI_IT_TXE | SPI_IT_ERR));
  }
  __HAL_SPI_ENABLE(unilink.SPI);
}

void unilink_reset_playback_time(void){
  __disable_irq();
  unilink.min=0;                          // reset playing time
  unilink.sec=0;
  unilink.millis=0;
  __enable_irq();
}


/*
 *  Keeps the master updated from what's going on.
 *  Sends different messages which don't have specific poll commands from the master.
 *  status, discinfo, time, are all responses to the generic "Slave poll" command, received after generating a slave break event.
 */
uint8_t unilink_auto_status(void){
  switch(unilink.lastAutoStatus){             // What was the last "auto" sent command?
    case cmd_status:                          // We sent Status, now send disc info
      if(unilink.status==unilink_playing){
        unilink.lastAutoStatus=cmd_discinfo;
      }
      break;
    case cmd_discinfo:                        // We sent Disc Info
      if(unilink.status==unilink_playing){    // If playing, send Time
        unilink.lastAutoStatus=cmd_time;
      }
      else{
        unilink.lastAutoStatus=cmd_status;    // Else, send Status
      }
      break;
    case cmd_time:                            // We sent Time
    default:                                  // Or other
      unilink.lastAutoStatus=cmd_status;      // Send status
      break;
  }
  return(unilink.lastAutoStatus);
}

void unilink_add_slave_break(uint8_t command){
  uint8_t i=slaveBreak.in;                            // Input buffer index
  if(slaveBreak.pending>=_BREAK_QUEUE_SZ_){           // slave break queue full?
    slaveBreak.lost++;                                // For debugging purposes, shouldn't happen
    return;
  }
  switch(command){
    case cmd_magazine:                                // magazine info
    {
      uint8_t msg[]=msg_magazine;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_discinfo:                                // disc total time and tracks
    {
      uint8_t msg[]=msg_discinfo;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_cfgchange:                               // mode
    {
      uint8_t msg[]=msg_cfgchange;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_time:                                    // time info update
    {
      uint8_t msg[]=msg_time;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_status:                                  // status
    {
      uint8_t msg[]=msg_status;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_cartridgeinfo:                           // info about the cartridge
    {
      if(mag_data.status==mag_slot_empty){
        uint8_t msg[]=msg_mag_slot_empty;
        unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      }
      else{
        uint8_t msg[]=msg_cartridge_info;
        unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      }
      break;
    }
    case cmd_dspdiscchange:                           // Sent by the original cd changer after changing cd
    {
      uint8_t msg[]=msg_dspchange;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_intro_end:                               // Sent by the original cd changer after changing cd
    {
      uint8_t msg[]=msg_intro_end;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    default:
    {
      uint8_t msg[]=msg_status;
      putString("unilink_add_slave_break: Unknown cmd!\r\n");
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
  }
  if(slaveBreak.data[i][cmd1] == cmd_status){                       // Update our status after sending current one
    unilink_update_status();
  }
  if(++slaveBreak.in >= _BREAK_QUEUE_SZ_){
    slaveBreak.in=0;
  }
  __disable_irq();
  slaveBreak.pending++;
  __enable_irq();
}

void unilink_clear_slave_break_queue(void){
  if(slaveBreak.msg_state > break_msg_pending){
    putString("Clear slave break: Busy, waiting...\r\n");         // Shouln't happen as this is called inmediately after receiving a command and no TX should be going on
  }
  while(slaveBreak.msg_state > break_msg_pending);                // If busy, wait until until completed
  __disable_irq();
  slaveBreak.pending=0;
  slaveBreak.in=0;
  slaveBreak.out=0;
  __enable_irq();
}

/****************************************************************
 *          unilink_handle_slave_break               *
 *                                 *
 *   Continually checks for breaks stored in the queue      *
 *   and reads the data line conditions to generate the break  *
 *   If the queue is empty it will automatically generate a new  *
 *   break, to keep the master updated              *
 *                                 *
 ****************************************************************/
void unilink_handle_slave_break(void){

  if( !unilink.masterinit ||                          // Exit if not initialized,
      unilink.mode==mode_tx ||                        // transmitting,
      (unilink.mode==mode_rx && unilink.rxCount) ){   // or reception going on

    return;
  }
  switch(slaveBreak.break_state){

    case break_wait_data_low_err:
      slaveBreak.break_counter=0;
      slaveBreak.break_state=break_wait_data_low;
      break;

    case break_wait_data_low:
      if(isDataLow()){
        slaveBreak.dataTime=8;                                // Wait for DATA low for 8mS
        slaveBreak.break_state=break_wait_data_low_time;
      }
      break;

    case break_wait_data_low_time:
      if(slaveBreak.dataTime){                                // Timer not expired
        if(isDataHigh()){                                     // If DATA goes high,
          slaveBreak.break_state=break_wait_data_low_err;     // reset state
        }
      }
      else{                                                   // Timeout expired
        slaveBreak.dataTime=3;                                // 3ms timeout while waiting for data to go HIGH
        slaveBreak.break_state=break_wait_data_high;
      }
      break;

    case break_wait_data_high:
      if(slaveBreak.dataTime){                                // Timeout not expired
        if(isDataHigh()){                                     // If DATA high
          slaveBreak.dataTime=2;                              // Wait 2ms
          slaveBreak.break_state=break_wait_data_high_time;
        }
      }
      else{                                                   // Timeout
        slaveBreak.break_state=break_wait_data_low_err;       // DATA didn't go high within time, reset state
      }

      break;

    case break_wait_data_high_time:
      if(slaveBreak.dataTime){                                // Timeout not expired
        if(isDataLow()){                                      // If data goes low,
          slaveBreak.break_state=break_wait_data_low_err;     // reset state
        }
      }
      else{                                                   // Timeout, start break
        if( slaveBreak.pending>1 || (++slaveBreak.break_counter > _BREAK_INTERVAL_)){ // Don't use interval limiter if we have pending elements in the queue
          slaveBreak.break_counter=0;
          unilink_data_mode(mode_output);                     // Pull data low
          slaveBreak.dataTime=2;                              // Keep data low for 2ms
          slaveBreak.break_state=break_wait_data_setlow;
        }
        else{
          slaveBreak.break_state=break_wait_data_low;
        }
      }
      break;

    case break_wait_data_setlow:
      if(!slaveBreak.dataTime){                               // Timeout expired
        unilink_data_mode(mode_SPI);                          // Release data, set to SPI
        slaveBreak.msg_state=break_msg_pending;               // Break condition executed, now wait for slave poll command from master
        slaveBreak.break_state=break_wait_data_low;           // Reset status
      }
      break;

    default:
      break;
  }
}

/****************************************************************
 *          unilink_slave_msg                    *
 *                                 *
 *   Sends the correct response after a slave poll request    *
 *                                 *
 ****************************************************************/
void unilink_slave_msg(void){
  uint8_t c=0,size;
  if(slaveBreak.pending==0){                                        // If empty queue, self-generate data
    unilink_add_slave_break(unilink_auto_status());
  }
  size=slaveBreak.data[slaveBreak.out][parity2_L+2];
  while(c<size){                                                    // Copy stored slave break message to Tx Buffer
    unilink.txData[c] = slaveBreak.data[slaveBreak.out][c];
    c++;
  }
  unilink.txSize=size;
  slaveBreak.msg_state=break_msg_pendingTx;
  unilink_spi_mode(mode_tx);
}

/****************************************************************
 *          unilink_tick                  *
 *                                 *
 *   Called from sysTick() every 1mS                *
 *   Used for slave break timing, spi reset timeouts        *
 *   and keeps running a clock for telling the master      *
 *                                 *
 ****************************************************************/
void unilink_tick(void){
  if(!unilink.hwinit) return;

  if(unilink.status==unilink_playing){            // Simulates play time
    if(++unilink.millis>999){
      unilink.millis=0;
      if(++unilink.sec>59){
        unilink.sec=0;
        if(++unilink.min>59){
          unilink.min=0;
        }
      }
    }
  }
  unilink.timeout++;
  if(unilink.masterinit){
    if(unilink.timeout > _RESET_TIMEOUT_){          // No with no clock from master
      putString("Master Timeout\r\n");
      unilinkColdReset();
    }
  }
  if(slaveBreak.dataTime){                          // For sampling data pin high/low time to generate breaks
    slaveBreak.dataTime--;
  }
  if(unilink.statusTimer){                          // For delaying status changes
    unilink.statusTimer--;
  }

  if(unilink.mode==mode_tx && (unilink.timeout > _MASTER_REQUEST_TIMEOUT_)){  // Timeout while waiting clocks from master to answer a poll cmd
    if(slaveBreak.msg_state==break_msg_pendingTx){
      putString("Slave msg timeout (Pending Tx)\r\n");
      slaveBreak.msg_state=break_msg_idle;
    }
    else if(unilink.txCount < unilink.txSize){                // We expect a timeout after sending a frame, so ignore if count is ok
      putString("Answer Timeout (TX)\r\n");                   // Timeout while sending a frame
    }
    unilink.timeout=0;
    unilink_spi_mode(mode_rx);
  }
}

void unilink_byte_timeout(void){                              // Byte timeout (When the master already started sending clocks)
  if(unilink.mode==mode_rx){
    if(unilink.rxCount && unilink.rxCount<unilink.rxSize){
      if(unilink.rxData[cmd1]!=cmd_seek){                      // This command always causes timeout because the ICS doesn't support 16-byte slave poll answers (Max. is 13).
        putString("Byte timeout (RX)\r\n");                   // This only happens in passive mode, we don't send this message
      }
      unilink_spi_mode(mode_rx);
    }
    else if(unilink.bad_checksum){                            // Resync after a bad checksum, waiting until the master stops transmitting to reset our state
      unilink_spi_mode(mode_rx);
    }
  }
  else if(unilink.mode==mode_tx && (unilink.txCount>1)){      // Byte timeout when sending a frame
    if(slaveBreak.msg_state==break_msg_sending){
      slaveBreak.msg_state=break_msg_idle;
      putString("Slave msg byte timeout (TX)\r\n");
    }
    else if(unilink.txCount<unilink.txSize){
      putString("Byte timeout (TX)\r\n");
    }
    unilink_spi_mode(mode_rx);
  }
}


/****************************************************************
 *                                 *
 *   Unilink callback, called from the interrupt after    *
 *   a byte was received or sent                  *
 *                                 *
 ****************************************************************/
void unilink_callback(void){
  __HAL_TIM_SET_COUNTER(unilink.timer, 0);                        // Reset byte timeout counter
  __HAL_TIM_CLEAR_FLAG(unilink.timer, TIM_FLAG_UPDATE);
   unilink.timeout=0;

  if (unilink.mode==mode_rx && (unilink.SPI->Instance->CR2 & SPI_CR2_RXNEIE)
      && (unilink.SPI->Instance->SR & SPI_SR_RXNE)){              // If in receive mode
    uint8_t rx = ~*(__IO uint8_t *)&unilink.SPI->Instance->DR;    // store last received byte (inverted)
    if(unilink.rxCount==0){                                       // If first byte
      if( rx<0x10 || rx>0xF0 ){                                   // Ignore invalid addresses
        return;
      }
      else{                                                       // First valid byte
        unilink.rxSize=unilink_short;                             // Begin by setting frame size to short until we get cmd1
      }
    }
    if(unilink.bad_checksum){                                     // bad checksum: Discard everything until the master transmission ends, causing a byte timeout (Resync)
      return;
    }
    unilink.rxData[unilink.rxCount] = rx;                         // Store data in the buffer
    if(unilink.rxCount==cmd1){                                    // cmd1 received, detect msg size now
      if(unilink.rxData[cmd1]>=0xC0){
        unilink.rxSize=unilink_long; }                            // set packet size to long
      else if(unilink.rxData[cmd1]>=0x80){
        unilink.rxSize=unilink_medium; }                          // set packet size to medium
    }
    if(++unilink.rxCount==unilink.rxSize){                        // Frame complete
#ifdef UNILINK_LOG_ENABLE
      unilinkLogUpdate(mode_rx);
#endif
      unilink.received=1;                                         // set RX complete status flag
      unilink_spi_mode(mode_rx);                                  // Done
    }
  }
  else if (unilink.mode==mode_tx && (unilink.SPI->Instance->CR2 & SPI_CR2_TXEIE)
      && (unilink.SPI->Instance->SR & SPI_SR_TXE)){               // If in transmit mode
    if(unilink.txCount==1){                                       // First interrupt is only caused by empty SPI Tx buffer, second one actually happen when the first byte was sent
      if(slaveBreak.msg_state==break_msg_pendingTx){              // We sent the first byte, this will be the second one. If this was a slave break
        slaveBreak.msg_state=break_msg_sending;                   // Update state
      }
    }
    if(unilink.txCount<unilink.txSize){                           // check if bytes left
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = ~unilink.txData[unilink.txCount++];   // output next byte (inverted)
    }
    else{                                                         // No data left
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = ~0x00;        // Clock extra bytes as 0 (Again, inverted)
    }
    if(unilink.txCount == unilink.txSize){                        // Last byte sent
      unilink.txCount++;                                          // Done. Increase txCount so we don't get there again in the following extra clocks sent by the master
#ifdef UNILINK_LOG_ENABLE                                         // When master stops sending clocks, the byte timeout will reset back to Rx mode.
      unilinkLogUpdate(mode_tx);
#endif
      if(slaveBreak.msg_state==break_msg_sending){                // Was this a slave break response?
#ifdef UNILINK_LOG_ENABLE
        unilink.logBreak=1;
#endif
        slaveBreak.msg_state=break_msg_idle;                      // Done
        if(slaveBreak.pending){                                   // Pending should always be at least 1
          slaveBreak.pending--;                                   // Decrease
          if(++slaveBreak.out>=_BREAK_QUEUE_SZ_){;                // Increase out index
            slaveBreak.out=0;
          }
        }
        else{
          Error_Handler();                                        // We got there without pending breaks??
        }
      }
    }
  }
}

