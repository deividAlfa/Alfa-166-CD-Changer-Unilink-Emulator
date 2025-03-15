/*
 * bt.c
 *
 *  Created on: Dec 25, 2024
 *      Author: David
 */

#include "bt.h"
#include "unilink.h"
#include "audioSrc.h"
#include "serial.h"

#if defined BT_SUPPORT
volatile bt_t BTStruct;
void BT_decode_status(void);
void BT_handle_state(void);
void BT_handle_buttons(void);
void BT_Stop_cmd(void);
void BT_Play_cmd(void);
void BT_Next_cmd(void);
void BT_Prev_cmd(void);

void BT_handle(void) {
    BT_decode_status();
    BT_handle_state();
    BT_handle_buttons();
}

void BT_decode_status(void) {                                         // Read BT module outputs
    uint8_t bt_state_now = ~(0xF8 |
        ((uint8_t) !ReadPin(BT_1V8)  <<0)     |
        ((uint8_t) ReadPin(BT_LED0) <<1)     |
        ((uint8_t) ReadPin(BT_LED1) <<2)     );

    uint32_t now = HAL_GetTick();

    if (bt_state_now == BTStruct.readState.last) {
        if ((bt_state_now != BTStruct.readState.stable) && (now > BTStruct.readState.time)) {
            BTStruct.readState.stable = bt_state_now;
        }
    }
    else {
        BTStruct.readState.time = now + _BT_DEBOUNCE_TIME;                // 20ms without changes to consider stable
    }
    BTStruct.readState.last = bt_state_now;
}

void BT_handle_state(void) {

    if (BTStruct.bt_status == bt_off){
        SetPinOtype(BT_STOP, OUTPUT_OD);
        SetPinOtype(BT_PLAY, OUTPUT_OD);
        SetPinOtype(BT_NEXT, OUTPUT_OD);
        SetPinOtype(BT_PREV, OUTPUT_OD);
        SetPinHigh(BT_STOP);
        SetPinHigh(BT_PLAY);
        SetPinHigh(BT_NEXT);
        SetPinHigh(BT_PREV);
        SetPinHigh(BT_ON);
        BTStruct.bt_status = bt_wait_on;
    }
    else if(BTStruct.bt_status == bt_wait_on){
        if(BTStruct.readState.stable & bt_on){
            BTStruct.pwr_on_time = HAL_GetTick();
            BTStruct.bt_status = bt_on_delay;
        }
    }
    else if(BTStruct.bt_status == bt_on_delay){
        if(BTStruct.readState.stable & bt_on && (HAL_GetTick()-BTStruct.pwr_on_time>1000)){
            SetPinLow(BT_STOP);
            SetPinLow(BT_PLAY);
            SetPinLow(BT_NEXT);
            SetPinLow(BT_PREV);
            SetPinOtype(BT_STOP, OUTPUT_PP);
            SetPinOtype(BT_PLAY, OUTPUT_PP);
            SetPinOtype(BT_NEXT, OUTPUT_PP);
            SetPinOtype(BT_PREV, OUTPUT_PP);
            BTStruct.bt_status = bt_on;
        }
    }
    else if(BTStruct.bt_status & bt_on){
        if((BTStruct.readState.stable & bt_on)==bt_off){
            BTStruct.bt_status = bt_off;
        }
        else if((BTStruct.readState.stable & bt_mask)==bt_on){
            BTStruct.bt_status = bt_on;
        }
        else if((BTStruct.readState.stable & bt_mask)==bt_linked){
            BTStruct.bt_status = bt_linked;
        }
        else if((BTStruct.readState.stable & bt_mask)==bt_streaming){
            BTStruct.bt_status = bt_streaming;
        }
    }

    if (BTStruct.bt_status!=bt_linked && BTStruct.bt_status!=bt_streaming)
        return;

    if(getAudioSource() != src_bt && BTStruct.set_mode != bt_stop)       // If BT unselected, stop BT playback
        BTStruct.set_mode = bt_stop;

    if(!BTStruct.button.busy){
        if ((BTStruct.set_mode <= bt_stop || unilink_status() != unilink_playing) &&      // Set mode or unilink=stop, BT playing, force stop
            BTStruct.bt_status == bt_streaming && !BTStruct.button.do_stop)

            BTStruct.button.do_stop = 1;

        if ((BTStruct.set_mode == bt_play && unilink_status() == unilink_playing) &&      // Set mode or unilink=play, BT stopped, force play
            BTStruct.bt_status != bt_streaming && !BTStruct.button.do_play)

            BTStruct.button.do_play = 1;
    }
}

void BT_handle_buttons(void) {
    uint32_t now = HAL_GetTick();
    if (BTStruct.button.off_time && ((now - BTStruct.button.off_time)>_BT_OFF_TIME)) {
        BTStruct.button.off_time = 0;
#ifdef BT_SKIP_FIX_TIME
    if(BTStruct.button.do_stop_skip){                                     // We stopped before changing
      BTStruct.button.do_stop_skip=0;
      BTStruct.button.on_time = now+_BT_ON_TIME;

      if(BTStruct.button.do_next)                                         // Change
        SetPinHigh(BT_NEXT);
      else if(BTStruct.button.do_prev)
        SetPinHigh(BT_PREV);

      return;
    }
    else
#endif
        if (BTStruct.button.do_next || BTStruct.button.do_prev) {                // Change done
            if (BTStruct.button.repeat) {                                   // Repeat?
                BTStruct.button.on_time = now;
                BTStruct.button.repeat--;
                if (BTStruct.button.repeat_cmd == bt_next) {                // Keep skipping
                    SetPinHigh(BT_NEXT);
                }
                else if (BTStruct.button.repeat_cmd == bt_prev) {
                    SetPinHigh(BT_PREV);
                }
                return;
            }
            else {
                BTStruct.button.do_next = 0;                              // Skip done
                BTStruct.button.do_prev = 0;
#ifdef BT_SKIP_FIX_TIME
        BTStruct.button.do_stop_afterskip=1;
        SetPinHigh(BT_STOP);
#endif
            }
#ifdef BT_SKIP_FIX_TIME
      return;
#endif
        }
#ifdef BT_SKIP_FIX_TIME
    else if(BTStruct.button.do_stop_afterskip){
      BTStruct.button.do_stop_afterskip=0;
      if(BTStruct.set_mode==bt_play){                                    // Resume after skipping
        BTStruct.button.on_time = now;
        SetPinHigh(BT_PLAY);
      }
      return;
    }
#endif
        BTStruct.button.busy = 0;
        BTStruct.button.do_stop = 0;
        BTStruct.button.do_play = 0;
    }
    else if (BTStruct.button.on_time && ((now-BTStruct.button.on_time)>_BT_ON_TIME)) {
        BTStruct.button.on_time = 0;
#ifdef BT_SKIP_FIX_TIME
    if(BTStruct.button.do_stop_afterskip)
      BTStruct.button.off_time = now;               // Wait before resuming playback for ICS to unmute
    else
#endif
        BTStruct.button.off_time = now;

        SetPinLow(BT_PLAY);
        SetPinLow(BT_STOP);
        SetPinLow(BT_NEXT);
        SetPinLow(BT_PREV);
    }

    if (BTStruct.button.busy || (BTStruct.readState.stable & bt_mask)<bt_linked)
        return;
#ifdef BT_SKIP_FIX_TIME
  BTStruct.button.do_stop_skip = (BTStruct.button.do_next || BTStruct.button.do_prev);       // Stop before changing track to avoid annoying issue with the ICS where the current song keeps playing for a moment
#else
    if (BTStruct.button.do_next) {
        SetPinHigh(BT_NEXT);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
    }
    else if (BTStruct.button.do_prev) {
        SetPinHigh(BT_PREV);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
    }
    else
#endif
    if (BTStruct.button.do_stop_skip || BTStruct.button.do_stop) {
        SetPinHigh(BT_STOP);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
    }
    else if (BTStruct.button.do_play) {
        SetPinHigh(BT_PLAY);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
    }
}
#endif

void BT_Stop(void) {
#if defined BT_SUPPORT
    BTStruct.button.flags = 0;
    BTStruct.button.repeat = 0;
    BTStruct.button.do_stop = 1;
    BTStruct.set_mode = bt_stop;
    putString("BT_STOP\r\n");
#endif
}

void BT_Play(void) {
#if defined BT_SUPPORT
    BTStruct.button.flags = 0;
    BTStruct.button.repeat = 0;
    BTStruct.button.do_play = 1;
    BTStruct.set_mode = bt_play;
    putString("BT_PLAY\r\n");
#endif
}

void BT_Next(void) {
#if defined BT_SUPPORT
    if (BTStruct.button.do_next && BTStruct.button.repeat_cmd == bt_next) {
        BTStruct.button.repeat++;
        putString("BT_NEXT(R)\r\n");
    }
    else {
        BTStruct.button.repeat = 0;
        putString("BT_NEXT\r\n");
    }
    BTStruct.button.repeat_cmd = bt_next;
    BTStruct.button.flags = 0;
    BTStruct.button.do_next = 1;
#endif
}

void BT_Prev(void) {
#if defined BT_SUPPORT
    if (BTStruct.button.do_prev && BTStruct.button.repeat_cmd == bt_prev) {
        BTStruct.button.repeat++;
        putString("BT_PREV(R)\r\n");
    }
    else {
        BTStruct.button.repeat = 0;
        putString("BT_PREV\r\n");
    }
    BTStruct.button.repeat_cmd = bt_prev;
    BTStruct.button.flags = 0;
    BTStruct.button.do_prev = 1;
#endif
}

