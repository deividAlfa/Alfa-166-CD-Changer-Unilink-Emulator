/*
 * bt.h
 *
 *  Created on: Dec 25, 2024
 *      Author: David
 */

#ifndef INC_BT_H_
#define INC_BT_H_


#include "main.h"

#define BT_btn_on_time    150   // In ms
#define BT_btn_off_time   500
//#define BT_SKIP_FIX_TIME   2000


typedef enum{
  bt_off=0,
  bt_waiting_on,
  bt_on,
  bt_no_link=0,
  bt_stop=1,
  bt_play=2,
  bt_next=3,
  bt_prev=4,
}bt_state_t;

typedef struct{
  uint8_t stable;
  uint8_t last;
  uint32_t time;
}bt_led_t;

typedef struct{
  union{
    uint8_t d;
    struct{
      unsigned do_play      :1;
      unsigned do_stop      :1;
      unsigned do_stop_skip :1;
      unsigned do_next      :1;
      unsigned do_prev      :1;
      unsigned do_stop_afterskip :1;
      unsigned busy         :1;
    };
  };
  uint8_t repeat;
  uint8_t repeat_cmd;
  uint32_t on_time;
  uint32_t off_time;
}bt_button_t;

typedef struct{
  bt_state_t pwr_status;
  uint8_t    pwr_on_done;
  bt_state_t bt_status;
  bt_state_t set_mode;
  bt_led_t led;
  bt_button_t button;
  uint32_t pwr_on_time;
}bt_t;



void BT_handle(void);
void BT_ON(void);
void BT_Stop(void);
void BT_Play(void);
void BT_Next(void);
void BT_Prev(void);

#endif /* INC_BT_H_ */
