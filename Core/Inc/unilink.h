/*
 * unilink.h
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

#ifndef __UNILINK_H
#define __UNILINK_H

#include "main.h"

#define SpiClock()          readPin(UNILINK_SCK_GPIO_Port, UNILINK_SCK_Pin)
#define isSpiClockActive()  !readPin(UNILINK_SCK_GPIO_Port, UNILINK_SCK_Pin)
#define isSpiClockIdle()    readPin(UNILINK_SCK_GPIO_Port, UNILINK_SCK_Pin)
#define isDataHigh()        readPin(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin)
#define isDataLow()         !readPin(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin)
#define setDataHigh()       setPinHigh(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin)
#define setDataLow()        setPinLow(UNILINK_DATA_GPIO_Port, UNILINK_DATA_Pin)



typedef enum {
  mode_tx           = 0,
  mode_rx           = 1,
}unilink_SPImode_t;

typedef enum {
  mode_SPI          = 0,
  mode_input        = 1,
  mode_output       = 2,
}unilink_DATAmode_t;

typedef enum{
  unilink_playing   = 0x00,
  unilink_changed   = 0x20,
  unilink_seeking   = 0x21,
  unilink_changing  = 0x40,
  unilink_idle      = 0x80,
  unilink_ejecting  = 0xC0,
  unilink_unknown   = 0xFF,
}unilinkStatus_t;

typedef enum{
  unilink_short     = 6,
  unilink_medium    = 11,
  unilink_long      = 16,
}Unilink_size_t;

typedef enum{
  mag_slot_empty    = 0x40,
  mag_inserted      = 0x80,
  mag_removed       = 0xC0,
}magInfo_t;

typedef enum{
  break_msg_idle        = 0,
  break_msg_pending     = 1,
  break_msg_pendingTx   = 2,
  break_msg_sending     = 3
}breakMsgState_t;

typedef enum{
  break_wait_data_low       = 0,
  break_wait_data_low_time  = 1,
  break_wait_data_high      = 2,
  break_wait_data_high_time = 3,
  break_wait_data_setlow    = 4,
  break_wait_data_keeplow   = 5,
  break_wait_data_low_err
}breakState_t;

typedef enum{
  dst_addr          = 0,
  src_addr          = 1,
  cmd1              = 2,
  cmd2              = 3,
  parity1           = 4,
  d1                = 5,
  d2                = 6,
  d3                = 7,
  d4                = 8,
  parity2           = 9,
  d2_1              = 9,
  d2_2              = 10,
  d2_3              = 11,
  d2_4              = 12,
  d2_5              = 13,
  parity2_L         = 14,
}unilinkFrame_t;

typedef enum{
  cmd_busReset      = 0x00,
  cmd_pwroff        = 0x00,
  cmd_status        = 0x00,
  cmd_busRequest    = 0x01,
  cmd_anyone        = 0x02,
  cmd_cfgfrommaster = 0x02,
  cmd_anyoneSpecial = 0x03,
  cmd_appointEnd    = 0x04,
  cmd_keyoff        = 0x08,
  cmd_timePollEnd   = 0x11,
  cmd_timePoll      = 0x12,
  cmd_slavePoll     = 0x13,
  cmd_play          = 0x20,
  cmd_switch        = 0x21,
  cmd_fastFwd       = 0x24,
  cmd_fastRwd       = 0x25,
  cmd_nextTrack     = 0x26,
  cmd_prevTrack     = 0x27,
  cmd_nextCD        = 0x28,
  cmd_prevCD        = 0x29,
  cmd_repeat        = 0x34,
  cmd_shuffle       = 0x35,
  cmd_intro         = 0x36,
  cmd_textRequest   = 0x84,
  cmd_power         = 0x87,
  cmd_pwron         = 0x89,
  cmd_anyoneResp    = 0x8C,
  cmd_cartridgeinfo = 0x8e,
  cmd_time          = 0x90,
  cmd_cfgchange     = 0x94,
  cmd_magazine      = 0x95,
  cmd_intro_end     = 0x98,
  cmd_dspdiscchange = 0x9c,
  cmd_discinfo      = 0x97,
  cmd_goto          = 0xB0,
  cmd_seek          = 0xC0,
  cmd_discname      = 0xCD,
  cmd_source        = 0xF0,
  cmd_raw           = 0xFE,
  cmd_unknown       = 0xFF,
}unilinkCmd_t;

typedef enum{
  disc1_size        = 12,
  disc2_size        = 24,
  disc3_size        = 48,
  disc4_size        = 64,
  disc5_size        = 72,
  disc6_size        = 96,
}discSize_t;


typedef enum {
  addr_master        = 0x10,
  addr_broadcast     = 0x18,
  addr_display       = 0x70,
  addr_display2      = 0x77,
  addr_dsp           = 0x90,
  addr_reset         = 0x30,
}unilinkAddress_t;



typedef struct {
  volatile bool             logRx;            // Indicates rx buffer ready for logging
  volatile bool             logTx;            // Indicates tx buffer ready for logging
  volatile bool             logBreak;         // Indicates tx buffer was a made by a slave break
  volatile bool             play;             // Stores play status from master
  volatile bool             powered_on;       // Stores play status from master when receiving power command
  volatile bool             trackChanged;     // Flag, set if disc/track needs to be changed
  volatile bool             mag_changed;      // Flag, set if usb was changed
  volatile bool             received;         // Flag, set when packet received
  volatile bool             bad_checksum;     // Flag, bad packet received, force byte timeout to clean clocks
  volatile bool             appoint;          // Flag, set if we did appoint
  volatile bool             busReset;         // Flag, set if master sent the bus reset
  volatile bool             hwinit;           // Flag, set if unilink hw is initialized
  volatile bool             masterinit;       // Flag, set if unilink was initialized by master
  volatile bool             mode;             // SPI transfer mode (1=rx, 0=tx)
  volatile uint8_t          status;           // Stores unilink status
  volatile uint8_t          statusTimer;      // Stores unilink status timer
  volatile uint8_t          ownAddr;          // Stores device address
  volatile uint8_t          groupID;          // Stores group ID
  volatile uint8_t          disc;             // Stores current disc
  volatile uint8_t          lastDisc;         // Stores previous disc after a disc change
  volatile uint8_t          track;            // Stores current track
  volatile uint8_t          lastTrack;        // Stores previous track after a track change
  volatile uint8_t          min;              // Stores current minute
  volatile uint8_t          sec;              // Stores current second
  volatile uint8_t          rxCount;          // Counter for received bytes
  volatile uint8_t          txCount;          // Counter for transmitted bytes
  volatile uint8_t          rxSize;           // Stores expected rx packet size
  volatile uint8_t          txSize;           // Stores expected tx packet size
  volatile uint8_t          logSize;
  volatile uint8_t          rxData[parity2_L+1];       // Stores received packet
  volatile uint8_t          txData[parity2_L  +1];       // Stores packet to be transmitted
  volatile uint8_t          logData[parity2_L+1]; // Stores log packet
  volatile uint16_t         millis;           // millisecond timer, for generating the playback time
  volatile uint16_t         timeout;          // Timeout counter for detecting bus stall

  TIM_HandleTypeDef*        timer;            // Stores the address of the clock timer handler (for clock timeout)
  SPI_HandleTypeDef*        SPI;              // Stores the address of the SPI handler
}unilink_t;


#define BrkSiz              16
typedef struct{
  volatile uint8_t          dataTime;         // For measuring data high/low states
  volatile uint16_t         lost;             // Counter for discarded slavebreaks if the queue was full
  volatile uint8_t          pending;          // Counter of slavebreaks pending to be send
  volatile uint8_t          BfPos;            // Input buffer position
  volatile uint8_t          SendPos;          // Sending buffer position
  uint8_t                   data[BrkSiz][parity2_L+3]; // tx buffer queue data. Byte 16 is tx size
  volatile uint8_t          lastAutoCmd;      // last self-generated break command (when no requested from master)
  volatile breakMsgState_t  msg_state;        // status for msg  sending
  breakState_t              break_state;      // status for generating break condition
}slaveBreak_t;

typedef struct{
  volatile uint8_t          status;           // Stores magazine status
  union{
    volatile uint8_t        cmd2;
    struct{
      volatile unsigned     CD5_present:1;
      volatile unsigned     CD6_present:1;
      volatile unsigned     unused1:1;
      volatile unsigned     unused2:1;
      volatile unsigned     CD1_present:1;
      volatile unsigned     CD2_present:1;
      volatile unsigned     CD3_present:1;
      volatile unsigned     CD4_present:1;
    };
  };
}magazine_t;

typedef struct{
  volatile bool     inserted;
  volatile uint8_t  tracks;
  volatile uint8_t  mins;
  volatile uint8_t  secs;
}cdinfo_t;


extern unilink_t    unilink;
extern slaveBreak_t slaveBreak;
extern uint8_t      raw[16];
extern magazine_t   mag_data;
extern cdinfo_t     cd_data[6];

                              // Checksums not included - computed later.
                              //    RAD           TAD           CMD1      CMD2    D1  D2  D3  D4
#define   msg_seek             { addr_display, unilink.ownAddr, cmd_seek, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x08 }
#define   msg_time             { addr_display, unilink.ownAddr, cmd_time, 0x00, hex2bcd(unilink.track), hex2bcd(unilink.min), hex2bcd(unilink.sec), ((unilink.disc<<4)|0xA) }
#define   msg_magazine         { addr_master,  unilink.ownAddr, cmd_magazine, mag_data.cmd2 ,0x00 ,0x00 ,0x00 , 0x06 }
#define   msg_mag_slot_empty   { addr_master,  unilink.ownAddr, cmd_cartridgeinfo, mag_slot_empty, 0x20 ,0x00 ,0x00, (unilink.disc<<4) }
#define   msg_cartridge_info   { addr_master,  unilink.ownAddr, cmd_cartridgeinfo, mag_data.status, 0x20 ,0x00 ,0x00, (unilink.disc<<4) }                       //  XXX: Unused?
#define   msg_dspchange        { addr_dsp,     unilink.ownAddr, cmd_dspdiscchange, 0x00, 0x00, 0x00, 0x00, ((unilink.disc<<4)|0x8) }
#define   msg_intro_end        { addr_master,  unilink.ownAddr, cmd_intro_end, 0x10, 0x00, 0x00, 0x01, (unilink.disc<<4) }
#define   msg_discinfo_empty   { addr_master,  unilink.ownAddr, cmd_discinfo, 0x01, 0x99, 0x00, 0x00, 0x01 }
#define   msg_discinfo         { addr_master,  unilink.ownAddr, cmd_discinfo, 0x01, hex2bcd(cd_data[unilink.disc-1].tracks), hex2bcd(cd_data[unilink.disc-1].mins), hex2bcd(cd_data[unilink.disc-1].secs), (unilink.disc<<4) }
#define   msg_status           { addr_master,  unilink.ownAddr, cmd_status, unilink.status }
#define   msg_cfgchange        { addr_display, unilink.ownAddr, cmd_cfgchange, 0x20, 0x00,0x00,0x00,0x00 }
#define   msg_anyoneResp       { addr_master,  unilink.ownAddr, cmd_anyoneResp, 0x11, 0x14, 0xA8, 0x17, 0x60 }        // Becker 2660AR ID (Alfa 166)
#define   msg_anyoneResp_alt   { addr_master,  unilink.ownAddr, cmd_anyoneResp, 0x11, 0x15, 0xA8, 0x17, 0x60 }        // Another ID, not used


void unilink_init(SPI_HandleTypeDef* SPI, TIM_HandleTypeDef* tim);
void unilink_handle(void);
void unilink_tick(void);
void unilink_callback(void);
void unilink_byte_timeout(void);
void unilink_reset_playback_time(void);


#endif
