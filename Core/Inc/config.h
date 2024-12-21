/*
 * config.h
 *
 *  Created on: Dec 18, 2024
 *      Author: David
 */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_


#define AUDIO_SUPPORT
//#define DEBUG_ALLOC


#define SWO_PRINT                     // Print to SWO
#define UART_PRINT                    // Print to the uart
#define USB_LOG                       // Save log to usb

//#define Unilink_Passive_Mode                             // Only sniff the bus, passive operation
#define Unilink_Log_Enable                            // Enable debug output
#define Unilink_Log_Detailed                        // Enable to append command description to each decoded frame
//#define Unilink_Log_Timestamp


#define answer_clk_timeout  10               // Timeout in ms waiting for master clock to send a response
#define byte_clk_timeout    2000            // Timeout in us between bytes in a frame
#define master_clk_timeout  2000            // Timeout in ms without master clock when idle
#define break_interval      4               // Don't flood the master with slave request too often? XXX: Maybe not required?

#define MAXDISCS            6                 // Max discs in the system


#if defined (Unilink_Passive_Mode) && defined (AUDIO_SUPPORT)
#undef AUDIO_SUPPORT
#endif

#if defined (Unilink_Passive_Mode) && !defined (Unilink_Log_Enable)
#define Unilink_Log_Enable
#endif


#endif /* INC_CONFIG_H_ */
