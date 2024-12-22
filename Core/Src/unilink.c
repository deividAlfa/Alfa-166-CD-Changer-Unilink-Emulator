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

/****************************************************************
 *             VARIABLES                *
 ****************************************************************/
unilink_t       unilink;
slaveBreak_t     slaveBreak;
magazine_t       mag_data;
cdinfo_t         cd_data[MAXDISCS];



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

  mag_data.cmd2=0;                        // Clear CDs
  mag_data.CD1_present=1;                 // Set CD1 by default
  cd_data[0].tracks = 33;
  cd_data[0].mins=90;
  cd_data[0].secs=10;
  cd_data[0].inserted=1;

  unilink.mag_changed=1;                    // XXX: Check this

  setDataLow();                             // Force DATA Output low (Doesn't affect when GPIO is in SPI mode)

  unilinkColdReset();
  HAL_TIM_Base_Start_IT(unilink.timer);                           // Start timer in IT mode
  unilink.hwinit=1;                          // Set init flag
}

void unilink_handle(void){
  if(unilink.timeout>10000){                      // 30 second without any activity
    putString("No activity timeout, shutting down...\r\n");
#ifdef USB_LOG
    flush_log();                                  // flush log
    removeDrive();
#endif
    setPinLow(PWR_ON_GPIO_Port, PWR_ON_Pin);      // Turn off
    static uint32_t time=0;
    while(1){
      if(HAL_GetTick()>time){
        time=HAL_GetTick()+50;
        togglePin(LED_GPIO_Port,LED_Pin);
      }
    }
  }
  #ifdef Unilink_Log_Enable
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
      #ifdef Unilink_Log_Enable
      putString("BAD CHECKSUM\r\n");
      #endif
    }
  }
  if(unilink.mag_changed && unilink.masterinit){      // usb was inserted, removed or contents changed
    /*
    static uint8_t wait;                              // XXX: Not implemented yet?
    static uint32_t timer;
    if(wait==0){
      wait=1;
      timer=HAL_GetTick();
      mag_data.status=mag_removed;                  // Set mag status=magazine removed
      unilink_add_slave_break(cmd_magazine);                  // Send magazine list (empty)
      unilink_set_status(unilink_ejecting);                  // set mode=ejecting for 5 seconds to force the master exiting cd mode
    }

    else if (wait==1 && (HAL_GetTick()-timer>100)){
      timer=HAL_GetTick();
      unilink_set_status(unilink_idle);
      unilink.play=0;
      wait=2;
    }
    else if (wait==2 && (HAL_GetTick()-timer>100)){
      wait=0;
      unilink.mag_changed=0;                        // reset flag
      mag_data.status=mag_inserted;                 // Set mag status=magazine inserted               // TODO: untested
      unilink_add_slave_break(cmd_cartridgeinfo);                   // Add slot msg
      unilink.disc=0;                               // First cd is 1. Set to 0 so the following loop works
      for(uint8_t i=0;i<6;i++){                     // Update magazine data
        if(cd_data[i].inserted){
          if(unilink.disc==0){                      // Find first cd with files
            unilink.disc=i+1;                       // Set current disc ands track
            unilink.lastDisc=0;                     // reset last disc and track
            unilink.track=1;
            unilink.lastTrack=0;
            unilink_reset_playback_time();
            break;
          }
        }
      }
      unilink.play=0;                               // Don't go into play mode automatically
      unilink_set_status(unilink_idle);                      // Set unilink status=changing cd
      unilink_add_slave_break(cmd_status);                    // Add status msg
    }
    */

    unilink.mag_changed=0;                        // reset flag
    unilink.disc=0;                               // First cd is 1. Set to 0 so the following loop works
    for(uint8_t i=0;i<6;i++){                     // Update magazine data
      if(cd_data[i].inserted){
        if(unilink.disc==0){                      // Find first cd with files
          unilink.disc=i+1;                       // Set current disc ands track
          unilink.lastDisc=0;                     // reset last disc and track
          unilink.track=1;
          unilink.lastTrack=0;
          unilink_reset_playback_time();
          break;
        }
      }
    }
#ifdef AUDIO_SUPPORT
    if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause)          // Audio playing or paused
        AudioStop();                                                                            // Stop audio
#endif
    unilink_reset_playback_time();
    unilink.play=0;                               // Don't go into play mode automatically
    unilink_set_status(unilink_idle);                      // Set unilink status=changing cd
    unilink_add_slave_break(cmd_status);                    // Add status msg
  }
#ifdef AUDIO_SUPPORT
  if(unilink.trackChanged){                                                                   // Track changed
    if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause) {       // Audio playing or paused
      AudioStop();                                                                            // Stop audio
    }
    unilink_reset_playback_time();
    AudioStart();                                                                             // Start audio
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
#endif

#ifndef PASSIVE_MODE
  unilink_handle_slave_break();                     // Handle slave break events
#endif

  unilink_handle_led();                             // Handle activity led

}

void unilink_handle_led(void){
  static uint32_t time=0;
#if defined (PASSIVE_MODE) && defined (USB_LOG)
  if(HAL_GetTick()>time){
    time=HAL_GetTick();
    togglePin(LED_GPIO_Port,LED_Pin);
    if(systemStatus.driveStatus==drive_ready){
      time+=500;                                // Drive mounted, slow blink
    }
    else{
      time+=50;                                 // Else, fast blinking
    }
  }
#elif !defined (PASSIVE_MODE)
  if(unilink.masterinit){
    if(unilink.status==unilink_idle){
      setPinLow(LED_GPIO_Port,LED_Pin);             // If in idle state and initialized, Led on
    }
    else if(unilink.status==unilink_playing){
      if(HAL_GetTick()-time>50){
        time=HAL_GetTick();
        togglePin(LED_GPIO_Port,LED_Pin);           // If in play state, quick led blinking
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

  dest[i++]=checksum;                              // Store checksum2
  dest[i]=0;                                  // End of msg always 0

  if(dest==unilink.txData){                          // If dest is tx buffer
    unilink.txSize=size;                            // Store Tx size
    unilink_spi_mode(mode_tx);                                // Set SPI transmit mode
  }
  else{                                             // Slave break buffer
    dest[parity2_L+2]=size;                        // Store Tx size
  }
}

void unilink_parse(void){
  if(unilink.rxData[dst_addr]==addr_broadcast){
    unilink_broadcast();                      // parse broadcast packets
  }
  else if(unilink.rxData[dst_addr]==unilink.ownAddr){
    unilink_myid_cmd();                        // parse packets for my ID
  }
  else if((unilink.rxData[dst_addr]&0xF0) == unilink.groupID ){      // Appoint for us?
    unilink_appoint();                        // do ID appoint procedure
  }
}

void unilink_broadcast(void){
  switch(unilink.rxData[cmd1]){                     // Switch CMD1
    case cmd_busRequest:  // 0x01 Bus requests (Broadcast)
    {
      switch(unilink.rxData[cmd2]){                  // Switch CMD2
        case cmd_busReset:                    // 0x01 0x00 Bus reset
          unilinkColdReset();
          unilink.busReset = 1;
          break;
        case cmd_anyone:                    // 0x01 0x02 Anyone?
          if(!unilink.masterinit && unilink.busReset){
            unilink.appoint = 1;              // Enable appoint
            uint8_t msg[] = msg_anyoneResp;
            unilink_create_msg(msg,unilink.txData);             // send my device info string
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
    case cmd_source:                        // 0xF0 SRC Source select
      if (unilink.rxData[cmd2]!=unilink.ownAddr){          // check if interface is deselected
        //unilink.status = unilink_idle;                       // set idle status on deselect   // TODO: Required?
      }
      break;
    case cmd_power:                        // 0x87 Power Event
      if(unilink.rxData[cmd2]==cmd_pwroff){              // Power off
        unilink.play=0;
        unilink.powered_on=0;
        unilink_set_status(unilink_idle);                   // set idle status on power off
#ifdef AUDIO_SUPPORT
        if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause) {       // Audio playing or paused
          AudioStop();                                                                            // Stop audio
        }
#endif
        unilink_reset_playback_time();
      }
      else if(unilink.rxData[cmd2]==cmd_pwron){
        unilink.play=0;
        unilink.powered_on=1;
        unilink_set_status(unilink_idle);
      }
      break;
  }
}

void unilink_myid_cmd(void){
  switch(unilink.rxData[cmd1]){                     // Switch CMD1
    case cmd_busRequest:                      // 0x01 Bus requests (for my ID)
    {
      switch(unilink.rxData[cmd2]){
        case cmd_timePoll:                    // 0x01 0x12 Respond to time poll (PONG)
        {
          uint8_t msg[] = msg_status;
          unilink_create_msg(msg,unilink.txData);
          unilink_update_status();
          break;
        }
        case cmd_slavePoll:                    // 0x01 0x13 permission to talk after poll request
        {
          unilink_slave_msg();                                // send slave response
          break;
        }
        default:
          break;
      }
      break;
    }
    case cmd_play:                          // 0x20 PLAY
    {
      if(!unilink.powered_on){        // Not powered on, ignore
        break;
      }
      if((mag_data.status!=mag_removed)&&(unilink.status!=unilink_ejecting)){  // If magazine is present and we are not ejecting      //FIXME: Ejecting check might be wrong?
        if(cd_data[unilink.disc-1].inserted){              // If current selected disc is valid
          if(unilink.track>cd_data[unilink.disc-1].tracks){      // If current track is valid
            unilink.track=1;                    // Else, reset track
            unilink.lastTrack=1;
          }
          unilink.play=1;
          //unilink_set_status(unilink_changing);                            // set changing status
          unilink_set_status(unilink_playing);                            // set changing status      TODO_ test
          unilink_add_slave_break(cmd_status);
        }
        else{
          for(uint8_t i=0;i<6;i++){                  // Else, find the first valid disc
            if(cd_data[i].inserted){
              unilink.disc=i+1;
              unilink.lastDisc=i+1;
              unilink.track=1;
              unilink.lastTrack=1;
              unilink.play=1;
              //unilink_set_status(unilink_changing);                        // set changing status
              unilink_set_status(unilink_playing);                            // set changing status      TODO_ test
              unilink_add_slave_break(cmd_status);
              break;
            }
          }
          if(cd_data[unilink.disc-1].mins==0){            // No valid cd was found
            unilink_set_status(unilink_idle);                            // set idle status
            unilink_add_slave_break(cmd_status);
          }
        }
      }
      else{                                // If we can't go into play mode
        unilink.play=0;
        unilink_add_slave_break(cmd_status);                    // Send unilink status
        mag_data.status = mag_removed;
        unilink_add_slave_break(cmd_cartridgeinfo);                    // Send magazine status: magazine was removed
        unilink_set_status(unilink_idle);                            // set idle status
        unilink_add_slave_break(cmd_status);
        unilink_add_slave_break(cmd_magazine);                  // Send magazine list
      }
      break;
    }
    case cmd_switch:                        // 0x21 TA message start
    {
      if(unilink.rxData[cmd2] == 0x20){
        unilink_set_status(unilink_idle);                            // set idle status
      }
      if(unilink.rxData[cmd2] == 0x10){
      }
      break;
    }

    case cmd_fastFwd:                        // 0x24 Fast Forward
    {
      break;
    }
    case cmd_fastRwd:                        // 0x25 Fast Reverse
    {
      break;
    }

    case cmd_repeat:                        // 0x34 Repeat mode change
    {
      switch(unilink.rxData[cmd2]){
        case 0x00:
        {
          //repeat_mode(0);
          break;
        }
        case 0x10:
        {
          //repeat_mode(1);
          break;
        }
      }
      unilink_add_slave_break(cmd_cfgchange);                  // Add command to queue
      //unilink.txCmd=cmd_mode;
      break;
    }
    case cmd_shuffle:                        // 0x35 Shuffle mode change
    {
      switch(unilink.rxData[cmd2]){
        case 0x00:
        {
          //shuffle_mode(0);
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
    case cmd_textRequest:                      // 0x84 request for command
    {
      switch(unilink.rxData[cmd2]){
        case cmd_magazine:                    // 0x95 request magazine info
        {
          uint8_t msg[] = msg_magazine;            // send mag info
          unilink_create_msg(msg, unilink.txData);
          /*
          if((mag_data.status!=mag_removed)&&(unilink.status!=unilink_ejecting)){               // OLD
          }
          else{
            uint8_t msg[] = msg_mag_slot_empty;
            unilink_create_msg(msg,unilink.txData);
          }
          */
          break;
        }
        case cmd_discinfo:                    // 0x97 request disc total time and tracks
        {
          if((mag_data.status!=mag_removed)&&(unilink.status!=unilink_ejecting)){
            uint8_t msg[]=msg_discinfo;
            unilink_create_msg(msg,unilink.txData);
          }
          else{
            uint8_t msg[]=msg_discinfo_empty;
            unilink_create_msg(msg,unilink.txData);
          }
          //unilink.txCmd=cmd_discinfo;
          break;
        }
        default:
          break;
      }
      break;
    }
    case cmd_goto:                          // 0xB0 Direct Disc keys
    {
      unilink.millis=0;
      unilink.sec=0;
      unilink.min=0;
      uint8_t disc=unilink.rxData[cmd2]&0x0F;
      uint8_t track=bcd2hex(unilink.rxData[d1]);
      if(track==0){ track=1; }
      if(disc==0){ disc=1; }

      unilink.lastTrack = unilink.track;
      unilink.track = track;
      unilink.trackChanged = 1;
      unilink_add_slave_break(cmd_status);                  // send status changing (updates to changed)
      if(unilink.disc != disc){             // check for disc change
        //unilink.play=0;
        unilink_clear_slave_break_queue();
        unilink.lastDisc = unilink.disc;
        unilink.disc = disc;
        //unilink_add_slave_break(cmd_discinfo);
        unilink_set_status(unilink_changed);
        unilink_add_slave_break(cmd_status);
        unilink_add_slave_break(cmd_magazine);
        //unilink_set_status(unilink_changed);
        //unilink_add_slave_break(cmd_status);
        /*
        unilink_set_status(unilink_changed);
        unilink_add_slave_break(cmd_status);
        */

        /*
        if( unilink.lastDisc<unilink.disc){
          //prev disc stuff
        }
        else if(unilink.lastDisc>unilink.disc){
          //next disc stuff                     // TODO: Implement this for bluetooth
        }
        */
        //unilink_set_status(unilink_changing);
        if((mag_data.status!=mag_removed)&&(unilink.status!=unilink_ejecting)){
          if(!cd_data[unilink.disc-1].inserted){      // If requested disc not present
            mag_data.status=mag_slot_empty;
            unilink_add_slave_break(cmd_cartridgeinfo);              // Add empty slot msg

            for(uint8_t i=0;i<6;i++){                // Find next valid disc
              if(cd_data[unilink.disc-1].inserted){
                break;
              }
              if(++unilink.disc>6){
                unilink.disc=1;
              }
            }
            if(!cd_data[unilink.disc-1].inserted){    // No discs on system
              unilink.trackChanged = 0;             // Abort track change
              unilink_set_status(unilink_idle);              // Idle state
              //unilink_add_slave_break(cmd_status);          //
            }
            else{
              unilink.lastDisc=unilink.disc;        // Update disk
              //unilink.play=1;
              //unilink_add_slave_break(cmd_status);          // changed->playing
              //unilink_add_slave_break(cmd_dspdiscchange);        // Add dsp changed disc
              //unilink_add_slave_break(cmd_status);          // playing
            }
          }
          if(unilink.status != unilink_idle){
            unilink_add_slave_break(cmd_discinfo);
            unilink_add_slave_break(cmd_cfgchange);
            unilink_add_slave_break(cmd_status);
            unilink_add_slave_break(cmd_dspdiscchange);
            unilink_add_slave_break(cmd_intro_end);
            unilink_set_status(unilink_playing);
            unilink_add_slave_break(cmd_status);
          }
        }
        else{
          for(uint8_t i=0;i<6;i++){                // Find next valid disc
            cd_data[unilink.disc-1].inserted=0;
          }
           mag_data.status=mag_removed;
           unilink_add_slave_break(cmd_cartridgeinfo);                                // FIXME: Properly find how to deal with this
           unilink_set_status(unilink_idle);                // Idle state
           unilink_add_slave_break(cmd_status);                //
           mag_data.cmd2=0;
           unilink_add_slave_break(cmd_magazine);
           unilink.play=0;
        }
      }
      break;
    }
    default:
      break;
  }
}

void unilink_appoint(void){                        // respond to ID appoint
  if((unilink.rxData[cmd1]==0x02) && unilink.appoint){          // check for previous Anyone command
    if ((unilink.rxData[dst_addr]&0xF0)==unilink.groupID){      // check if packet is for my group
      if(!unilink.masterinit){                  // I have no ID
        unilink.masterinit=1;
        unilink.appoint=0;                    // Disable appoint
        unilink.ownAddr = unilink.rxData[dst_addr];         // save my new ID
        uint8_t msg[] = msg_anyoneResp;            // Generate response
        unilink_create_msg(msg,unilink.txData);             // send my device info string
      }
    }
  }
}

void unilink_update_status(void){
  if(unilink.statusTimer){ return; }      // Wait 100mS between changes
  //unilink.statusTimer=100;          // Load 100mS
  switch(unilink.status){
    case unilink_playing:
    case unilink_idle:
    case unilink_ejecting:
      break;
    case unilink_changed:
      /*
      if(unilink.play==1){
        unilink.status = unilink_seeking;
      }
      else{
        unilink.status = unilink_idle;
      }
      */
      break;
    case unilink_seeking:
      //unilink.status = unilink_playing;
      break;
    case unilink_changing:
      //unilink.status = unilink_changed;
      break;
    default:
      unilink.status = unilink_idle;
      break;
  }
}
void unilink_set_status(uint8_t status){
  //unilink.statusTimer=100;                    // Load 100mS
  if(unilink.status==unilink_playing && status!=unilink_playing){
    __NOP();
  }
  unilink.status=status;
}

bool unilink_checksum(void){                             // check parity of complete Unilink packet
  uint8_t count=0;                              // local byte counter
    uint8_t size=unilink.rxSize-2;                    // size-2 (subtract checksum and null termination bytes)
    uint8_t checksum=0;                          // local checksum

    while(count<parity1){                             // calculate checksum for byte 1-4
      checksum+=unilink.rxData[count++];                // add to checksum
    }
    if(checksum==unilink.rxData[count]){                // verify the 1st checksum, skip rest if is invalid
      if(count==size){                             // check if short packet
        if(unilink.rxData[count+1]==0){       //
          return 1;                            // return with true if checksum 1 is ok AND end byte is 0x00 (short)
        }
      }
    }
    else{
      return 0;                                   // if checksum 1 or end byte is invalid, return false
    }
    count++;                                    // skip checksum byte (4)
    while(count<size){                          // calculate checksum for all other bytes
      checksum+=unilink.rxData[count++];                // add to checksum
    }
    if(count==size){                           // check for medium or long packet
      if(checksum==unilink.rxData[count]){
        if(unilink.rxData[count+1] == 0){
          return 1;                          // return with true if checksum 2 is ok AND end byte is 0x00 (medium or long)
        }
      }
    }
    return 0;                                     // if checksum 2 or end byte is invalid, return false
}

void unilinkColdReset(void){

  unilink.busReset   = 0;
  unilink.masterinit  = 0;
  unilink.appoint   = 0;                                // We don't want appoint yet (only after "Anyone" command)
  unilink.ownAddr   = addr_reset;                       // Load default address
  unilink.groupID   = addr_reset;                       // Load default address
  unilinkWarmReset();
}

void unilinkWarmReset(void){
  /*
  static uint8_t count=0;                               // XXX: Probably not necessary?
  static uint32_t last=3000;                            // Skip checks for the first 3 seconds
  uint32_t now=HAL_GetTick();
  count++;
  if(now>3000){
    if((now-last)<5000){                                // If having resets too often, something's wrong
      if(count>2){                                      // Increase value, if too high
        count=0;                                        // Cold reset
        unilink.busReset   = 0;
        unilink.masterinit  = 0;
        unilink.appoint   = 0;                          // We don't want appoint yet (Only after "Anyone?" command)
        unilink.ownAddr   = addr_reset;                 // Load default address
        unilink.groupID   = addr_reset;                 // Load default address
      }
    }
    else{
      count=1;                                          // Reset count if the previous was long time ago
    }
  }
  last=now;
*/


  unilink.timeout    = 0;
  unilink.rxCount    = 0;                               // Reset rx counter
  unilink.txCount    = 0;                               // Reset tx counter
  unilink.play=0;
  unilink.powered_on=0;
  unilink.received  = 0;                                // Clear received flag
  unilink.status    = unilink_idle;                     // Idle status
  slaveBreak.BfPos  = 0;                                // Clear all slavebreak stuff
  slaveBreak.SendPos   = 0;                             //
  slaveBreak.pending  = 0;                              //
  slaveBreak.msg_state  = break_msg_idle;                      //
  unilink_spi_mode(mode_rx);                                  // Start SPI in receive mode

  #ifdef AUDIO_SUPPORT
  if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause)           // Audio playing or paused
    AudioStop();                                                                            // Stop audio
#endif

  unilink_reset_playback_time();

#ifdef Unilink_Log_Enable
  if(unilink.ownAddr == addr_reset)
    putString("COLD RESET\r\n");
  else
    putString("WARM RESET\r\n");
#endif
}

void unilink_data_mode(unilink_DATAmode_t mode){
  if(mode==mode_SPI){    // SPI DATA
    setPinMode(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin, MODE_AF);
  }
  else if(mode==mode_input){  // GPIO Input
    setPinMode(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin, MODE_INPUT);
  }
  else{        // GPIO Output
    setPinMode(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin, MODE_OUTPUT);
  }
}

void unilink_wait_spi_busy(void){
  uint32_t t = HAL_GetTick()+2;                       // 2ms timeout should be more than enough
  while(unilink.SPI->Instance->SR & SPI_SR_BSY){      // Wait until Busy is gone
    if(HAL_GetTick() > t){
#ifdef Unilink_Log_Enable
      putString("SPI Timeout! Resetting system\r\n");
      HAL_Delay(100);                                 // Some time to send the message just in case
#endif
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
    #ifdef SPI2                                                             // XXX: Only required if SPI module is changed
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
    /*
    unilink.SPI->Init.CLKPolarity = SPI_POLARITY_LOW;
    unilink.SPI->Init.CLKPhase = SPI_PHASE_2EDGE;
    */
    unilink.mode = mode_rx;
    SPI_1LINE_RX(unilink.SPI);
    __HAL_SPI_ENABLE_IT(unilink.SPI, (SPI_IT_RXNE | SPI_IT_ERR));
  }
  else{
    /*
    unilink.SPI->Init.CLKPolarity = SPI_POLARITY_LOW;
    unilink.SPI->Init.CLKPhase = SPI_PHASE_2EDGE;
    */
    unilink.mode = mode_tx;                                 // Enable Tx mode
    SPI_1LINE_TX(unilink.SPI);
    __HAL_SPI_ENABLE_IT(unilink.SPI, (SPI_IT_TXE | SPI_IT_ERR));
  }

  //MODIFY_REG(unilink.SPI->Instance->CR1, SPI_CR1_CPHA | SPI_CR1_CPOL, unilink.SPI->Init.CLKPolarity | unilink.SPI->Init.CLKPhase);
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
  static uint8_t lastcmd = 0xff;
  switch(lastcmd){                          // What was the last "auto" sent command?
    case cmd_status:                        // We sent Status, now send disc info
      //lastcmd=cmd_magazine;               // XXX: Magazine is polled by master, not required
      lastcmd=cmd_discinfo;
      break;
/*
    case cmd_magazine:                      // Polled by master, not required
      lastcmd=cmd_discinfo;
      break;
*/
    case cmd_discinfo:                      // We sent Disc Info
      if(unilink.status==unilink_playing){  // If playing, send Time
        lastcmd=cmd_time;
      }
      else{
        lastcmd=cmd_status;                 // Else, send Status
      }
      break;
    case cmd_time:                          // We sent Time
    default:                                // Or other
      lastcmd=cmd_status;                   // Send status
      break;
  }
  return(lastcmd);
}

void unilink_add_slave_break(uint8_t command){
  uint8_t i=slaveBreak.BfPos;               // Current buffer index
  if(slaveBreak.pending>=BrkSiz){           // slave break queue full?
    slaveBreak.lost++;
    return;
  }
  switch(command){
    case cmd_magazine:                        // magazine info
    {
      uint8_t msg[]=msg_magazine;
/*
      static uint8_t last;
      if(last==addr_master){
        msg[dst_addr] = addr_display;   // Alternate disp and disp2
      }
      else if(last==addr_display){
        msg[dst_addr] = addr_display2;   // Alternate disp and disp2
      }
      last = msg[dst_addr];
      */
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_discinfo:                        // disc total time and tracks
    {
      if(mag_data.status==mag_removed){
        uint8_t msg[]=msg_discinfo_empty;
        unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      }
      else{
        uint8_t msg[]=msg_discinfo;
        unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      }
      break;
    }
    case cmd_cfgchange:                          // mode
    {
      uint8_t msg[]=msg_cfgchange;
      /*
      static uint8_t last;
      if(last==addr_master){
        msg[dst_addr] = addr_display;   // Alternate disp and disp2
      }
      else if(last==addr_display){
        msg[dst_addr] = addr_display2;   // Alternate disp and disp2
      }
      last = msg[dst_addr];
      */
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_time:                          // time info update
    {
      uint8_t msg[]=msg_time;
      /*
      if(msg[4]<0x10){
        msg[4] |= 0xF0;
      }
      if(msg[5]<0x10){
        msg[5] |= 0xF0;
      }
      */
/*
      static uint8_t last;
      if(last==addr_master){
        msg[dst_addr] = addr_display;   // Alternate disp and disp2
      }
      else if(last==addr_display){
        msg[dst_addr] = addr_display2;   // Alternate disp and disp2
      }
      last = msg[dst_addr];
*/
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_status:                        // status
    {
      uint8_t msg[]=msg_status;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_cartridgeinfo:
    {
      if(mag_data.status==mag_slot_empty){
        uint8_t msg[]=msg_mag_slot_empty;
        unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      }
      else{
        uint8_t msg[]=msg_cartridge_info;
        /*
        static uint8_t last;
        if(last==addr_master){
          msg[dst_addr] = addr_display;   // Alternate disp and disp2
        }
        else if(last==addr_display){
          msg[dst_addr] = addr_display2;   // Alternate disp and disp2
        }
        last = msg[dst_addr];
        */
        unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      }
      break;
    }
    case cmd_dspdiscchange:
    {
      uint8_t msg[]=msg_dspchange;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    case cmd_intro_end:
    {
      uint8_t msg[]=msg_intro_end;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
    default:                            // Unknown
    {
      uint8_t msg[]=msg_status;
      unilink_create_msg(msg,(uint8_t*)slaveBreak.data[i]);
      break;
    }
  }
  if(slaveBreak.data[i][cmd1] == cmd_status){
    unilink_update_status();
  }
  if(++slaveBreak.BfPos >= BrkSiz){
    slaveBreak.BfPos=0;
  }
  __disable_irq();
  slaveBreak.pending++;
  __enable_irq();
}

void unilink_clear_slave_break_queue(void){
#ifdef Unilink_Log_Enable
  if(slaveBreak.msg_state > break_msg_pending){
    putString("Clear slave break: Busy, waiting...\r\n");   // Shouln't happen as this is called inmediately after receiving a B0 Goto command
  }
#endif
  while(slaveBreak.msg_state > break_msg_pending);               // If busy, wait until until completed
  __disable_irq();
  slaveBreak.pending=0;
  slaveBreak.BfPos=0;
  slaveBreak.SendPos=0;
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
  static uint8_t brk_cnt;
  if( !unilink.masterinit ||                          // Exit if not initialized,
      unilink.mode==mode_tx ||                        // transmitting,
      (unilink.mode==mode_rx && unilink.rxCount) ){   // or reception going on

    return;
  }
  switch(slaveBreak.break_state){

    case break_wait_data_low_err:
      brk_cnt=0;
      slaveBreak.break_state=break_wait_data_low;
      break;

    case break_wait_data_low:
      if(isDataLow()){
        slaveBreak.dataTime=7;                                // Wait for DATA low for 7mS
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
        slaveBreak.dataTime=3;                                // 3ms timeout while waiting for data HIGH
        slaveBreak.break_state=break_wait_data_high;
      }
      break;

    case break_wait_data_high:
      if(slaveBreak.dataTime){                                // Timeout not expired
        if(isDataHigh()){                                     // If for DATA high
          slaveBreak.dataTime=3;                              // Wait for data high for 3ms
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
          slaveBreak.break_state=break_wait_data_low_err;     // reset to state
        }
      }
      else{                                                   // Timeout, start break
        if(++brk_cnt > break_interval){
          brk_cnt=0;
          unilink_data_mode(mode_output);                     // Pull data low
          slaveBreak.dataTime=3;                              // Keep data low for 3ms
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
  if(slaveBreak.pending==0){                      // If empty queue, generate data
    unilink_add_slave_break(unilink_auto_status());                  // Add new data
  }
  size=slaveBreak.data[slaveBreak.SendPos][parity2_L+2];
  while(c<size){                            // Copy stored slave break message to Tx Buffer
    unilink.txData[c] = slaveBreak.data[slaveBreak.SendPos][c];
    c++;
  }
  unilink.txSize=size;                        // Copy Tx size
  slaveBreak.msg_state=break_msg_pendingTx;
  unilink_spi_mode(mode_tx);                                // Set SPI transmit mode
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

  if(unilink.status==unilink_playing){          // Simulates play time
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
  if(unilink.masterinit){                       // If >2000mS with no clock from master
    if(unilink.timeout > master_clk_timeout){
#ifdef Unilink_Log_Enable
      putString("Master Timeout\r\n");
#endif
      unilinkColdReset();                       // Warm reset (don't reset our ID)
    }
  }
  if(slaveBreak.dataTime){                      // For sampling data high/low time
    slaveBreak.dataTime--;
  }
  if(unilink.statusTimer){                      // For delaying status changes
    unilink.statusTimer--;
  }

  if(unilink.mode==mode_tx && (unilink.timeout > answer_clk_timeout)){
    if(slaveBreak.msg_state==break_msg_pendingTx){
#ifdef Unilink_Log_Enable
      putString("Slave msg timeout (Pending Tx)\r\n");
#endif
      slaveBreak.msg_state=break_msg_idle;
    }
    else if(unilink.txCount < unilink.txSize){               // We expect a timeout after sending a frame
#ifdef Unilink_Log_Enable
      putString("Answer Timeout (TX)\r\n");                  // Timeout while sending a frame!
#endif
    }
    unilink.timeout=0;
    unilink_spi_mode(mode_rx);                                  // Set RX mode
  }
}

void unilink_byte_timeout(void){
  if(unilink.mode==mode_rx){
    if(unilink.rxCount && unilink.rxCount<unilink.rxSize){
#ifdef Unilink_Log_Enable
      if(unilink.rxData[cmd1]!=cmd_seek)                            // This command always causes timeout for the ICS
        putString("Byte timeout (RX)\r\n");
#endif
      unilink_spi_mode(mode_rx);                                  // Set RX mode
    }
    else if(unilink.bad_checksum){
      unilink_spi_mode(mode_rx);                                  // Set RX mode
    }
  }
  else if(unilink.mode==mode_tx && (unilink.txCount>1)){                       // Byte timeout when sending a frame
    if(slaveBreak.msg_state==break_msg_sending){
      slaveBreak.msg_state=break_msg_idle;
#ifdef Unilink_Log_Enable
      putString("Slave msg byte timeout (TX)\r\n");
#endif
    }
    else if(unilink.txCount<unilink.txSize){
#ifdef Unilink_Log_Enable
      putString("Byte timeout (TX)\r\n");
#endif
    }
    unilink_spi_mode(mode_rx);                                  // Set RX mode
  }
}


/****************************************************************
 *                                 *
 *   Unilink callback, called from the interrupt after    *
 *   a byte was received or sent                  *
 *                                 *
 ****************************************************************/
/*
void unilink_callback(void){
  __HAL_TIM_SET_COUNTER(unilink.timer, 0);        // Reset timeout counter
  unilink.timeout=0;

  if (unilink.mode==mode_rx && (unilink.SPI->Instance->CR2 & SPI_CR2_RXNEIE) && (unilink.SPI->Instance->SR & SPI_SR_RXNE)){                                      // If in receive mode
    uint8_t rx = ~*(__IO uint8_t *)&unilink.SPI->Instance->DR;          // store last received byte (inverted)
    if(unilink.rxCount==0){                                       // If first byte
      if(rx==0){                                                  // skip if 1st byte of packet is 0x00
        return;
      }                                                           // If first valid byte
   }
    unilink.rxData[unilink.rxCount] = rx;                     // If valid, store data in the buffer
    if(unilink.rxCount<cmd1){                                     // if didn't receive cmd1 yet
      unilink.rxSize=6;                                           // set packet size to short
    }
    else if(unilink.rxCount==cmd1){                               // if cmd1 received, detect msg size now
      if(unilink.rxData[cmd1]>=0xC0){ unilink.rxSize=16; }        // set packet size to long
      else if(unilink.rxData[cmd1]>=0x80){ unilink.rxSize=11; }   // set packet size to medium
      else{ unilink.rxSize=6; }                                   // set packet size to short
    }
    if(unilink.bad_checksum){                                     // Discard everything until a byte timeout happens (Resync)
      return;
    }
    if(++unilink.rxCount>=unilink.rxSize){               // Increase counter, if packet complete
      unilink.rxCount=0;                                  // reset counter
      #ifdef Unilink_Log_Enable
      unilinkLogUpdate(mode_rx);                                    //
      #endif
      unilink.received=1;                                 // set RX complete status flag
    }
  }
  else if (unilink.mode==mode_tx && (unilink.SPI->Instance->CR2 & SPI_CR2_TXEIE) && (unilink.SPI->Instance->SR & SPI_SR_TXE)){ // If in transmit mode
    if(unilink.txCount==1){                               // First interrupt is only caused by the empty buffer, second byte happens when the first byte was sent
      if(slaveBreak.msg_state==break_msg_pendingTx){             // Was this the first byte for a slave poll answer?
        slaveBreak.msg_state=break_msg_sending;                  // OK, we are sending a slave response
      }
    }
    if(unilink.txCount<unilink.txSize){                   // check if bytes left
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = ~unilink.txData[unilink.txCount];   // output next byte (inverted)
      unilink.txCount++;
    }
    else{
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = 0xFF;   // Any other incoming clock, send 0
    }
    if(unilink.txCount == unilink.txSize){                // Last byte sent
      unilink.txCount++;
      #ifdef Unilink_Log_Enable
      unilinkLogUpdate(mode_tx);                                    //
      #endif
      if(slaveBreak.msg_state==break_msg_sending){               // Were we sending a slave response?
        #ifdef Unilink_Log_Enable
        unilink.logBreak=1;
        #endif
        slaveBreak.msg_state=break_msg_idle;                     // Done
        if(slaveBreak.pending){                           // We should have something here
          slaveBreak.pending--;                           // Decrease
          if(++slaveBreak.SendPos>(BrkSiz-1)){;           // Increase sendPos
            slaveBreak.SendPos=0;
          }
        }
      }
    }
                                                     // Do nothing until master stop sending clocks
                                                          // And the timeout happens (resetting spi to Rx mode)

  }
}

*/
void unilink_callback(void){
  __HAL_TIM_SET_COUNTER(unilink.timer, 0);        // Reset byte timeout counter
   unilink.timeout=0;

  if (unilink.mode==mode_rx && (unilink.SPI->Instance->CR2 & SPI_CR2_RXNEIE) && (unilink.SPI->Instance->SR & SPI_SR_RXNE)){                                      // If in receive mode
    uint8_t rx = ~*(__IO uint8_t *)&unilink.SPI->Instance->DR;          // store last received byte (inverted)
    if(unilink.rxCount==0){                                       // If first byte
      if( rx<0x10 || rx>0xF0 ){                         // Filter invalid dst addresses
        return;
      }                                                           // If first valid byte
    }
    if(unilink.bad_checksum){                                     // Discard everything until a byte timeout happens (Resync)
      return;
    }
    unilink.rxData[unilink.rxCount] = rx;                     // If valid, store data in the buffer
    if(unilink.rxCount<cmd1){                                     // if didn't receive cmd1 yet
      unilink.rxSize=unilink_short;                                           // set packet size to short
    }
    else if(unilink.rxCount==cmd1){                               // if cmd1 received, detect msg size now
      if(unilink.rxData[cmd1]>=0xC0){
        unilink.rxSize=unilink_long; }        // set packet size to long
      else if(unilink.rxData[cmd1]>=0x80){
        unilink.rxSize=unilink_medium; }   // set packet size to medium
      else{
        unilink.rxSize=unilink_short; }                                   // set packet size to short
    }
    if(++unilink.rxCount==unilink.rxSize){                //Packet complete
      #ifdef Unilink_Log_Enable
      unilinkLogUpdate(mode_rx);
      #endif
      unilink.received=1;                                 // set RX complete status flag
      unilink_spi_mode(mode_rx);                                // Done
    }
  }
  else if (unilink.mode==mode_tx && (unilink.SPI->Instance->CR2 & SPI_CR2_TXEIE) && (unilink.SPI->Instance->SR & SPI_SR_TXE)){ // If in transmit mode
    if(unilink.txCount==1){                               // First interrupt is only caused by the empty buffer, second byte happens when the first byte was sent
      if(slaveBreak.msg_state==break_msg_pendingTx){             // Was this the first byte for a slave poll answer?
        slaveBreak.msg_state=break_msg_sending;                  // OK, we are sending a slave response
      }
    }
    if(unilink.txCount<unilink.txSize){                   // check if bytes left
#ifdef Unilink_use_npn_output
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = unilink.txData[unilink.txCount];   // output next byte (inverted)
#else
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = ~unilink.txData[unilink.txCount];   // output next byte (inverted)
#endif
    }
    else{
#ifdef Unilink_use_npn_output
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = 0;   // Clock extra bytes as 0s
#else
      *(__IO uint8_t *)&unilink.SPI->Instance->DR = 0xFF;   // Clock extra bytes as 0s
#endif
    }
    if(unilink.txCount < unilink.txSize){
      unilink.txCount++;
    }
    else if(unilink.txCount == unilink.txSize){             // Last byte sent
      unilink.txCount++;                                    // Done. Increase txCount so we don't get there again in the following extra clocks sent by the master
      #ifdef Unilink_Log_Enable                                       // When master stops sending clocks, the byte timeout will reset back to Rx mode.
      unilinkLogUpdate(mode_tx);
      #endif
      if(slaveBreak.msg_state==break_msg_sending){               // Were we sending a slave response?
        #ifdef Unilink_Log_Enable
        unilink.logBreak=1;
        #endif
        slaveBreak.msg_state=break_msg_idle;                     // Done
        if(slaveBreak.pending){                           // We should have something here
          slaveBreak.pending--;                           // Decrease
          if(++slaveBreak.SendPos>(BrkSiz-1)){;           // Increase sendPos
            slaveBreak.SendPos=0;
          }
        }
      }
    }
  }
}

