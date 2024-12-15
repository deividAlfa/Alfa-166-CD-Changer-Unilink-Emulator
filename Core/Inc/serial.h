/*
 * serial.h
 *
 *  Created on: Dec 23, 2020
 *      Author: David
 */

#ifndef INC_SERIAL_H_
#define INC_SERIAL_H_
#include "main.h"
#include "unilink.h"

#define SerFifoLen  128

typedef struct{
  volatile uint16_t LostinPutString;
  volatile uint16_t LostinCallback;
  volatile uint16_t errPutString;
  volatile uint16_t errCallback;
  volatile const char *      Queue[SerFifoLen];
  volatile uint8_t  QueueLen[SerFifoLen];
  volatile uint8_t  TxPos;
  volatile uint8_t  BfPos;
  volatile uint8_t  Pending;
  UART_HandleTypeDef* uart;
}serial_t;


void initSerial(UART_HandleTypeDef* huart);
void putString(const char *str);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
#endif /* INC_SERIAL_H_ */
