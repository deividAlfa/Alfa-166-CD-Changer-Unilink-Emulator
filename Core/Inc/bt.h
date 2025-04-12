/*
 * bt.h
 *
 *  Created on: Dec 25, 2024
 *      Author: David
 */

#ifndef INC_BT_H_
#define INC_BT_H_

#include "main.h"

#define _BT_ON_TIME         10      // Button active time in ms.
#define _BT_OFF_TIME        800     // Button release time in ms.
#define _BT_STOP_SKIP_TIME_ 2000    // Button release time in ms, special case for stop-skip action, increased delay, needed to wait for ICS to unmute after skipping tracks
#define _BT_DEBOUNCE_TIME   5       // For inputs reading status LEDFs

typedef enum {
    bt_off          = 0,
    bt_on           = 1,
    bt_linked       = bt_on|2,
    bt_streaming    = bt_on|4,
    bt_mask         = bt_linked | bt_streaming,
    bt_stop         = bt_on|8,
    bt_play         = bt_on|16,
    bt_next         = bt_on|32,
    bt_prev         = bt_on|64,
    bt_wait_on      = bt_on|128,
    bt_on_delay     = 0xFF,
} bt_state_t;

typedef struct {
    uint8_t stable;
    uint8_t last;
    uint32_t time;
} bt_readState_t;

typedef struct {
    union {
        uint8_t flags;
        struct {
            uint8_t do_play :1;
            uint8_t do_stop :1;
            uint8_t do_next :1;
            uint8_t do_prev :1;
            uint8_t busy :1;
        };
    };
    uint8_t repeat;
    uint32_t on_time;
    uint32_t off_time;
} bt_button_t;

typedef struct {
    bt_state_t bt_status;
    bt_state_t set_mode;
    bt_readState_t readState;
    bt_button_t button;

    uint32_t pwr_on_time;
} bt_t;

void BT_handle(void);
void BT_ON(void);
void BT_Stop(void);
void BT_Play(void);
void BT_Next(void);
void BT_Prev(void);

#endif /* INC_BT_H_ */
