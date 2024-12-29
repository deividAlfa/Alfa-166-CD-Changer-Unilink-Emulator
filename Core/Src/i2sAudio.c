/*
 * i2sAudio.c
 *
 *  Created on: Mar 21, 2021
 *      Author: David
 */
#include "i2sAudio.h"
#include "files.h"
#include "mp3Decoder.h"
#include "wavDecoder.h"
#include "unilink.h"

#if defined AUDIO_SUPPORT || defined USB_LOG
#include "fatfs.h"
system_t systemStatus;
#endif

#if defined AUDIO_SUPPORT

static I2S_HandleTypeDef *i2sHandle;
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

void initAudio(I2S_HandleTypeDef *hi2s){
  i2sHandle = hi2s;
}

void handleAudio(void){
  if(systemStatus.updateBuffer==1){
    systemStatus.updateBuffer=0;
    handleBuffer(0);                          // Refill first half
  }
  else if(systemStatus.updateBuffer==2){
    systemStatus.updateBuffer=0;
    handleBuffer(systemStatus.PCMSamples/2);  // Refill last half
  }
  if(systemStatus.underflow){
    systemStatus.underflow=0;
    iprintf("AUDIO: Buffer underflow!\r\n");
  }
	if(systemStatus.audioStatus != audio_play && systemStatus.driveStatus == drive_ready && systemStatus.fileStatus == file_opened){
		AudioStart();
	}
	if(systemStatus.audioStatus == audio_play && unilink.status!=unilink_playing){      // If unilink stopped and audio is playing, stop playback
	  AudioStop();
	}
	if(systemStatus.audioStatus == audio_play){
	  /*
		static uint32_t btnTimer=0;
		static char btnPrev=1;
		char btnNow = HAL_GPIO_ReadPin(btn_GPIO_Port, btn_Pin);
		if(!btnNow && btnPrev && HAL_GetTick()-btnTimer > 200){
		  AudioStop();
			systemStatus.fileStatus = file_end;
		}
		if(btnPrev != btnNow){
		  btnPrev = btnNow;
			btnTimer = HAL_GetTick();
		}
		*/
	}
}
#endif


void AudioStart(void){
#ifdef AUDIO_SUPPORT
  if(systemStatus.driveStatus!=drive_ready)
    return;

  if(systemStatus.audioStatus==audio_stop || systemStatus.audioStatus==audio_idle){                         // If playback was stopped
    if( systemStatus.PCMbuffer || systemStatus.decoder ){           // If not empty, something went out of control
      AudioStop();                                                  // Stop and empty everything
    }

    if(unilink.lastDisc != unilink.disc){                           // Disc (folder) changed?
      unilink.lastDisc = unilink.disc;
      sortFS();                                                     // Sort files
    }
    if(openFile() != FR_OK){
      AudioStop();
      return;
    }
    switch((uint8_t)systemStatus.filetype){

      case file_mp3:
        systemStatus.startDecoder = mp3Start;
        systemStatus.stopDecoder = mp3Stop;
        break;

      case file_wav:
        systemStatus.startDecoder = wavStart;
        systemStatus.stopDecoder = wavStop;
        break;

      default:
        iprintf("AUDIO: Skipping unknown filetype!\r\n");
        AudioStop();
        return;
    }

    if(!systemStatus.startDecoder()){
      AudioStop();
      return;
    }
    systemStatus.remainingSamples = systemStatus.fillBuffer(systemStatus.PCMbuffer, systemStatus.PCMSamples);   // Fill the entire buffer with data if no error

    if(systemStatus.remainingSamples == 0 ){							        // If zero bytes transferred or error
      AudioStop();                                                      // Stop
      return;
    }
    else{
      if(systemStatus.remainingSamples < systemStatus.PCMSamples){					          // If less than 2048 bytes transferred
        systemStatus.fileStatus = file_end;							                // Reached the end
        padBuffer(&systemStatus.PCMbuffer[systemStatus.remainingSamples],\
            systemStatus.PCMSamples-systemStatus.remainingSamples, 0);		            // Fill the remaining data with silence
      }
      HAL_I2S_Transmit_DMA(i2sHandle, (uint16_t*)systemStatus.PCMbuffer, systemStatus.PCMSamples);   // Start I2S DMA
      systemStatus.audioStatus=audio_play;							                // Status = playing
    }
    iprintf("AUDIO: Playback started\r\n");
  }
  else if(systemStatus.audioStatus==audio_pause){
    systemStatus.audioStatus=audio_play;                              // Status = playing
    HAL_I2S_DMAResume(i2sHandle);
    iprintf("AUDIO: Playback resumed\r\n\r\n");
  }
#endif
}


void AudioStop(void){
#ifdef AUDIO_SUPPORT
  systemStatus.fileStatus = file_end;

  if(systemStatus.audioStatus==audio_play || systemStatus.audioStatus==audio_pause){
    HAL_I2S_DMAStop(i2sHandle);
    systemStatus.audioStatus = audio_stop;                // Playback finished
  }
  if(systemStatus.startDecoder && systemStatus.stopDecoder){
    systemStatus.stopDecoder();
  }
  if(systemStatus.decoder){
    _free(systemStatus.decoder);
    systemStatus.decoder = NULL;
  }
  if(systemStatus.PCMbuffer){
    _free(systemStatus.PCMbuffer);
    systemStatus.PCMbuffer = NULL;
  }
  closeFile();
#endif
	iprintf("AUDIO: Playback stopped\r\n\r\n");
}

void AudioNext(void){
#ifdef AUDIO_SUPPORT
  AudioStop();
  unilink.lastTrack = unilink.track;
  if(++unilink.track >= cd_data[unilink.disc-1].tracks){
    unilink.track=1;
  }
  unilink_reset_playback_time();
  AudioStart();
#endif
}
void AudioPause(void){
#ifdef AUDIO_SUPPORT
  if(systemStatus.audioStatus==audio_play){
    HAL_I2S_DMAPause(i2sHandle);
    systemStatus.audioStatus = audio_pause;                // Playback paused
    iprintf("AUDIO: Playback paused\r\n\r\n");
  }
#endif
}

#ifdef AUDIO_SUPPORT
void padBuffer(int16_t* dest, int16_t data, uint16_t count){
  uint32_t val = data;
  HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)&val, (uint32_t)dest, count);
}


void handleBuffer(uint16_t offset){
	if(systemStatus.fileStatus==file_opened){							// File opened?

	  uint32_t count = systemStatus.fillBuffer(&systemStatus.PCMbuffer[offset], systemStatus.PCMSamples/2);

		if(count<systemStatus.PCMSamples/2){											            // If less data than expected
			systemStatus.fileStatus=file_end;							                      // File end reached
			systemStatus.remainingSamples -= systemStatus.PCMSamples/2;					// Subtract 1/2 buffer count
			systemStatus.remainingSamples += count;						                  // Add the remaining bytes
			padBuffer(&systemStatus.PCMbuffer[offset+count], 0, (systemStatus.PCMSamples/2)-count);	// Fill the remaining data with silence
		}
	}
	else{																                                    // File already reached end, so no more data to transfer
		if(systemStatus.remainingSamples <= systemStatus.PCMSamples/2){				// Remaining bytes less than 1/2 buffer?
		  AudioNext();												                                // Done, next song
		}
		else{
			systemStatus.remainingSamples -= systemStatus.PCMSamples/2;					// Buffer not done yet
		}
	}

}


void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s){
  if(hi2s!=i2sHandle){ return; }
  if(systemStatus.updateBuffer){
    systemStatus.underflow=1;
  }
  systemStatus.updateBuffer=1;
}


void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s){
  if(hi2s!=i2sHandle){ return; }
  if(systemStatus.updateBuffer){
    systemStatus.underflow=1;
  }
  systemStatus.updateBuffer=2;
}
#endif
