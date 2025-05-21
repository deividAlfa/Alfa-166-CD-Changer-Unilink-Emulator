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
#include "audioDecode.h"
#include "audioSrc.h"
#include "serial.h"
#include "bt.h"

const uint8_t mag_cd[_DISCS_] = { mag_cd1, mag_cd2, mag_cd3, mag_cd4, mag_cd5, mag_cd6 };                // Bitfields for setting the presence of each CD in mag_info.cmd2
unilink_t unilink;                                          // Unilink structure
slaveBreak_t slaveBreak;                                // Slave break structure
magazine_t mag_data;                                     // Magazine data/status
cdinfo_t cd_data[_DISCS_];                // Disc structure, contains "inserted" flag, tracks, minutes, seconds.

static void unilink_cold_reset(void);
static void unilink_warm_reset(void);
static bool unilink_checksum(void);                // Checks the integrity of incoming Unilink frames
static void unilink_broadcast(void);                // Handles broadcast unilink commands
static void unilink_myid_cmd(void);                  // Handles our ID unilink commands
static void unilink_appoint(void);                        // Associates with the master
//static void unilink_send_cartridge_status(uint8_t status);                // Set status and queue a cartridgeinfo slave message
static void unilink_send_status(uint8_t status);                // Set status and queue an unilink slave message
static void unilink_update_status(void);                // Updates unilink status automatically
static void unilink_set_status(uint8_t status);                   // Set current status
static void unilink_add_slave_break(uint8_t command);                // Create and add a new message to the slave break queue based on the input command
static void unilink_handle_slave_break(void);                // Creates slave break conditions when required
static void unilink_handle_play_time(void);                // Keeps track of the playback time
static void unilink_handle_timeout(void);                // Controls Unilink protocol timeouts
static void unilink_handle_led(void);                       // Handles the activity led
static void unilink_slave_msg(void);                // Called when receiving a SlavePoll cmd. Handles the msg queue, placing the data in the TX buffer, or creating new one when empty with unilink_auto_poll.
static void unilink_spi_mode(unilink_DataMode_t mode);                // Sets the SPI peripheral to RX/TX or forces the pin output low. Handles different flags and resets the SPI when stuck.
static void unilink_wait_spi_busy(void);                // Waits until the SPI flags are cleared, only used when resetting the peripheral
static void unilink_create_msg(uint8_t *msg, volatile uint8_t *dest);                // Computes the checksums to a basic input message and creates the final Unilink frame.
static void unilink_parse(void);
/*
 void unilink_repeat(bool isOn);                         	                                	    // TODO: Not implemented
 void unilink_intro(bool isOn);
 void unilink_shuffle(bool isOn);
 */

static void flashTrackWrite(void);
static void flashTrackInit(void);
static void flashTrackHandle(void);

void unilink_init(SPI_HandleTypeDef *SPI, TIM_HandleTypeDef *tim) {
    flashTrackInit();
    unilink.timer = tim;
    unilink.SPI = SPI;
    mag_data.status = mag_inserted;
    mag_data.cmd2 = mag_full;
    //unilink_update_magazine();
    SetPinLow(UNILINK_DATA);                // Set DATA Output latch low (Doesn't affect when GPIO is set to input or SPI mode)
    unilink_cold_reset();
    __HAL_TIM_SET_AUTORELOAD(unilink.timer, _BYTE_TIMEOUT_);
    HAL_TIM_Base_Start_IT(unilink.timer);
    unilink.hwinit = 1;
    unilink_set_status(unilink_idle);;
    dac_mute();
}

void unilink_handle(void) {

    flashTrackHandle();
    unilink_parse();

    if(is_dac_muted() && unilink_status() == unilink_playing && (getAudioSource() != src_bt || BT_Allow_Play()) )
        dac_unmute();
    else if(!is_dac_muted() && unilink_status() != unilink_playing)
        dac_mute();

    if (unilink.update_time) {
        unilink.update_time = 0;
        unilink_add_slave_break(cmd_time);	// Update playback time
    }

#ifndef PASSIVE_MODE
    if (unilink.entered_poweroff == 0 && unilink.timeout > _PWROFF_TIMEOUT_) {                // 10 second without any activity
        unilink_cold_reset();
        putString("No activity timeout, shutting down...\r\n");
#ifdef USB_LOG
        flush_log();
        removeDrive();
#endif
#ifndef DEBUG
        SetPinLow(SYS_ON);
#endif
        unilink.entered_poweroff = 1;                // Set this flag to not repeat this. But don't block the program execution,
    }                //  just in case the ICS comes back to live. We'll enable the pin again if that happens...
#endif

#ifdef UNILINK_LOG_ENABLE
    unilinkLogShow();
#endif
}

static bool unilink_checksum(void) {                // Check parity of complete Unilink packet
    uint8_t i = 0;                                         // local byte counter
    uint8_t checksum = 0;                                      // local checksum

    for (i = 0; i < (unilink.rxSize - 2); i++) {
        if (i == parity1) {                // Short messages will never get here, this is the first checksum for medium or long frame
            if (checksum != unilink.rxData[parity1]) {
                return 0;
            }
        }
        else {
            checksum += unilink.rxData[i];
        }
    }
    if (checksum != unilink.rxData[i] || unilink.rxData[i + 1] != 0) {
        return 0;
    }
    return 1;                        // Second checksum is invalid, return false
}

static void unilink_handle_led(void) {                                  // Activity LED
    /*
     #if defined (PASSIVE_MODE) && defined (USB_LOG)                                                 // Passive mode: LED for usb status
     static uint32_t time = 0;
     if(HAL_GetTick()>time){
     time=HAL_GetTick();
     togglePin(LED_GPIO_Port,LED_Pin);
     if(systemStatus.driveStatus==drive_ready){
     time+=500;                                                                                // Drive mounted, slow blink
     }
     else{
     time+=50;                                                                                 // Else, fast blinking
     }
     }
     #elif !defined (PASSIVE_MODE)
     static uint32_t time = 0;
     if (unilink.masterinit) {                                                                     // Device mode: Led for unilink status
     if (unilink.status == unilink_idle) {
     setPinLow(LED_GPIO_Port, LED_Pin);                                                        // Idle state and initialized, Led on
     }
     else if (unilink.status == unilink_playing) {                                               // Play state, quick led blinking
     if (HAL_GetTick() - time > 100) {
     time = HAL_GetTick();
     TogglePin(LED);
     }
     }
     }
     else {
     if (!ReadPin(LED)) {
     if (HAL_GetTick() - time > 20) {
     time = HAL_GetTick();                                                                   // If not initialized, make small pulses
     SetPinHigh(LED);                                                     // Indicating we're alive but not initialized
     }
     }
     else {
     if (HAL_GetTick() - time > 1000) {
     time = HAL_GetTick();
     SetPinLow(LED);
     }
     }
     }
     #endif
     */
}
/*
void unilink_clear_discs(void) {
    for (uint8_t i = 0; i < _DISCS_; i++) {                       // Clear discs
        cd_data[i].inserted = 0;
        cd_data[i].tracks = 99;
        cd_data[i].mins = 0;
        cd_data[i].secs = 0;
    }
}
*/

void unilink_update_magazine(void) {                // usb was inserted, removed or contents changed
#ifndef PASSIVE_MODE
    mag_data.cmd2 = mag_full;
    unilink.disc = 0;                // First cd is 1. Set to 0 to detect if the following loop fails

    if(getAudioSource()==src_usb){
        unilink_restore_usb_position();

        if(gen_usb_discinfo() == OK){                                               // USB ok
            if(cd_data[unilink.disc-1].inserted == 0){                              // If restored disc is not valid
                unilink.track = 0;                                                    // Reset track
                for (uint8_t i = 0; i < _DISCS_; i++){
                    if (cd_data[i].inserted && cd_data[unilink.disc-1].tracks != 0){    // Find first valid disc
                        unilink.disc = i + 1;
                        break;
                    }
                }
                if (unilink.track == 0 || unilink.track >= cd_data[unilink.disc - 1].tracks)    // Check if restored track is valid
                    unilink.track = 1;
            }
        }
        if(unilink.disc==0){                     // USB Empty
            cd_data[0].tracks = 0xEE;
            unilink.disc = 1;
            unilink.track = 33;
        }
    }
    else{
        cd_data[0].mins = 88;
        cd_data[0].secs = 00;
        cd_data[0].inserted = 1;
        unilink.disc = 1;
        if(getAudioSource()==src_aux){
            cd_data[0].tracks = 0xAA;
            unilink.track = 44;
        }
        else if(getAudioSource()==src_bt){
            cd_data[0].tracks = 0xBB;
            unilink.track = 88;
        }
    }

    if (!unilink.masterinit)
        return;

    AudioStop();
    unilink_reset_playback_time();
    unilink.play = 0;                   // Don't go into play mode automatically
#endif
}
//  Unilink frame structure
//  char data[]= "00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00";
//                0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
//                rad tad m1  m2  c1  d1  d2  d3  d4  d5  d6  d7  d8  d9  c2
//                [             ] [c] [                                ] [c]  [0]   // 16 byte frame
//                [             ] [c] [            ] [c] [0]                        // 11 byte frame
//                [             ] [c] [0]                                           // 6 byte frame

static void unilink_create_msg(uint8_t *msg, volatile uint8_t *dest) {
#ifndef PASSIVE_MODE
    uint8_t checksum = 0;                                      // local checksum
    uint8_t i = 0;                                                // local index
    uint8_t size;                                                 // msg size

    if (msg[2] >= 0xC0) {
        size = unilink_long;
    }                                                         // 16 byte command
    else if (msg[2] >= 0x80) {
        size = unilink_medium;
    }                                                         // 11 byte command
    else {
        size = unilink_short;
    }                                                         //  6 byte command

    while (i < parity1) {
        checksum += msg[i];                // copy the first 4 bytes from msg to Tx buffer
        dest[i] = msg[i];                       // and calculate checksum for it
        i++;
    }
    // i = byte 4: checksum1
    dest[i++] = checksum;                         // copy checksum1 to Tx buffer
    // i = byte 5: D1
    while (i < (size - 2)) {                               // Handle rest of msg
        dest[i] = msg[i - 1];                 // copy from msg to Tx data buffer
        checksum += msg[i - 1];                    // add each byte to checksum2
        i++;
    }

    dest[i++] = checksum;                                     // Store checksum2
    dest[i] = 0;                                          // End of msg always 0

    if (dest == unilink.txData) {                        // If dest is tx buffer
        unilink.txSize = size;                                  // Store Tx size
        unilink_spi_mode(mode_SPI_tx);
    }
    else {                                                 // Slave break buffer
        dest[parity2_L + 2] = size;                             // Store Tx size
    }
#endif
}

static void unilink_parse(void){
    if(unilink.received == 0) return;
    unilink.received = 0;

    if (unilink_checksum()) {
#ifndef PASSIVE_MODE
        if (unilink.rxData[dst_addr] == addr_broadcast)
            unilink_broadcast();                  // parse broadcast packets
        else if (unilink.rxData[dst_addr] == unilink.ownAddr)
            unilink_myid_cmd();                   // parse packets for my ID
        else if ((unilink.rxData[dst_addr] & 0xF0) == unilink.groupID)                // Appoint for us?
            unilink_appoint();                    // do ID appoint procedure
#endif
    }
    else {
        unilink.bad_checksum = 1;                // Bad checksum, probably skipped a clock
        putString("BAD CHECKSUM\r\n");                // Resync by ignoring further data until master stops sending clocks and triggers a byte timeout.
    }
}

static void unilink_broadcast(void) {                             // BROADCAST COMMANDS
#ifndef PASSIVE_MODE
    switch (unilink.rxData[cmd1]) {                               // Switch CMD1
        case cmd_busRequest:                    // 0x01 Bus requests (Broadcast)
        {
            if(unilink.entered_poweroff){
                SetPinHigh(SYS_ON);
                unilink.entered_poweroff = 0;
                putString("Resuming after activity timeout!\r\n");
            }
            // 0x01 0x00 Bus reset
            switch (unilink.rxData[cmd2]) {                       // Switch CMD2
                case cmd_busReset:
                    unilink_cold_reset();
                    unilink.busReset = 1;
                    break;
                case cmd_anyone:                            // 0x01 0x02 Anyone?
                    if (!unilink.masterinit && unilink.busReset) {
                        unilink.appoint = 1;                   // Enable appoint
                        uint8_t msg[] = msg_anyoneResp;
                        unilink_create_msg(msg, unilink.txData);                // send my device info string
                    }
                    break;
                default:
                    break;
            }
            break;
        }
        case cmd_source:                               // 0xF0 SRC Source select
            if (unilink.rxData[cmd2] != unilink.ownAddr)                 // check if interface is deselected
                unilink_set_status(unilink_idle);                // set idle status on deselect
            break;
        case cmd_power:                                      // 0x87 Power Event
            if (unilink.rxData[cmd2] == cmd_pwroff) {                // 0x00 Power off
                unilink.play = 0;
                unilink.powered_on = 0;
                unilink_set_status(unilink_idle);                // set idle status on power off
                AudioPause();
                unilink.off_time = HAL_GetTick();
            }
            else if (unilink.rxData[cmd2] == cmd_pwron) {                // 0x89 Power on
                unilink.play = 0;
                unilink.powered_on = 1;
                unilink_set_status(unilink_idle);
            }
            break;
    }
#endif
}

static void unilink_myid_cmd(void) {
#ifndef PASSIVE_MODE
    switch (unilink.rxData[cmd1]) {                               // Switch CMD1
        case cmd_busRequest:                    // 0x01 Bus requests (for my ID)
        {
            switch (unilink.rxData[cmd2]) {
                case cmd_timePoll:                // 0x01 0x12 Respond to time poll (PONG)
                {
                    uint8_t msg[] = msg_status;
                    unilink_create_msg(msg, unilink.txData);
                    unilink_update_status();                // Update our status after sending current one
                    break;
                }
                case cmd_slavePoll:                // 0x01 0x13 permission to talk after poll request
                {
                    unilink_slave_msg();                  // send slave response
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case cmd_play:                                              // 0x20 PLAY
        {

            unilink_set_status(unilink_changing);      // Send changing status to force reset ICS disc/track
            if (!unilink.powered_on) break;                 // Not powered on, ignore
            if ((mag_data.status != mag_removed)
                && (unilink.status != unilink_ejecting)) {                // If magazine is present and we are not ejecting      //FIXME: Ejecting check might be wrong?
                if (cd_data[unilink.disc - 1].inserted) {                // If current selected disc is valid
                    if (unilink.track >= cd_data[unilink.disc - 1].tracks)                // If current track is valid
                        unilink.track = 1;                  // Else, reset track
                    unilink.play = 1;                       // Allow play

                    if(unilink_status() == unilink_idle)    // If idling, set play status
                        unilink_send_status(unilink_playing); // Other states: Will set play automatically when done
                }
                else {
                    unilink.disc = 0;
                    for (uint8_t i = 0; i < _DISCS_; i++) {                // Else, find the first valid disc
                        if (cd_data[i].inserted) {
                            unilink.disc = i + 1;
                            unilink.track = 1;
                            unilink.play = 1;
                            unilink_send_status(unilink_playing);
                            break;
                        }
                    }
                    if (unilink.disc == 0) {                // No valid cd was found
                        unilink_send_status(unilink_idle);                // FIXME: Untested, probably wrong or lacking further actions
                    }
                }
            }
            break;
        }
        case cmd_switch:                                // 0x21 TA message start
        {
            if (unilink.rxData[cmd2] == 0x20)              // Not used?
                unilink_set_status(unilink_idle);
            else if (unilink.rxData[cmd2] == 0x10) {             // Switch to CD? (Enable CD)
                uint32_t now = HAL_GetTick();
                uint32_t off_elapsed = now - unilink.off_time;
                uint32_t src_elapsed = now - unilink.src_time;
                if(src_elapsed>3000 && off_elapsed>1000 && off_elapsed<3000){  // Quick disable/enable sequence,switch source
                    AudioStop();
                    unilink_reset_playback_time();
                    unilink.src_time = now;
                    setAudioSource(src_auto);
                }
            }
            break;
        }
        case cmd_textRequest:                        // 0x84 request for command
        {
            switch (unilink.rxData[cmd2]) {
                case cmd_magazine:                 // 0x95 request magazine info
                {
                    unilink_add_slave_break(cmd_magazine);
                    break;
                }
                case cmd_discinfo:                // 0x97 request disc total time and tracks
                {
                    unilink_add_slave_break(cmd_discinfo);
                    if(unilink_status()==unilink_playing && unilink.play)
                        unilink_add_slave_break(cmd_time);
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case cmd_goto:                                  // 0xB0 Direct Disc keys
        {
            uint8_t disc = unilink.rxData[cmd2] & 0x0F;
            int8_t track = bcd2hex(unilink.rxData[d1]);
            unilink.changing = 1;
            if(track==0) track=1;
            if (getAudioSource() == src_bt && unilink.disc == disc) {

                if (track == unilink.track)
                    BT_Prev();
                else if (abs(unilink.track - track) < 5){                    // Sometimes the ICS is too slow and still reports an older track than what we already reported, and might requestand older track than what we already reported, so ignore large skips
                    if (track < unilink.track)
                        for (uint8_t i = track; i < unilink.track; i++)
                            BT_Prev();
                    else
                        for (uint8_t i = unilink.track; i < track; i++)
                            BT_Next();
                }
            }
            if (unilink.disc != disc) {                             // Disc changed
                unilink_set_status(unilink_changing);
                if (!cd_data[disc - 1].inserted) {                  // If requested disc is not present
                    unilink.fake_change = 1;                        // Hack to temporally agree with ICS
                    unilink.fake_track = track;
                    unilink.fake_disc = disc;
                    AudioPause();                                   // Fake change, only pause current track, resume later
                    if (!cd_data[unilink.disc - 1].inserted) {      // If previous disc is empty (Shouldn't happen...)
                        unilink.disc = 0;                           // Set current disc invalid
                        for (uint8_t i = 0; i < _DISCS_; i++) {     // Find first valid one
                            if (cd_data[i].inserted) {
                                unilink.disc = i + 1;
                                break;
                            }
                        }
                    }
                }
                else
                    updateFiles();                                  // Changing disc ok, force file sort
            }
            else                                                   // Only changed track
                unilink_set_status(unilink_playing);

            if (!unilink.fake_change){                              // Valid disc / track
                AudioStop();
                unilink.disc = disc;
                unilink.track = track;
                unilink_reset_playback_time();
            }
            break;
        }
        default:
            break;
    }
#endif
}

static void unilink_appoint(void) {                            // respond to ID appoint
#ifndef PASSIVE_MODE
    if ((unilink.rxData[cmd1] == 0x02) && unilink.appoint) {                // check for previous Anyone command
        if ((unilink.rxData[dst_addr] & 0xF0) == unilink.groupID) {                // check if packet is for my group
            if (!unilink.masterinit) {                           // I have no ID
                unilink.masterinit = 1;
                unilink.appoint = 0;                          // Disable appoint
                unilink.ownAddr = unilink.rxData[dst_addr];                // save my new ID
                uint8_t msg[] = msg_anyoneResp;                // Generate response
                unilink_create_msg(msg, unilink.txData);                // send my device info string
            }
        }
    }
#endif
}

static void unilink_update_status(void) {
#ifndef PASSIVE_MODE
    unilink.status = unilink.send_status;
    switch (unilink.send_status) {
        /*
        case unilink_ejecting:
            unilink.status = unilink_idle;
            break;
        */
        case unilink_changing:
            unilink.send_status = unilink_changed;
            break;
        case unilink_changed:
            unilink.send_status = unilink.play ? unilink_playing : unilink_idle;
            //unilink.status = unilink.play ? unilink_seeking : unilink_idle;
            break;
            /*
        case unilink_seeking:
            unilink.status = unilink_playing;
            break;
            */
        case unilink_playing:
                unilink.changing = 0;
            break;
        default:
            break;
    }
#endif
}
static void unilink_set_status(uint8_t status) {
#ifndef PASSIVE_MODE
    unilink.send_status = status;
    unilink.status = status;
#endif
}

unilinkStatus_t unilink_status(void){
    return(unilink.changing ? unilink_idle : unilink.status);
}
unilinkStatus_t unilink_sent_status(void){
    return(unilink.changing ? unilink_idle : unilink.status);
}
static void unilink_cold_reset(void) {
    unilink.masterinit = 0;
    unilink.busReset = 0;
    unilink.appoint = 0;                // We don't want appoint yet (only after "Anyone" command)
    unilink.ownAddr = addr_reset;                        // Load default address
    unilink.groupID = addr_reset;                        // Load default address
    unilink_warm_reset();
}

static void unilink_warm_reset(void) {
    unilink.powered_on = 0;
    unilink.timeout = 0;
    unilink.rxCount = 0;
    unilink.txCount = 0;
    slaveBreak.in = 0;
    slaveBreak.out = 0;
    slaveBreak.pending = 0;
    unilink.play = 0;
    unilink.status = unilink_idle;
    slaveBreak.msg_state = break_msg_idle;
    unilink_spi_mode(mode_SPI_rx);
    AudioStop();
    unilink_reset_playback_time();

    if (unilink.ownAddr == addr_reset)
        putString("COLD RESET\r\n");
    else
        putString("WARM RESET\r\n");
}

static void unilink_wait_spi_busy(void) {                // Wait until SPI flag busy clears out
    uint32_t t = HAL_GetTick() + 2;                // 2ms timeout should be more than enough
    while (unilink.SPI->Instance->SR & SPI_SR_BSY) {                // Wait until Busy is gone
        if (HAL_GetTick() > t) {
            putString("SPI Timeout! Resetting system\r\n");
            HAL_Delay(500);                // Some time to send the message just in case
            __NVIC_SystemReset();                // Busy is not going out, we've already tried everything, so reset the system (Should never happen)
        }
    }
}

static void unilink_spi_mode(unilink_DataMode_t mode) {
    __HAL_SPI_DISABLE(unilink.SPI);
    __HAL_SPI_CLEAR_OVRFLAG(unilink.SPI);
    __HAL_SPI_CLEAR_FREFLAG(unilink.SPI);
    __HAL_SPI_CLEAR_MODFFLAG(unilink.SPI);

    __HAL_SPI_DISABLE_IT(unilink.SPI, (SPI_IT_RXNE | SPI_IT_TXE));

    if (mode & mode_SPI) {
        SetPinMode(UNILINK_DATA, MODE_AF);
        WritePin(UNILINK_OUT_EN, (mode == mode_SPI_rx ? 1 : 0));
    }
    else if (mode == mode_output_low) {                     // GPIO Output (Low)
        SetPinMode(UNILINK_DATA, MODE_OUTPUT);
        SetPinLow(UNILINK_OUT_EN);
        return;
    }

    unilink.timeout = 0;
    unilink.bad_checksum = 0;
    unilink.rxCount = 0;
    unilink.txCount = 0;

    if ((unilink.SPI->State > HAL_SPI_STATE_READY)
        && (unilink.SPI->State < HAL_SPI_STATE_ERROR)) {                // Handler BUSY in any mode (But not error or reset);
        unilink.SPI->State = HAL_SPI_STATE_READY;
    }
    else if ((unilink.SPI->Instance->SR & SPI_SR_BSY)
        || (unilink.SPI->State != HAL_SPI_STATE_READY)) {                // If peripheral is busy (Probably missed some clocks and it's waiting for more), or handler error
        unilink.SPI->State = HAL_SPI_STATE_RESET;

#ifdef SPI1
        if (unilink.SPI->Instance == SPI1) {
            __HAL_RCC_SPI1_FORCE_RESET();                          // Reset SPI1
            unilink_wait_spi_busy();
            __HAL_RCC_SPI1_RELEASE_RESET();                     // Release reset
        }
#endif
        /*
         #ifdef SPI2                                                // XXX: Only required if SPI module is changed
         if(unilink.SPI->Instance == SPI2){
         __HAL_RCC_SPI2_FORCE_RESET();                              // Reset SPI2
         unilink_wait_spi_busy();
         __HAL_RCC_SPI2_RELEASE_RESET();                            // Release reset
         }
         #endif
         #ifdef SPI3
         if(unilink.SPI->Instance == SPI3){
         __HAL_RCC_SPI3_FORCE_RESET();                              // Reset SPI3
         unilink_wait_spi_busy();
         __HAL_RCC_SPI3_RELEASE_RESET();                            // Release reset
         }
         #endif
         #ifdef SPI4
         if(unilink.SPI->Instance == SPI4){
         __HAL_RCC_SPI4_FORCE_RESET();                              // Reset SPI4
         unilink_wait_spi_busy();
         __HAL_RCC_SPI4_RELEASE_RESET();                            // Release reset
         }
         #endif
         #ifdef SPI5
         if(unilink.SPI->Instance == SPI5){
         __HAL_RCC_SPI5_FORCE_RESET();                              // Reset SPI5
         unilink_wait_spi_busy();
         __HAL_RCC_SPI5_RELEASE_RESET();                            // Release reset
         }
         #endif
         */
        unilink_wait_spi_busy();                              // Check BSY again

        if (HAL_SPI_Init(unilink.SPI) != HAL_OK)                  // Re-init SPI
            Error_Handler();
    }

    slaveBreak.break_state = break_wait_data_low;

    if (mode == mode_SPI_rx) {
        slaveBreak.msg_state = break_msg_idle;
        unilink.mode = mode_rx;
        SPI_1LINE_RX(unilink.SPI);
        __HAL_SPI_ENABLE_IT(unilink.SPI, (SPI_IT_RXNE | SPI_IT_ERR));
    }
    else {
        unilink.mode = mode_tx;
        SPI_1LINE_TX(unilink.SPI);
        __HAL_SPI_ENABLE_IT(unilink.SPI, (SPI_IT_TXE | SPI_IT_ERR));
    }
    __HAL_SPI_ENABLE(unilink.SPI);
}

void unilink_reset_playback_time(void) {
#ifndef PASSIVE_MODE
    __disable_irq();
    unilink.min = 0;
    unilink.sec = 0;
    unilink.millis = 0;
    __enable_irq();
#endif
}
uint8_t unilink_disc(void){
    return unilink.disc;
}
uint8_t unilink_track(void){
    return unilink.track;
}
void unilink_next_track(void){
    if (++unilink.track >= cd_data[unilink.disc - 1].tracks) {
        unilink.track = 1;
    }
}

void unilink_backup_usb_position(void){
    unilink.usb_disc=unilink.disc;
    unilink.usb_track=unilink.track;
}
void unilink_restore_usb_position(void){
    if(unilink.usb_disc  && unilink.usb_disc < _DISCS_ &&
       unilink.usb_track && unilink.usb_track < _MAXFILES_){
      unilink.disc = unilink.usb_disc;
      unilink.track = unilink.usb_track;
    }
    else{
      unilink.disc = 1;
      unilink.track = 1;
    }
}
void unilink_clear_backup_usb_position(void){
    unilink.usb_disc=0;
    unilink.usb_track=0;
}
static void unilink_send_status(uint8_t status) {
    unilink_set_status(status);
    unilink_add_slave_break(cmd_status);
}

static void unilink_add_slave_break(uint8_t command) {
#ifdef PASSIVE_MODE
    return;
#endif
    uint8_t i = slaveBreak.in;                             // Input buffer index
    if (slaveBreak.pending >= _BREAK_QUEUE_SZ_) {                // slave break queue full?
        slaveBreak.lost++;                // For debugging purposes, shouldn't happen
        putString("Slave buffer full! Increase _BREAK_QUEUE_SZ_\r\n");
        return;
    }
    switch (command) {
        case cmd_magazine:                                      // magazine info
        {
            uint8_t msg[] = msg_magazine;
            unilink_create_msg(msg, (uint8_t*) slaveBreak.data[i]);
            break;
        }
        case cmd_discinfo:                         // disc total time and tracks
        {
            uint8_t msg[] = msg_discinfo;
            unilink_create_msg(msg, (uint8_t*) slaveBreak.data[i]);
            break;
        }
        case cmd_time:                                       // time info update
        {
            uint8_t msg[] = msg_time;
            if(unilink.fake_change){
                unilink.fake_change = 0;
                msg[4] = hex2bcd(unilink.fake_track);           // Send requested disc once to make ICS happy
                msg[7] = ((unilink.fake_disc<<4)|0xA);          // Otherwise it'll keep asking for the disc over and over
            }                                                   // Sending "Empty disc" cmd causes issues so this is a workaround

            unilink_create_msg(msg, (uint8_t*) slaveBreak.data[i]);

            if(getAudioSource() == src_aux)       // Aux: Instantly revert to track 44 as there's nothing to skip
              unilink.track = 44;
            else if(getAudioSource() == src_bt && !BT_Busy() && (unilink.min || unilink.sec > 5) ) // BT: For multiple skip detection, revert to track 88 after BT buttons are done
              unilink.track = 88;                                                                  // Wait until BT finished processing the buttons, + 3 seconds to make sure the ICS Acks the new track.
            break;
        }
        case cmd_status:                                              // status
        {
            uint8_t msg[] = msg_status;
            unilink_create_msg(msg, (uint8_t*) slaveBreak.data[i]);
            break;
        }
        case cmd_cartridgeinfo:                      // info about the cartridge
        {
            uint8_t msg[] = msg_cartridge_info;
            unilink_create_msg(msg, (uint8_t*) slaveBreak.data[i]);
            break;
        }
        default: {
            uint8_t msg[] = msg_status;
            putString("unilink_add_slave_break: Unknown cmd!\r\n");
            unilink_create_msg(msg, (uint8_t*) slaveBreak.data[i]);
            break;
        }
    }
    if (++slaveBreak.in >= _BREAK_QUEUE_SZ_)
        slaveBreak.in = 0;

    __disable_irq();
    slaveBreak.pending++;
    __enable_irq();
}

static void unilink_handle_play_time(void) {
    if (unilink.status == unilink_playing && unilink.changing==0 && (getAudioSource() != src_bt || BT_Allow_Play()) ) {  // Start play time when Unilink changing / BT are done
        unilink.millis++;
        if (unilink.millis == 500)
            unilink.update_time = 1;                // Send time msg twice a second
        else if (unilink.millis > 999) {
            unilink.update_time = 1;
            unilink.millis = 0;
            if (++unilink.sec > 59) {                     // Simulates play time
                unilink.sec = 0;
                if (++unilink.min > 59)
                    unilink.min = 0;
            }
        }
    }
}

static void unilink_handle_timeout(void) {
    unilink.timeout++;

    if (unilink.masterinit) {
        if (unilink.timeout > _RESET_TIMEOUT_) {                // No clock from master
            putString("Master Timeout\r\n");
            unilink_cold_reset();
        }
    }

    if (unilink.mode == mode_tx) {
        if ((unilink.timeout > _MASTER_REQUEST_TIMEOUT_)) {                // Timeout while waiting clocks from master to answer a poll cmd
            if (slaveBreak.msg_state == break_msg_pendingTx) {
                putString("Slave msg timeout (Pending Tx)\r\n");
                slaveBreak.msg_state = break_msg_idle;
            }
            else if (unilink.txCount < unilink.txSize)                // We expect a timeout after sending a frame, so ignore if count is ok
                putString("Answer Timeout (TX)\r\n");                // Timeout while sending a frame
            unilink.timeout = 0;
            unilink_spi_mode(mode_SPI_rx);
        }
    }
}
static void unilink_handle_slave_break(void) {

    if (!unilink.masterinit ||                      // Exit if not initialized,
        unilink.mode != mode_rx ||                             // transmitting,
        unilink.rxCount ||                                // reception going on
        slaveBreak.pending == 0) {                            // nothing to send

        return;
    }

    bool data_now = ReadPin(UNILINK_DATA);
    if (slaveBreak.sample_time)                // For sampling data pin high/low time to generate breaks
        slaveBreak.sample_time--;

    switch (slaveBreak.break_state) {

        case break_wait_data_low:
            if (!data_now) {
                slaveBreak.sample_time = 8;                // Wait for DATA low for 8mS
                slaveBreak.break_state = break_wait_data_low_time;
            }
            break;

        case break_wait_data_low_time:
            if (slaveBreak.sample_time) {                   // Timer not expired
                if (data_now)                              // If DATA goes high,
                    slaveBreak.break_state = break_wait_data_low;                // reset state
            }
            else {                                            // Timeout expired
                slaveBreak.sample_time = 3;                // 3ms timeout while waiting for data to go HIGH
                slaveBreak.break_state = break_wait_data_high;
            }
            break;

        case break_wait_data_high:
            if (slaveBreak.sample_time) {                 // Timeout not expired
                if (data_now) {                                  // If DATA high
                    slaveBreak.sample_time = 2;                      // Wait 2ms
                    slaveBreak.break_state = break_wait_data_high_time;
                }
            }
            else
                // Timeout
                slaveBreak.break_state = break_wait_data_low;                // DATA didn't go high within time, reset state

            break;

        case break_wait_data_high_time:
            if (slaveBreak.sample_time) {                 // Timeout not expired
                if (!data_now)                              // If data goes low,
                    slaveBreak.break_state = break_wait_data_low;                // reset state
            }
            else {                                       // Timeout, start break
                unilink_spi_mode(mode_output_low);                // Pull data low
                slaveBreak.sample_time = 2;                // Keep data low for 2ms
                slaveBreak.break_state = break_wait_data_setlow;
            }
            break;

        case break_wait_data_setlow:
            if (!slaveBreak.sample_time) {                    // Timeout expired
                unilink_spi_mode(mode_SPI_rx);                // Release data, set to SPI
                slaveBreak.msg_state = break_msg_pending;                // Break condition executed, now wait for slave poll command from master
                slaveBreak.break_state = break_wait_data_low;                // Reset status
            }
            break;

        default:
            break;
    }
}

static void unilink_slave_msg(void) {
    if (slaveBreak.pending == 0) {                // If empty queue, self-generate data
        if(unilink_status() == unilink_playing && unilink.play)
            unilink_add_slave_break(cmd_time);
        else{
            unilink_add_slave_break(cmd_status);
            unilink_update_status();                // Update our status after sending current one
        }
    }
    unilink.txSize = slaveBreak.data[slaveBreak.out][parity2_L + 2];
    for (uint8_t i = 0; i < unilink.txSize; i++)                // Copy stored slave break message to Tx Buffer
        unilink.txData[i] = slaveBreak.data[slaveBreak.out][i];
    slaveBreak.msg_state = break_msg_pendingTx;
    unilink_spi_mode(mode_SPI_tx);
}

void unilink_tick(void) {
    if (!unilink.hwinit)
        return;
    unilink_handle_led();
    unilink_handle_play_time();
    unilink_handle_timeout();
    unilink_handle_slave_break();
}

void unilink_byte_timeout(void) {                // Byte timeout (When the master already started sending clocks)
    __HAL_TIM_CLEAR_IT(unilink.timer, TIM_IT_UPDATE);                // Clear flag

    if (unilink.mode == mode_rx) {
        if (unilink.rxCount && unilink.rxCount < unilink.rxSize) {
            if (unilink.rxData[cmd1] != cmd_seek)                // This command always causes timeout because the ICS doesn't support 16-byte slave poll answers (Max. is 13).
                putString("Byte timeout (RX)\r\n");                // This only happens in passive mode, we don't send this message
            unilink_spi_mode(mode_SPI_rx);
        }
        else if (unilink.bad_checksum)                // Resync after a bad checksum, waiting until the master stops transmitting to reset our state
            unilink_spi_mode(mode_SPI_rx);
    }
    else if (unilink.mode == mode_tx && (unilink.txCount > 1)) {                // Byte timeout when sending a frame
        if (slaveBreak.msg_state == break_msg_sending) {
            slaveBreak.msg_state = break_msg_idle;
            putString("Slave msg byte timeout (TX)\r\n");
        }
        else if (unilink.txCount < unilink.txSize)
            putString("Byte timeout (TX)\r\n");
        unilink_spi_mode(mode_SPI_rx);
    }
}

void unilink_callback(void) {
    __HAL_TIM_SET_COUNTER(unilink.timer, 0);                // Reset byte timeout counter
    __HAL_TIM_CLEAR_FLAG(unilink.timer, TIM_FLAG_UPDATE);
    unilink.timeout = 0;

    if (unilink.mode == mode_rx && (unilink.SPI->Instance->CR2 & SPI_CR2_RXNEIE)
        && (unilink.SPI->Instance->SR & SPI_SR_RXNE)) {                // If in receive mode
        uint8_t rx = ~*(__IO uint8_t*) &unilink.SPI->Instance->DR;                // store last received byte (inverted)
        if (unilink.bad_checksum) {                // bad checksum: Discard everything until the master transmission ends, causing a byte timeout (Resync)
            return;
        }
        if (unilink.rxCount == 0) {                             // If first byte
            if (rx < 0x10 || rx > 0xF0)
                return;                              // Ignore invalid addresses
            else
                // First valid byte
                unilink.rxSize = unilink_short;                // Begin by setting frame size to short until we get cmd1
        }
        unilink.rxData[unilink.rxCount] = rx;                // Store data in the buffer
        if (unilink.rxCount == cmd1) {                // cmd1 received, detect msg size now
            if (unilink.rxData[cmd1] >= 0xC0)
                unilink.rxSize = unilink_long;
            else if (unilink.rxData[cmd1] >= 0x80)
                unilink.rxSize = unilink_medium;
        }
        if (++unilink.rxCount == unilink.rxSize) {                // Frame complete
            unilinkLogUpdate(mode_rx);
            unilink_spi_mode(mode_SPI_rx);                               // Done
            unilink.received = 1;
        }
    }
    else if (unilink.mode == mode_tx
        && (unilink.SPI->Instance->CR2 & SPI_CR2_TXEIE)
        && (unilink.SPI->Instance->SR & SPI_SR_TXE)) {                // If in transmit mode
        if (unilink.txCount == 1) {                // First interrupt is only caused by empty SPI Tx buffer, second one actually happen when the first byte was sent
            if (slaveBreak.msg_state == break_msg_pendingTx)                // We sent the first byte, this will be the second one. If this was a slave break
                slaveBreak.msg_state = break_msg_sending;                // Update state
        }
        if (unilink.txCount < unilink.txSize)                // check if bytes left
            *(__IO uint8_t*) &unilink.SPI->Instance->DR =
                ~unilink.txData[unilink.txCount++];                // output next byte (inverted)
        else
            // No data left
            *(__IO uint8_t*) &unilink.SPI->Instance->DR = ~0x00;                // Clock extra bytes as 0 (Again, inverted)

        if (unilink.txCount == unilink.txSize) {                // Last byte sent
            unilink.txCount++;                // Done. Increase txCount so we don't get there again in the following extra clocks sent by the master
                                                  // When master stops sending clocks, the byte timeout will reset back to Rx mode.
            unilinkLogUpdate(mode_tx);
            if (slaveBreak.msg_state == break_msg_sending) {                // Was this a slave break response?
                unilinkLogBreak();
                slaveBreak.msg_state = break_msg_idle;                   // Done
                if (slaveBreak.pending) {                // Pending should always be at least 1
                    slaveBreak.pending--;                            // Decrease
                    if (++slaveBreak.out >= _BREAK_QUEUE_SZ_)                // Increase out index
                        slaveBreak.out = 0;
                }
                else
                    Error_Handler();                // We got there without pending breaks??
            }
        }
    }
}

uint32_t flash_index;

typedef union {
    uint16_t data;
    struct{
        uint8_t usb_track :8;
        uint8_t usb_disc :4;
        uint8_t source :4;
    };
} flash_save_t ;

#define _FLASH_SAVE_SZ_ (128UL*1024/sizeof(flash_save_t))
flash_save_t flash_last;
__attribute__((section(".flashSettings"))) flash_save_t flash_save[_FLASH_SAVE_SZ_];

static void flashTrackWrite(void)
{
  uint32_t _irq = __get_PRIMASK();
  __disable_irq();

  HAL_FLASH_Unlock();
  __set_PRIMASK(_irq);

  if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)&flash_save[flash_index], flash_last.data ) != HAL_OK)
    Error_Handler();
  HAL_FLASH_Lock();
  flash_index++;
}

static void flashTrackErase(void)
{
  uint32_t error = 0;
  FLASH_EraseInitTypeDef erase = {
      .TypeErase   = FLASH_TYPEERASE_SECTORS,
      .Banks = 0,
      .Sector = FLASH_SECTOR_7,
      .NbSectors = 1,
      .VoltageRange =  FLASH_VOLTAGE_RANGE_3
  };

  uint32_t _irq = __get_PRIMASK();
  __disable_irq();
  HAL_FLASH_Unlock();
  __set_PRIMASK(_irq);

  if((HAL_FLASHEx_Erase(&erase, &error)!=HAL_OK) || (error != UINT32_MAX))
     Error_Handler();
  HAL_FLASH_Lock();

  for (uint32_t i = 0u; i < _FLASH_SAVE_SZ_; i++) {        // Ensure flash was erased
    if((volatile uint16_t)flash_save[i].data != UINT16_MAX)
      Error_Handler();
  }
  flash_index = 0;
  flashTrackWrite();
}

void flashTrackSetDefaults(void){
    flash_last.source = src_bt;
    flash_last.usb_disc = 0;
    flash_last.usb_track = 0;
    flashTrackErase();
}

static void flashTrackInit(void) //call it only once during init
{
  bool setDefault=0;
  uint32_t i;
  for(i=0; i<(_FLASH_SAVE_SZ_-1) && flash_save[i+1].data != UINT16_MAX; i++);   // Seek through the array

  if(i<(_FLASH_SAVE_SZ_-1)){                                                              // Free slot found
    for(uint32_t j=i+1; j<_FLASH_SAVE_SZ_; j++) {                                     // Ensure rest of array is erased
      if((volatile uint16_t)flash_save[j].data != UINT16_MAX){                          // Found unexpected data
        setDefault=1;                                                                                         // Reset
        break;
      }
    }
  }                                                                                                           // No free slot found, reset
  else
    setDefault=1;

  if(setDefault || (volatile uint16_t)flash_save[i].data==UINT16_MAX){        // Reset defaults or no data
    flashTrackSetDefaults();
    i=0;
  }
  flash_last = (volatile flash_save_t)flash_save[i];
  unilink.usb_disc = flash_last.usb_disc;
  unilink.usb_track = flash_last.usb_track;
  //setAudioSource(((volatile flash_save_t)flash_save[i]).source);// FIXME: Might restore non present sources and get stuck?
  setAudioSource(src_bt);
  flash_index = i+1;
}

static void flashTrackHandle(void){

    if(HAL_GetTick()<5000) return;             // Ignore first 5 seconds after boot to let everything settle down

    flash_save_t new;
    new.source = getAudioSource();
    if(new.source==src_usb){
        new.usb_disc = unilink.disc;
        new.usb_track =  unilink.track;
    }
    else{
        new.usb_disc = unilink.usb_disc;
        new.usb_track =  unilink.usb_track;
    }
    if( (flash_last.data != new.data) && (unilink.min || unilink.sec>5) ){      // Stable for at least 5 seconds
        flash_last = new;
        if(flash_index >= (_FLASH_SAVE_SZ_-1))                // All positions used
          flashTrackErase();
        flashTrackWrite();
    }
    if(getAudioSource()==src_usb && usb_has_files()==0)      // 10 second after boot and no usb available, abort usb restore attempt, switch to BT
       setAudioSource(src_bt) ;
}

void flashTrackRestoreFromFlash(void){
    unilink.usb_disc = flash_last.usb_disc;
    unilink.usb_track = flash_last.usb_track;
}
