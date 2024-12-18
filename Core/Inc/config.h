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


#define SWO_PRINT                       // Print to SWO
//#define UART_PRINT                    // Print to the uart


//#define Unilink_Passive_Mode                             // Only sniff the bus, passive operation
#define Unilink_Log_Enable                            // Enable debug output
#define Unilink_Log_Detailed                        // Enable to append command description to each decoded frame
//#define Unilink_Log_Timestamp


/* CRITICAL TIMING */
#define answer_clk_timeout  10               // Timeout in ms waiting for master clock to send a response
#define byte_clk_timeout    2000            // Timeout in us between bytes in a frame
#define master_clk_timeout  2000            // Timeout in ms without master clock when idle
#define break_interval      4
#define MAXDISCS            6                 // Max discs in the system

#endif /* INC_CONFIG_H_ */
