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
#include "files.h"
#include "i2sAudio.h"
#include "usb_host.h"
#endif

#ifdef Unilink_Log_Enable

#ifdef UART_PRINT
UART_HandleTypeDef* uart;
bool uart_init;
#endif

#ifdef USB_LOG
#define BFSZ   512

uint8_t buf[2][BFSZ];
uint16_t cnt;
uint8_t curr_bf;
bool opened;
volatile uint8_t wr_log;
volatile uint8_t wr_log_bf;
volatile uint16_t wr_log_cnt;

FIL logfile;
FRESULT res;
size_t logwritten;
uint32_t last_log_time;

void write_log(void);


void flush_log(void){
  handle_log();                 // Check if there's a pending write

  if(cnt){                      // Flush current buffer
    wr_log_bf=curr_bf;
    wr_log_cnt=cnt;
    wr_log=1;
    handle_log();
  }
  f_close(&logfile);
}

void handle_log(void){
  if(wr_log){
    write_log();
    wr_log=0;
  }
}

void write_log(void){
  size_t wr;
  last_log_time = HAL_GetTick();

  if(systemStatus.driveStatus!=drive_ready) return;

  res = f_chdir("/");
  if(!opened){
    opened=1;
    res = f_open(&logfile, "/log.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
  }
  if(res!=FR_OK) systemStatus.driveStatus=drive_error;
  res = f_write(&logfile, buf[wr_log_bf], wr_log_cnt, &wr);
  if(res!=FR_OK || wr!=wr_log_cnt) systemStatus.driveStatus=drive_error;
}
#endif

int _write(int32_t file, uint8_t *ptr, int32_t len){
  int32_t l=len;
#ifdef UART_PRINT
  sendSerial(ptr, l);
#endif
  while(l--){

#ifdef USB_LOG
    buf[curr_bf][cnt++] = *ptr;
    if(cnt==BFSZ){
      wr_log_bf=curr_bf;
      wr_log_cnt=BFSZ;
      wr_log=1;
      cnt=0;
      if(++curr_bf>1){
        curr_bf=0;
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
  uart_init=1;
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
  if(!uart_init){ return; }
  HAL_UART_Transmit(uart, ptr, len, 100);
#endif
}

#endif
