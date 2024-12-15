/*
 * unilink_log.h
 *
 *  Created on: 15 dic. 2024
 *      Author: David
 */

#ifndef INC_UNILINK_LOG_H_
#define INC_UNILINK_LOG_H_


#include "unilink.h"
#include "main.h"

#define DebugLog                            // Enable debug output
#define OnlyLog                             // Only sniff the bus, passive operation
#define Detail_Log                        // Enable to append command description to each decoded frame
//#define EnableSerialLog                   // Print to the uart

#if defined (OnlyLog) && !defined (DebugLog)
#define DebugLog
#endif



#ifdef DebugLog



typedef enum{
  label_seek,
  label_time,
  label_magazine,
  label_discinfo,
  label_status,
  label_cfgchange,
  label_raw,
  label_playing,
  label_changed,
  label_seeking,
  label_changing,
  label_idle,
  label_ejecting,
  label_unknownStatus,
  label_unknownCmd,
  label_busReset,
  label_anyone,
  label_anyoneResp,
  label_cmd1,
  label_request1,
  label_power,
  label_setactive,
  label_setidle,
  label_srcselect,
  label_switch,
  label_play,
  label_cmd,
  label_anyoneNotOK,
  label_timePoll,
  label_slavePoll,
  label_Poll_,
  label_fastFwd,
  label_fastRwd,
  label_repeat,
  label_shuffle,
  label_off,
  label_on,
  label_change,
  label_appoint,
  label_appointEnd,
  label_dspdiscchange,
  label_intro,
  label_intro_end,
  label_cartridge_info,
  label_mag_removed,
  label_mag_slotempty,
  label_mag_inserted,
}labelIndex_t;




void unilinkLogUpdate(unilink_mode_t mode);
void unilinkLog(void);
#endif
char hex2ascii(uint8_t hex);
uint8_t hex2bcd(uint8_t hex);           // convert 1 byte BCD to 1 byte HEX
uint8_t bcd2hex(uint8_t bcd);           // convert 1 byte BCD to 1 byte HEX

#endif /* INC_UNILINK_LOG_H_ */
