/*
 * audio.h
 *
 *  Created on: 8 mar. 2025
 *      Author: David
 */

#ifndef INC_AUDIOSRC_H_
#define INC_AUDIOSRC_H_

#include "main.h"

typedef enum {
    src_aux = 0, src_i2s = 2, src_bt = src_i2s|0, src_usb = src_i2s|1, src_auto=4
} audioSrc_t;
void monitorAudioSource(void);
void setAudioSource(audioSrc_t new_src);
audioSrc_t getAudioSource(void);

#endif /* INC_AUDIO_H_ */
