/*
 * bt.c
 *
 *  Created on: Dec 25, 2024
 *      Author: David
 */

#include "bt.h"
#include "unilink.h"
#include "audioSrc.h"

#if defined BT_SUPPORT
volatile bt_t bt;
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

    if (bt_state_now == bt.readState.last) {
        if ((bt_state_now != bt.readState.stable) && (now > bt.readState.time)) {
            bt.readState.stable = bt_state_now;
        }
    }
    else {
        bt.readState.time = now + _BT_DEBOUNCE_TIME;                // 20ms without changes to consider stable
    }
    bt.readState.last = bt_state_now;
}

void BT_handle_state(void) {

    if (bt.bt_status == bt_off){
        SetPinOtype(BT_STOP, OUTPUT_OD);
        SetPinOtype(BT_PLAY, OUTPUT_OD);
        SetPinOtype(BT_NEXT, OUTPUT_OD);
        SetPinOtype(BT_PREV, OUTPUT_OD);
        SetPinHigh(BT_STOP);
        SetPinHigh(BT_PLAY);
        SetPinHigh(BT_NEXT);
        SetPinHigh(BT_PREV);
        SetPinHigh(BT_ON);
        bt.bt_status = bt_wait_on;
    }
    else if(bt.bt_status == bt_wait_on){
        if(bt.readState.stable & bt_on){
            bt.pwr_on_time = HAL_GetTick();
            bt.bt_status = bt_on_delay;
        }
    }
    else if(bt.bt_status == bt_on_delay){
        if(bt.readState.stable & bt_on && (HAL_GetTick()-bt.pwr_on_time>1000)){
            SetPinLow(BT_STOP);
            SetPinLow(BT_PLAY);
            SetPinLow(BT_NEXT);
            SetPinLow(BT_PREV);
            SetPinOtype(BT_STOP, OUTPUT_PP);
            SetPinOtype(BT_PLAY, OUTPUT_PP);
            SetPinOtype(BT_NEXT, OUTPUT_PP);
            SetPinOtype(BT_PREV, OUTPUT_PP);
            bt.bt_status = bt_on;
        }
    }
    else if(bt.bt_status & bt_on){
        if((bt.readState.stable & bt_on)==bt_off){
            bt.bt_status = bt_off;
        }
        else if((bt.readState.stable & bt_mask)==bt_on){
            bt.bt_status = bt_on;
        }
        else if((bt.readState.stable & bt_mask)==bt_linked){
            bt.bt_status = bt_linked;
        }
        else if((bt.readState.stable & bt_mask)==bt_streaming){
            bt.bt_status = bt_streaming;
        }
    }

    if (bt.bt_status!=bt_linked && bt.bt_status!=bt_streaming)
        return;

    if(getAudioSource() != src_bt && bt.set_mode != bt_stop)       // If BT unselected, stop BT playback
        bt.set_mode = bt_stop;

    if(!bt.button.busy){
        if (bt.set_mode <= bt_stop && bt.bt_status == bt_streaming && !bt.button.do_stop)
            bt.button.do_stop = 1;
        if (bt.set_mode == bt_play && bt.bt_status != bt_streaming && !bt.button.do_play)
            bt.button.do_play = 1;
    }
}

void BT_handle_buttons(void) {
    uint32_t now = HAL_GetTick();
    if (bt.button.off_time && ((now - bt.button.off_time)>_BT_OFF_TIME)) {
        bt.button.off_time = 0;
#ifdef BT_SKIP_FIX_TIME
    if(bt.button.do_stop_skip){                                     // We stopped before changing
      bt.button.do_stop_skip=0;
      bt.button.on_time = now+_BT_ON_TIME;

      if(bt.button.do_next)                                         // Change
        SetPinHigh(BT_NEXT);
      else if(bt.button.do_prev)
        SetPinHigh(BT_PREV);

      return;
    }
    else
#endif
        if (bt.button.do_next || bt.button.do_prev) {                // Change done
            if (bt.button.repeat) {                                   // Repeat?
                bt.button.on_time = now;
                bt.button.repeat--;
                if (bt.button.repeat_cmd == bt_next) {                // Keep skipping
                    SetPinHigh(BT_NEXT);
                }
                else if (bt.button.repeat_cmd == bt_prev) {
                    SetPinHigh(BT_PREV);
                }
                return;
            }
            else {
                bt.button.do_next = 0;                              // Skip done
                bt.button.do_prev = 0;
#ifdef BT_SKIP_FIX_TIME
        bt.button.do_stop_afterskip=1;
        SetPinHigh(BT_STOP);
#endif
            }
#ifdef BT_SKIP_FIX_TIME
      return;
#endif
        }
#ifdef BT_SKIP_FIX_TIME
    else if(bt.button.do_stop_afterskip){
      bt.button.do_stop_afterskip=0;
      if(bt.set_mode==bt_play){                                    // Resume after skipping
        bt.button.on_time = now;
        SetPinHigh(BT_PLAY);
      }
      return;
    }
#endif
        bt.button.busy = 0;
        bt.button.do_stop = 0;
        bt.button.do_play = 0;
    }
    else if (bt.button.on_time && ((now-bt.button.on_time)>_BT_ON_TIME)) {
        bt.button.on_time = 0;
#ifdef BT_SKIP_FIX_TIME
    if(bt.button.do_stop_afterskip)
      bt.button.off_time = now;               // Wait before resuming playback for ICS to unmute
    else
#endif
        bt.button.off_time = now;

        SetPinLow(BT_PLAY);
        SetPinLow(BT_STOP);
        SetPinLow(BT_NEXT);
        SetPinLow(BT_PREV);
    }

    if (bt.button.busy || (bt.readState.stable & bt_mask)<bt_linked)
        return;
#ifdef BT_SKIP_FIX_TIME
  bt.button.do_stop_skip = (bt.button.do_next || bt.button.do_prev);       // Stop before changing track to avoid annoying issue with the ICS where the current song keeps playing for a moment
#else
    if (bt.button.do_next) {
        SetPinHigh(BT_NEXT);
        bt.button.on_time = now;
        bt.button.busy = 1;
    }
    else if (bt.button.do_prev) {
        SetPinHigh(BT_PREV);
        bt.button.on_time = now;
        bt.button.busy = 1;
    }
    else
#endif
    if (bt.button.do_stop_skip || bt.button.do_stop) {
        SetPinHigh(BT_STOP);
        bt.button.on_time = now;
        bt.button.busy = 1;
    }
    else if (bt.button.do_play) {
        SetPinHigh(BT_PLAY);
        bt.button.on_time = now;
        bt.button.busy = 1;
    }
}
#endif

void BT_Stop(void) {
#if defined BT_SUPPORT
    bt.button.flags = 0;
    bt.button.repeat = 0;
    bt.button.do_stop = 1;
    bt.set_mode = bt_stop;
    iprintf("BT_STOP\r\n");
#endif
}

void BT_Play(void) {
#if defined BT_SUPPORT
    bt.button.flags = 0;
    bt.button.repeat = 0;
    bt.button.do_play = 1;
    bt.set_mode = bt_play;
    iprintf("BT_PLAY\r\n");
#endif
}

void BT_Next(void) {
#if defined BT_SUPPORT
    if (bt.button.do_next && bt.button.repeat_cmd == bt_next) {
        bt.button.repeat++;
        iprintf("BT_NEXT(R)\r\n");
    }
    else {
        bt.button.repeat = 0;
        iprintf("BT_NEXT\r\n");
    }
    bt.button.repeat_cmd = bt_next;
    bt.button.flags = 0;
    bt.button.do_next = 1;
#endif
}

void BT_Prev(void) {
#if defined BT_SUPPORT
    if (bt.button.do_prev && bt.button.repeat_cmd == bt_prev) {
        bt.button.repeat++;
        iprintf("BT_PREV(R)\r\n");
    }
    else {
        bt.button.repeat = 0;
        iprintf("BT_PREV\r\n");
    }
    bt.button.repeat_cmd = bt_prev;
    bt.button.flags = 0;
    bt.button.do_prev = 1;
#endif
}

