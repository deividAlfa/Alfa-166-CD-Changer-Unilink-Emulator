/*
 * i2sAudio.h
 *
 *  Created on: Mar 21, 2021
 *      Author: David
 */

#ifndef INC_I2SAUDIO_H_
#define INC_I2SAUDIO_H_
#include "main.h"
#include "files.h"

#if defined AUDIO_SUPPORT || defined USB_LOG
#include "FATFS.h"
#endif

typedef enum{
	audio_idle = 0,					// Audio idle after boot
  audio_play,             // Audio is playing
  audio_pause,            // Audio is paused
	audio_stop,							// Audio was stopped after playing

	audio_mono,
	audio_stereo,

	audio_8bit,							// Standard = 8 bit unsigned
	audio_16bit,						// Standard = 16 bit signed

	audio_8KHz,
	audio_16KHz,
	audio_22KHz,
	audio_32KHz,
	audio_44KHz,
	audio_48KHz,
	audio_96KHz

}audioStatus_t;


typedef struct{
  uint8_t         unilink_change;     // Flag to indicate unilink track/disc change was received
  uint8_t         currentFolder;      // Current folder number
  uint8_t         currentFile;        // Current file number
#if defined AUDIO_SUPPORT || defined USB_LOG
	driveStatus_t	 	driveStatus; 		    // 0 = no drive, 1=mounted, 2=scanned
	fileStatus_t		fileStatus; 		    // 0 = No file, 1 = File opened, 2 = File end reached
	filetype_t      filetype;           // File status
	FATFS           *fat;               // Pointer to FAT
	FIL             *file;              // Pointer to file
  uint32_t        lastFsSize;
  char            lastFolder[6];      // Ex. "/CD01"
  char            lastFile[13];       // Ex "SONG01~1.MP3"
  uint8_t         fileCount[FOLDERS]; // File count for each folder

  uint32_t        (*fillBuffer)(int16_t* dest, uint16_t samples);
	void            *decoder;           // Pointer to decoder
	uint8_t         (*startDecoder)(void);// Pointer to stop decoder function
	void            (*stopDecoder)(void);// Pointer to stop decoder function
  int16_t         *PCMbuffer;         // Pointer to PCM buffer
	uint16_t        PCMSamples;         // Buffer capacity
	volatile uint8_t updateBuffer;      // To tell the handler to add new data to the buffer
	volatile uint8_t underflow;
	uint16_t			  remainingSamples;	  // Remaining samples in the PCM buffer
	audioStatus_t		audioBits;			    // 8Bits, 16Bits
	audioStatus_t		audioChannels;		  // Mono, Stereo
	audioStatus_t		audioRate;			    // 16KHz, 32KHz, 48KHz
#endif
	audioStatus_t		audioStatus;		    // Idle, Playing, Stopped(after playing)
}system_t;

extern system_t systemStatus;

#if defined AUDIO_SUPPORT
void initAudio(I2S_HandleTypeDef *hi2s);
void padBuffer(int16_t* dest, int16_t data, uint16_t count);
void handleBuffer(uint16_t offset);
void handleAudio(void);
void AudioStop(void);
void AudioStart(void);
void AudioNext(void);
void AudioPause(void);
#endif



#endif /* INC_PWMAUDIO_H_ */
