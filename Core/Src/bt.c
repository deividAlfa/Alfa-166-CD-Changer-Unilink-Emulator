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
    uint8_t bt_state_now = (ReadPin(BT_1V8)) | ((uint8_t)!ReadPin(BT_LED0)<<1) | ((uint8_t)!ReadPin(BT_LED1)<<2);   // Leds are active-low
    uint32_t now = HAL_GetTick();
    if (bt_state_now == BTStruct.readState.last) {
        if ((bt_state_now != BTStruct.readState.stable) && (now > BTStruct.readState.time))
            BTStruct.readState.stable = bt_state_now;
    }
    else
        BTStruct.readState.time = now + _BT_DEBOUNCE_TIME;                // 20ms without changes to consider stable
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
        if((BTStruct.readState.stable & bt_on)==bt_off)
            BTStruct.bt_status = bt_off;
        else if((BTStruct.readState.stable & bt_mask)==bt_on)
            BTStruct.bt_status = bt_on;
        else if((BTStruct.readState.stable & bt_mask)==bt_linked)
            BTStruct.bt_status = bt_linked;
        else if((BTStruct.readState.stable & bt_mask)==bt_streaming)
            BTStruct.bt_status = bt_streaming;
    }

    if (BTStruct.bt_status!=bt_linked && BTStruct.bt_status!=bt_streaming)
        return;

    if((getAudioSource() != src_bt || unilink_status() != unilink_playing) && BTStruct.set_mode != bt_stop)       // If BT unselected or unilink stopped, stop BT playback
        BTStruct.set_mode = bt_stop;

    else if(getAudioSource() == src_bt && unilink_status() == unilink_playing && BTStruct.set_mode != bt_play)    // If BT selected and unilink playing, start BT playback
        BTStruct.set_mode = bt_play;

    if(!BTStruct.button.busy){
        if (!BTStruct.button.do_stop && BTStruct.set_mode <= bt_stop && BTStruct.bt_status == bt_streaming)       // Set mode or unilink=stop, BT playing, force stop
            BTStruct.button.do_stop = 1;

        if (!BTStruct.button.do_play && BTStruct.set_mode == bt_play && BTStruct.bt_status != bt_streaming)       // Set mode and unilink=play, BT stopped, force play
            BTStruct.button.do_play = 1;
    }
}

void BT_handle_buttons(void) {
    uint32_t now = HAL_GetTick();
    if (BTStruct.button.off_time){
        if( (BTStruct.button.do_stop && (BTStruct.button.do_next || BTStruct.button.do_prev) && unilink_status() != unilink_playing) ||     // Stop beforer skipping, wait until unilink resumes playback to skip tracks
            ((now - BTStruct.button.off_time)< _BT_OFF_TIME))                                                                               // Button OFF time not done, return
            return;

        BTStruct.button.off_time = 0;
        if(BTStruct.bt_status!=bt_linked)
            BTStruct.button.flags = 0;

        if ( (BTStruct.button.repeat  || BTStruct.button.do_stop) &&                            // Next/prev after stop, or repeat pending
             (BTStruct.button.do_next || BTStruct.button.do_prev)) {

            if(BTStruct.button.do_stop)
                BTStruct.button.do_stop = 0;                                                    // Skip pending after stop
            else
                BTStruct.button.repeat--;                                                       // Repeat pending

            BTStruct.button.on_time = now;
            if (BTStruct.button.do_next)                                                        // Execute pending skip
                SetPinHigh(BT_NEXT);
            else if (BTStruct.button.do_prev)
                SetPinHigh(BT_PREV);
            return;
        }
        BTStruct.button.flags = 0;                                                              // Nothing left
    }
    else if (BTStruct.button.on_time){
        if((now-BTStruct.button.on_time)<_BT_ON_TIME)                                           // Button ON time not done, return
            return;
        BTStruct.button.on_time = 0;                                                            // Proceed with OFF time
        BTStruct.button.off_time = now;
        if(  BTStruct.button.do_play ||                                                         // Play: ON time done, allow play
            (!BTStruct.button.do_stop && !BTStruct.button.repeat && (BTStruct.button.do_next || BTStruct.button.do_prev)) ){ // Skip done, no repeats pending, allow play

            BTStruct.button.allow_play = 1;
        }

        SetPinLow(BT_PLAY);                                                                     // Release all keys
        SetPinLow(BT_STOP);
        SetPinLow(BT_NEXT);
        SetPinLow(BT_PREV);
    }

    if (BTStruct.button.busy || (BTStruct.readState.stable & bt_mask)<bt_linked)                // Button busy or not linked, return
        return;

    if (BTStruct.button.do_stop) {                                                              // Pending STOP
        SetPinHigh(BT_STOP);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
        BTStruct.button.allow_play = 0;
    }
    else if (BTStruct.button.do_play) {                                                         // Pending PLAY
        SetPinHigh(BT_PLAY);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
        BTStruct.button.allow_play = 0;
    }
    else if (BTStruct.button.do_next) {                                                         // Pending NEXT
        SetPinHigh(BT_NEXT);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
        BTStruct.button.allow_play = 0;
    }
    else if (BTStruct.button.do_prev) {                                                         // Pending PREV
        SetPinHigh(BT_PREV);
        BTStruct.button.on_time = now;
        BTStruct.button.busy = 1;
        BTStruct.button.allow_play = 0;
    }
}
#endif

void BT_Stop(void) {
#if defined BT_SUPPORT
    if(BTStruct.bt_status!=bt_linked) return;
    BTStruct.button.flags = 0;
    BTStruct.button.repeat = 0;
    BTStruct.button.do_stop = 1;
    BTStruct.set_mode = bt_stop;
    putString("BT_STOP\r\n");
#endif
}

void BT_Play(void) {
#if defined BT_SUPPORT
    if(BTStruct.bt_status!=bt_linked) return;
    BTStruct.button.flags = 0;
    BTStruct.button.repeat = 0;
    BTStruct.button.do_play = 1;
    BTStruct.set_mode = bt_play;
    putString("BT_PLAY\r\n");
#endif
}

void BT_Next(void) {
#if defined BT_SUPPORT
    if(BTStruct.bt_status!=bt_linked) return;
    if (BTStruct.button.do_next) {
        BTStruct.button.repeat++;
        putString("BT_NEXT(R)\r\n");
    }
    else {
        BTStruct.button.flags = 0;
        BTStruct.button.repeat = 0;
        BTStruct.button.do_stop = 1;            // Stop playback before changing (To avoid unwanted sound when the ICS unmutes)
        BTStruct.button.do_next = 1;
        putString("BT_NEXT\r\n");
    }
#endif
}

void BT_Prev(void) {
#if defined BT_SUPPORT
    if(BTStruct.bt_status!=bt_linked) return;
    if (BTStruct.button.do_prev) {
        BTStruct.button.repeat++;
        putString("BT_PREV(R)\r\n");
    }
    else {
        BTStruct.button.flags = 0;
        BTStruct.button.repeat = 0;
        BTStruct.button.do_stop = 1;            // Stop playback before changing
        BTStruct.button.do_prev = 1;
        putString("BT_PREV\r\n");
    }
#endif
}

uint8_t BT_Allow_Play(void){
    return BTStruct.button.allow_play;
}

uint8_t BT_Busy(void){
    return BTStruct.button.busy;
}

