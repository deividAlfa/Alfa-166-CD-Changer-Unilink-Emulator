/*
 * config.h
 *
 *  Created on: Dec 18, 2024
 *      Author: David
 */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_


/*  Main options  */
//#define PASSIVE_MODE                              // Only sniff the bus, passive operation
#define AUDIO_SUPPORT                               // Support for audio playback from USB drive (MP3).
//#define _BT_MODE_                                 // Support for Bluetooth
//#define DEBUG_ALLOC                               // To debug heap usage. See _malloc, _calloc, _free in main.h


/*   Logging outputs. Can be enabled concurrently   */
#define SWO_PRINT                                   // Print to SWO
//#define UART_PRINT                                // Print to the uart
//#define USB_LOG                                   // Print into a USB file


/*   Logging options   */
#define UNILINK_LOG_ENABLE
#define UNILINK_LOG_DETAILED                      // Enable to append command description to each decoded frame
//#define UNILINK_LOG_TIMESTAMP                     // Add timestamps


/*  Unilink protocol stuff */
#define _MASTER_REQUEST_TIMEOUT_  10              // Timeout in ms waiting for master clock to start sending clocks for our answer
#define _BYTE_TIMEOUT_            1300            // Timeout in us between bytes in a frame
#define _RESET_TIMEOUT_           2000            // Timeout in ms without master clock when idle causing a warm reset
#define _PWROFF_TIMEOUT_          10000           // Timeout in ms without master clock to release PWR_ON pin ans shut down (Ourselves)
#define _BREAK_INTERVAL_          5               // Limiter for self-generated messages to avoid flooding the master
#define _DISCS_                   6               // Discs in the system






/* Do not touch */

#if defined (PASSIVE_MODE) && defined (AUDIO_SUPPORT)
#undef AUDIO_SUPPORT
#endif

#if defined (PASSIVE_MODE) && !defined (UNILINK_LOG_ENABLE)
#define UNILINK_LOG_ENABLE
#endif


#endif /* INC_CONFIG_H_ */
