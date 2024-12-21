/*
 * c
 *
 *  Created on: Dec 23, 2020
 *      Author: David
 */
#include "serial.h"
#include "unilink_log.h"

#ifdef USB_LOG
#include "fatfs.h"
#include "i2sAudio.h"
#include "usb_host.h"
#endif

#ifdef Unilink_Log_Enable

#ifdef UART_PRINT
static bool init=0;
UART_HandleTypeDef* uart;
#endif

#ifdef USB_LOG
#define BFSZ   2048

char buf[2][BFSZ];
uint16_t bi, bp;
FIL logfile;
FRESULT res;
size_t logwritten;

void write_log(char* s, uint16_t len){
  size_t wr;
  static bool new;

  if(systemStatus.driveStatus!=drive_mounted) return;

  res = f_chdir("/");
  if(!new){
    new=1;
    res = f_open(&logfile, "log.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
  }
  else{
    res = f_open(&logfile, "log.txt", FA_OPEN_APPEND | FA_WRITE | FA_READ);
  }
  if(res!=FR_OK) systemStatus.driveStatus=drive_error;
  res = f_write(&logfile, s, len, &wr);
  if(res!=FR_OK || wr!=len) systemStatus.driveStatus=drive_error;
  else logwritten += BFSZ;
  res = f_close(&logfile);
  if(res!=FR_OK) systemStatus.driveStatus=drive_error;
}
#endif

int _write(int32_t file, uint8_t *ptr, int32_t len){
  int32_t l=len;
#ifdef UART_PRINT
  sendSerial(ptr, l);
#endif
  while(l--){

#ifdef USB_LOG
    buf[bp][bi++] = *ptr;
    if(bi>=1024){
      bi=0;
      write_log(buf[bp], BFSZ);
      if(++bp>1){
        bp=0;
      }
    }
#endif
#ifdef SWO_PRINT
    ITM_SendChar(*ptr++);
#endif
  }
  return len;
}

void initSerial(UART_HandleTypeDef* huart){
#ifdef UART_PRINT
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
#ifdef UART_PRINT
  if(!init){ return; }
  HAL_UART_Transmit(uart, ptr, len, 100);
#endif
}

#endif
