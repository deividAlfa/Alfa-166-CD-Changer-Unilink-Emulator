/*
 * serial.h
 *
 *  Created on: Dec 23, 2020
 *      Author: David
 */

#ifndef INC_SERIAL_H_
#define INC_SERIAL_H_
#include "main.h"


void initSerial(UART_HandleTypeDef* huart);
void putString(const char *str);
void sendSerial(uint8_t *ptr, uint32_t len);
void flush_log(void);
void handle_log(void);
#endif /* INC_SERIAL_H_ */
