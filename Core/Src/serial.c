/*
 * serial.c
 *
 *  Created on: Dec 23, 2020
 *      Author: David
 */
#include "serial.h"
#include "unilink_log.h"
#ifdef DebugLog
#ifdef EnableSerialLog
static bool init=0;
serial_t serial;
#endif


void initSerial(UART_HandleTypeDef* huart){
#ifdef EnableSerialLog
  serial.uart=huart;
  serial.uart->Init.BaudRate = 250000;
  HAL_UART_Init(serial.uart);
  init=1;
#endif
}

void putString(const char *str){
#ifdef EnableSerialLog
  uint16_t len=0;
  uint8_t tmp;
#endif
  if(*str){                                                 // If empty string, return
    iprintf(str);
#ifdef EnableSerialLog
    if(!init){ return; }
    //HAL_UART_DMAPause(serial.uart);                        // Pause DMA,  the interrupt could write at the same time and cause data loss
    //__HAL_DMA_DISABLE_IT(serial.uart->hdmatx,DMA_IT_TC);              // Disable DMA interrupt
    len=strlen(str);                                // Get string length

    tmp=HAL_UART_Transmit_DMA(serial.uart, (uint8_t *)str, len);          // Try to send data

    if(tmp==HAL_BUSY){                                    // If busy, try to store in queue
      if(serial.Pending>=SerFifoLen){                     // If queue full, discard data
        serial.LostinPutString++;                         // Increase lost counter (debugging)
        return;                                           // return
      }
      serial.Queue[serial.BfPos]=str;                     // Store string pointer
      serial.QueueLen[serial.BfPos]=len;                  // Store string length
      if(++serial.BfPos>=SerFifoLen){                     // Increase Buffer position(circular mode)
        serial.BfPos=0;                                   // If reached end, go to start
      }
      __disable_irq();
      serial.Pending++;                                   // Increase pending transfers
      __enable_irq();
    }
    else if(tmp==HAL_ERROR){                              // If error
      serial.errPutString++;                              // Increase error counter (debugging)
    }
    //__HAL_DMA_ENABLE_IT(serial.uart->hdmatx,DMA_IT_TC); // Enable DMA interrupt
    //HAL_UART_DMAResume(serial.uart);                    // Resume DMA
#endif
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
#ifdef EnableSerialLog
  if(!init){ return; }
  uint8_t tmp;
  if(huart==serial.uart){                                 // If it's our UART
    if(serial.Pending){                                   // If data pending

      tmp=HAL_UART_Transmit_DMA(  serial.uart,            // Transmit data
                    (uint8_t *)serial.Queue[serial.TxPos],
                    serial.QueueLen[serial.TxPos]);
      if(tmp==HAL_OK){                                    // If OK
        serial.Pending--;                                 // Decrease pending
        if(++serial.TxPos>=SerFifoLen){
          serial.TxPos=0;                                 // Increase TxPos
        }
      }
      else if(tmp==HAL_BUSY){                             // If busy
        serial.LostinCallback++;                          // Increase lost data counter
      }
      else if(tmp==HAL_ERROR){                            // If error
        serial.errCallback++;                             // Increase error counter
      }
    }
  }
#endif
}


#endif
