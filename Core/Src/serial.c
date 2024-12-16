/*
 * c
 *
 *  Created on: Dec 23, 2020
 *      Author: David
 */
#include "serial.h"
#include "unilink_log.h"

#ifdef DebugLog

#ifdef EnableSerialLog
static bool init=0;
UART_HandleTypeDef* uart;
#endif

int _write(int32_t file, uint8_t *ptr, int32_t len){
  int32_t l=len;
  sendSerial(ptr, l);
#ifdef SWO_PRINT
  while(l--){
    ITM_SendChar(*ptr++);
  }
#endif
  return len;
}

void initSerial(UART_HandleTypeDef* huart){
#ifdef EnableSerialLog
  uart=huart;
  uart->Init.BaudRate = 1000000;
  HAL_UART_Init(uart);
  init=1;
#endif
}

void putString(const char *str){
  if(*str){                                                 // If empty string, return
    uint32_t l = strlen(str);
    _write(0, (uint8_t*)str, l);                                                        // XXX removed printf so it redirects to _write and SWO
  }
}

void sendSerial(uint8_t *ptr, uint32_t len){
#ifdef EnableSerialLog
  if(!init){ return; }
  HAL_UART_Transmit(uart, ptr, len, 100);
#endif
}

#endif
