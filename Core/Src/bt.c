/*
 * bt.c
 *
 *  Created on: Dec 25, 2024
 *      Author: David
 */

#include "bt.h"
#include "unilink.h"

#if defined BT_SUPPORT
bt_t bt;
void BT_decode_led(void);
void BT_handle_state(void);
void BT_handle_buttons(void);
void BT_Stop_cmd(void);
void BT_Play_cmd(void);
void BT_Next_cmd(void);
void BT_Prev_cmd(void);

void BT_handle(void){
  BT_decode_led();
  BT_handle_state();
  BT_handle_buttons();
}

void BT_decode_led(void){                                             // Read leds,
  uint8_t led_now = ~(0xF8 | ((uint8_t)readPin(LED3_GPIO_Port, LED3_Pin)<<2) | ((uint8_t)readPin(LED2_GPIO_Port, LED2_Pin)<<1) | (uint8_t)readPin(LED1_GPIO_Port, LED1_Pin));
  uint32_t now = HAL_GetTick();

  if(led_now == bt.led.last){
    if( (led_now != bt.led.stable ) && (now > bt.led.time) ){
      bt.led.stable=led_now;
    }
  }
  else{
    bt.led.time = now+20;       // 20ms without changes to consider stable
  }
  bt.led.last = led_now;
}

void BT_handle_state(void){
  uint8_t bt_pwr=readPin(BT_PWR_GPIO_Port, BT_PWR_Pin);
  uint32_t now = HAL_GetTick();

  if(bt.pwr_status != bt_on && bt_pwr){
    bt.pwr_status = bt_on;
    bt.pwr_on_time = now+2000;
    bt.pwr_on_done=0;
  }
  else if(bt.pwr_status == bt_on && !bt_pwr){
    bt.pwr_status = bt_off;
    bt.pwr_on_done=0;
    setPinLow(BT_ON_GPIO_Port, BT_ON_Pin);
  }
  if(bt.pwr_status == bt_off && now>500){
    setPinHigh(BT_ON_GPIO_Port, BT_ON_Pin);
    bt.pwr_status = bt_waiting_on;
  }
  else if(bt.pwr_status == bt_waiting_on && bt.led.stable==3 && bt_pwr==1){
    setPinLow(BT_ON_GPIO_Port, BT_ON_Pin);
    bt.pwr_status = bt_on;
    bt.pwr_on_time = now+2000;
  }

  if(!bt.pwr_on_done && now>bt.pwr_on_time){      // one second for module init, otherwise buttons stop working
    bt.pwr_on_done=1;
  }

  if(bt.led.stable==0) bt.bt_status = bt_no_link;
  if(bt.led.stable==1) bt.bt_status = bt_stop;
  else if(bt.led.stable==2) bt.bt_status = bt_play;       // Get status based on the bluetooth led outputs

  if(!bt.pwr_on_done || bt.pwr_status != bt_on || bt.bt_status==bt_no_link) return;

  if(bt.set_mode<=bt_stop && bt.bt_status != bt_stop){
    bt.button.do_stop=1;
  }
  if(bt.set_mode==bt_play && bt.bt_status != bt_play){
    bt.button.do_play=1;
  }
}

void BT_handle_buttons(void){
  uint32_t now = HAL_GetTick();
  if(bt.button.off_time && now>bt.button.off_time){
    bt.button.off_time=0;
#ifdef BT_SKIP_FIX_TIME
    if(bt.button.do_stop_skip){                                     // We stopped before changing
      bt.button.do_stop_skip=0;
      bt.button.on_time = now+BT_btn_on_time;

      if(bt.button.do_next)                                         // Change
        setPinHigh(NEXT_GPIO_Port, NEXT_Pin);
      else if(bt.button.do_prev)
        setPinHigh(PREV_GPIO_Port, PREV_Pin);

      return;
    }
    else
#endif
    if (bt.button.do_next || bt.button.do_prev){             // Change done
      if(bt.button.repeat){                                       // Repeat?
        bt.button.on_time = now+BT_btn_on_time;
        bt.button.repeat--;
        if(bt.button.repeat_cmd==bt_next){                        // Keep skipping
          setPinHigh(NEXT_GPIO_Port, NEXT_Pin);
        }
        else if(bt.button.repeat_cmd==bt_prev){
          setPinHigh(PREV_GPIO_Port, PREV_Pin);
        }
        return;
      }
      else{
        bt.button.do_next=0;                                      // Skip done
        bt.button.do_prev=0;
#ifdef BT_SKIP_FIX_TIME
        bt.button.do_stop_afterskip=1;
        setPinHigh(STOP_GPIO_Port, STOP_Pin);
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
        bt.button.on_time = now+BT_btn_on_time;
        setPinHigh(PLAY_GPIO_Port, PLAY_Pin);
      }
      return;
    }
#endif
    bt.button.busy=0;
    bt.button.do_stop=0;
    bt.button.do_play=0;
  }
  else if(bt.button.on_time && now>bt.button.on_time){
    bt.button.on_time=0;
#ifdef BT_SKIP_FIX_TIME
    if(bt.button.do_stop_afterskip)
      bt.button.off_time = now+BT_SKIP_FIX_TIME;               // Wait before resuming playback for ICS to unmute
    else
#endif
      bt.button.off_time = now+BT_btn_off_time;

    setPinLow(PLAY_GPIO_Port, PLAY_Pin);
    setPinLow(STOP_GPIO_Port, STOP_Pin);
    setPinLow(NEXT_GPIO_Port, NEXT_Pin);
    setPinLow(PREV_GPIO_Port, PREV_Pin);
  }

  if(!bt.pwr_on_done || bt.button.busy || bt.bt_status == bt_no_link) return;
#ifdef BT_SKIP_FIX_TIME
  bt.button.do_stop_skip = (bt.button.do_next || bt.button.do_prev);       // Stop before changing track to avoid annoying issue with the ICS where the current song keeps playing for a moment
#else
  if(bt.button.do_next){
    setPinHigh(NEXT_GPIO_Port, NEXT_Pin);
    bt.button.on_time = now+BT_btn_on_time;
    bt.button.busy=1;
  }
  else if(bt.button.do_prev){
    setPinHigh(PREV_GPIO_Port, PREV_Pin);
    bt.button.on_time = now+BT_btn_on_time;
    bt.button.busy=1;
  }
  else
#endif
  if(bt.button.do_stop_skip || bt.button.do_stop){
    setPinHigh(STOP_GPIO_Port, STOP_Pin);
    bt.button.on_time = now+BT_btn_on_time;
    bt.button.busy=1;
  }
  else if (bt.button.do_play){
    setPinHigh(PLAY_GPIO_Port, PLAY_Pin);
    bt.button.on_time = now+BT_btn_on_time;
    bt.button.busy=1;
  }
}

void BT_Stop(void){
  bt.button.d=0;
  bt.button.repeat=0;
  bt.button.do_stop=1;
  bt.set_mode=bt_stop;
}

void BT_Play(void){
  bt.button.d=0;
  bt.button.repeat=0;
  bt.button.do_play=1;
  bt.set_mode=bt_play;
}

void BT_Next(void){
  if(bt.button.do_next && bt.button.repeat_cmd==bt_next){
    bt.button.repeat++;
  }
  else{
    bt.button.repeat=0;
  }
  bt.button.repeat_cmd=bt_next;
  bt.button.d=0;
  bt.button.do_next=1;
}

void BT_Prev(void){
  if(bt.button.do_prev && bt.button.repeat_cmd==bt_prev){
    bt.button.repeat++;
  }
  else{
    bt.button.repeat=0;
  }
  bt.button.repeat_cmd=bt_prev;
  bt.button.d=0;
  bt.button.do_prev=1;
}
#endif
