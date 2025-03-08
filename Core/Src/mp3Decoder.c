/*
 * mp3Decoder.c
 *
 *  Created on: Mar 21, 2021
 *      Author: David
 */
#include "main.h"
#include "audioDecode.h"
#include "mp3Decoder.h"
#include "spiritMP3Dec.h"
#if defined AUDIO_SUPPORT
#include "fatfs.h"

ID3TAG_t ID3TAG;

int unsynchsafe(uint32_t in) {
    int out = 0, mask = 0x7F000000;
    in = ((in >> 24) | ((in >> 8) & 0xFF00ul) | ((in << 8) & 0xFF0000ul)
        | (in << 24));
    while (mask) {
        out >>= 1;
        out |= in & mask;
        mask >>= 8;
    }
    return out;
}

result_t readID3(void) {
    UINT count = 0;
    if (f_read(getFile(), &ID3TAG, 10, &count) != FR_OK) {
        return 0;
    }
    if ((ID3TAG.id[0] != 'I') || (ID3TAG.id[1] != 'D')
        || (ID3TAG.id[2] != '3')) {
        iprintf("MP3: ID3Tag not found\n");
        f_lseek(getFile(), 0);                  // Seek to start of file
        return 0;
    }
    ID3TAG.size = (uint32_t) ID3TAG.size_[3] << 24
        | (uint32_t) ID3TAG.size_[2] << 16 | (uint32_t) ID3TAG.size_[1] << 8
        | (uint32_t) ID3TAG.size_[0];
    ID3TAG.size = unsynchsafe(ID3TAG.size);
    f_lseek(getFile(), ID3TAG.size + 10);                // We skip ID3 for now.
    return 1;
}

// Starts mp3 decoder
result_t mp3Start(void) {
    readID3();
    if(setDecoder(_calloc(1, sizeof(TSpiritMP3Decoder))) != OK){
        iprintf("MP3: Error allocating decoder!\n");
        return ERR;
    }

    if(setPCMbuffer(_malloc(MP3_PCM_Samples * 2)) != OK){                // *2 because 1sample = 16bits
        iprintf("MP3: Error allocating Buffer!\n");
        freeDecoder();
        return ERR;
    }
    setFillBuffer(mp3FillBuffer);
    setPCMsamples(MP3_PCM_Samples);
    SpiritMP3DecoderInit(getDecoder(), RetrieveMP3Data, NULL, NULL);
    return OK;
}

// Stops mp3 decoder
void mp3Stop(void) {

}

// Callback function, reads MP3 data from the file.
unsigned int RetrieveMP3Data(void *pMP3CompressedData,
    unsigned int nMP3DataSizeInChars, void *token) {
    UINT count = 0;
    if (f_read(getFile(), (uint8_t*) pMP3CompressedData,
        nMP3DataSizeInChars, &count) != FR_OK) {
        return -1;
    }
    return count;
}

// Callback function, transfers PCM samples to the audio buffer
uint32_t mp3FillBuffer(int16_t *dest, uint16_t samples) {
    return (SpiritMP3Decode(getDecoder(), (int16_t*) dest, samples / 2,
        NULL) * 2);                // 1 MP3 sample outputs 2 PCM samples (stereo)
}
#endif
