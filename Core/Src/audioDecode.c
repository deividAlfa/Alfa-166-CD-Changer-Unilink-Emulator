/*
 * i2sAudio.c
 *
 *  Created on: Mar 21, 2021
 *      Author: David
 */
#include "audioDecode.h"
#include "files.h"
#include "mp3Decoder.h"
#include "wavDecoder.h"
#include "unilink.h"
#include "fatfs.h"
#include "serial.h"

#if defined AUDIO_SUPPORT

static I2S_HandleTypeDef *i2sHandle;
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

audioStruct_t AudioStruct;

result_t setDecoder(void *d){
    if(d==NULL) return ERR;
    AudioStruct.decoder=d;
    return OK;
}
void *getDecoder(void){
    return AudioStruct.decoder;
}
void freeDecoder(void){
    _free(AudioStruct.decoder);
    AudioStruct.decoder=NULL;
}

result_t setFillBuffer(fillBuffer_t * f){
    if(f==NULL) return ERR;
    AudioStruct.fillBuffer = f;
    return OK;
}
fillBuffer_t *getFillBuffer(void){
    return AudioStruct.fillBuffer;
}
void clearFillBuffer(void){
    AudioStruct.fillBuffer = NULL;
}

result_t setPCMbuffer(int16_t *b){
    if(b==NULL) return ERR;
    AudioStruct.PCMbuffer = b;
    return OK;
}
int16_t *getPCMbuffer(void){
    return AudioStruct.PCMbuffer;
}
void freePCMbuffer(void){
    _free(AudioStruct.PCMbuffer);
    AudioStruct.PCMbuffer=NULL;
}

void setPCMsamples(uint16_t s){
    AudioStruct.PCMSamples=s;
}
uint16_t getPCMsamples(void){
    return AudioStruct.PCMSamples;
}


void setAudioStatus(audioStatus_t s){
    AudioStruct.audioStatus = s;
}
audioStatus_t audio_status(void){
    return AudioStruct.audioStatus;
}

void initAudio(I2S_HandleTypeDef *hi2s) {
    i2sHandle = hi2s;
}

void handleAudio(void) {
    if (getAudioSource()==src_usb && usb_has_files()) {
        if (unilink_status() == unilink_playing) {
            if (audio_status() == audio_pause)
                AudioResume();
            else if (audio_status() != audio_play)
                AudioStart();
        }
        else if (unilink_status() == unilink_idle && audio_status() == audio_play)
                AudioPause();
    }
    if (AudioStruct.updateBuffer == 1) {
        AudioStruct.updateBuffer = 0;
        handleBuffer(0);                          // Refill first half
    }
    else if (AudioStruct.updateBuffer == 2) {
        AudioStruct.updateBuffer = 0;
        handleBuffer(AudioStruct.PCMSamples / 2);                // Refill last half
    }
    if (AudioStruct.underflow) {
        AudioStruct.underflow = 0;
        putString("AUDIO: Buffer underflow!\r\n");
    }/*
    if (AudioStruct.audioStatus != audio_play
        && getDriveStatus() == drive_ready
        && getFileStatus() == file_opened) {
        AudioStart();
    }
    if (AudioStruct.audioStatus == audio_play
        && unilink_status() != unilink_playing) {                // If unilink stopped and audio is playing, stop playback
        AudioStop();
    }
    */
}
#endif

void AudioStart(void) {
#ifdef AUDIO_SUPPORT
    if (getDriveStatus() != drive_ready)
        return;
    AudioStop();                            // Stop and clear everything
    sortFS();
    if (openFile() != FR_OK) {
        AudioStop();
        return;
    }
    switch (getFileType()){

        case file_mp3:
            AudioStruct.startDecoder = mp3Start;
            AudioStruct.stopDecoder = mp3Stop;
            break;

        case file_wav:
            AudioStruct.startDecoder = wavStart;
            AudioStruct.stopDecoder = wavStop;
            break;

        default:
            putString("AUDIO: Skipping unknown filetype!\r\n");
            AudioStop();
            return;
    }

    if (AudioStruct.startDecoder() == ERR) {
        AudioStop();
        return;
    }
    AudioStruct.remainingSamples = AudioStruct.fillBuffer(
        AudioStruct.PCMbuffer, AudioStruct.PCMSamples);                // Fill the entire buffer with data if no error

    if (AudioStruct.remainingSamples == 0) {                // If zero bytes transferred or error
        AudioStop();                                                 // Stop
        return;
    }
    else {
        if (AudioStruct.remainingSamples < AudioStruct.PCMSamples) {                // If less than 2048 bytes transferred
            setFileStatus(file_end);                // Reached the end
            padBuffer(
                &AudioStruct.PCMbuffer[AudioStruct.remainingSamples],
                AudioStruct.PCMSamples - AudioStruct.remainingSamples, 0);              // Fill the remaining data with silence
        }
        HAL_I2S_Transmit_DMA(i2sHandle, (uint16_t*) AudioStruct.PCMbuffer,
            AudioStruct.PCMSamples);                // Start I2S DMA
        AudioStruct.audioStatus = audio_play;               // Status = playing
    }
    putString("AUDIO: Playback started\r\n");
#endif
}

void AudioStop(void) {
#ifdef AUDIO_SUPPORT
    setFileStatus(file_end);

    if (AudioStruct.audioStatus == audio_play
        || AudioStruct.audioStatus == audio_pause) {
        HAL_I2S_DMAStop(i2sHandle);
        AudioStruct.audioStatus = audio_stop;                // Playback finished
        putString("AUDIO: Playback stopped\r\n\r\n");
    }
    if (AudioStruct.startDecoder && AudioStruct.stopDecoder) {
        AudioStruct.stopDecoder();
    }
    freeDecoder();
    freePCMbuffer();
    clearFillBuffer();
    closeFile();
#endif
}

void AudioNext(void) {
#ifdef AUDIO_SUPPORT
    AudioStop();
    unilink_next_track();

#ifndef PASSIVE_MODE
  unilink_reset_playback_time();
#endif
    AudioStart();
#endif
}
void AudioPause(void) {
#ifdef AUDIO_SUPPORT
    if (AudioStruct.audioStatus == audio_play) {
        HAL_I2S_DMAPause(i2sHandle);
        AudioStruct.audioStatus = audio_pause;                // Playback paused
        putString("AUDIO: Playback paused\r\n\r\n");
    }
#endif
}
void AudioResume(void) {
#ifdef AUDIO_SUPPORT
    if (AudioStruct.audioStatus == audio_pause) {
        AudioStruct.audioStatus = audio_play;                // Status = playing
        HAL_I2S_DMAResume(i2sHandle);
        putString("AUDIO: Playback resumed\r\n\r\n");
    }
#endif
}
#ifdef AUDIO_SUPPORT

void setAudioDecodeInfo(audioChannels_t ch, audioRate_t rate, audioBits_t bits){
    AudioStruct.channels = ch;
    AudioStruct.rate = rate;
    AudioStruct.bits = bits;
}

void padBuffer(int16_t *dest, int16_t data, uint16_t count) {
    uint32_t val = data;
    HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t) &val,
        (uint32_t) dest, count);
}

void handleBuffer(uint16_t offset) {
    if (getFileStatus() == file_opened) {               // File opened?

        uint32_t count = AudioStruct.fillBuffer(
            &AudioStruct.PCMbuffer[offset], AudioStruct.PCMSamples / 2);

        if (count < AudioStruct.PCMSamples / 2) {               // If less data than expected
            setFileStatus(file_end);                // File end reached
            AudioStruct.remainingSamples -= AudioStruct.PCMSamples / 2;             // Subtract 1/2 buffer count
            AudioStruct.remainingSamples += count;              // Add the remaining bytes
            padBuffer(&AudioStruct.PCMbuffer[offset + count], 0,
                (AudioStruct.PCMSamples / 2) - count);                // Fill the remaining data with silence
        }
    }
    else {              // File already reached end, so no more data to transfer
        if (AudioStruct.remainingSamples <= AudioStruct.PCMSamples / 2) {                // Remaining bytes less than 1/2 buffer?
            AudioNext();                                    // Done, next song
        }
        else {
            AudioStruct.remainingSamples -= AudioStruct.PCMSamples / 2;             // Buffer not done yet
        }
    }

}

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s != i2sHandle) {
        return;
    }
    if (AudioStruct.updateBuffer) {
        AudioStruct.underflow = 1;
    }
    AudioStruct.updateBuffer = 1;
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {


    if (hi2s != i2sHandle) {
        return;
    }
    if (AudioStruct.updateBuffer) {
        AudioStruct.underflow = 1;
    }
    AudioStruct.updateBuffer = 2;
}
#endif
