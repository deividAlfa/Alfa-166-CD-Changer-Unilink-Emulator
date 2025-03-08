/*
 * audioDecode.h
 *
 *  Created on: Mar 21, 2021
 *      Author: David
 */

#ifndef INC_AUDIODECODE_H_
#define INC_AUDIODECODE_H_
#include "main.h"
#include "audioSrc.h"

#if defined AUDIO_SUPPORT || defined USB_LOG
#include "FATFS.h"
#endif

typedef enum {
    audio_idle = 0,                 // Audio idle after boot
    audio_play,                // Audio is playing
    audio_pause,                // Audio is paused
    audio_stop,                         // Audio was stopped after playing
} audioStatus_t;

typedef enum {
    audio_mono,
    audio_stereo,
} audioChannels_t;

typedef enum {
    audio_8bit,                         // Standard = 8 bit unsigned
    audio_16bit,                        // Standard = 16 bit signed
} audioBits_t;

typedef enum {
    audio_8KHz,
    audio_16KHz,
    audio_22KHz,
    audio_32KHz,
    audio_44KHz,
    audio_48KHz,
    audio_96KHz
} audioRate_t;

//typedef return_type (*alias_name)(parameter_types and numbers....);
typedef  uint32_t (fillBuffer_t)(int16_t *dest, uint16_t samples);

typedef struct {
    fillBuffer_t *fillBuffer;
    void *decoder;                // Pointer to decoder
    result_t (*startDecoder)(void);                // Pointer to stop decoder function
    void (*stopDecoder)(void);                // Pointer to stop decoder function
    int16_t *PCMbuffer;                // Pointer to PCM buffer
    uint16_t PCMSamples;                // Buffer capacity
    volatile uint8_t updateBuffer;                // To tell the handler to add new data to the buffer
    volatile uint8_t underflow;
    uint16_t remainingSamples;                // Remaining samples in the PCM buffer
    audioBits_t bits;                // 8Bits, 16Bits
    audioChannels_t channels;                // Mono, Stereo
    audioRate_t rate;                // 16KHz, 32KHz, 48KHz
    audioStatus_t audioStatus;                // Idle, Playing, Stopped(after playing)
} audioStruct_t;


audioStatus_t getAudioStatus(void);
void setAudioStatus(audioStatus_t s);

result_t setDecoder(void *d);
void *getDecoder(void);
void freeDecoder(void);

result_t setFillBuffer(fillBuffer_t * f);
fillBuffer_t *getFillBuffer(void);

result_t setPCMbuffer(int16_t *b);
int16_t *getPCMbuffer(void);
void freePCMbuffer(void);

void setPCMsamples(uint16_t s);
uint16_t getPCMsamples(void);

void initAudio(I2S_HandleTypeDef *hi2s);
void padBuffer(int16_t *dest, int16_t data, uint16_t count);
void handleBuffer(uint16_t offset);
void handleAudio(void);
void AudioStop(void);
void AudioStart(void);
void AudioNext(void);
void AudioPause(void);
void AudioResume(void);
void setAudioDecodeInfo(audioChannels_t ch, audioRate_t rate, audioBits_t bits);

#endif /* INC_PWMAUDIO_H_ */
