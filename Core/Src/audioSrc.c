/*
 * audio.c
 *
 *  Created on: 8 mar. 2025
 *      Author: David
 */

#include "audioSrc.h"
#include "audioDecode.h"
#include "unilink.h"
#include "serial.h"

audioSrc_t audioSource;
uint8_t aux_available=1, aux_available_last, usb_available;   // Preset aux to 1, so it automatically exits if the system resumes aux mode at boot but it's not connected anymore
uint32_t aux_timer;

void monitorAudioSource(void){
    uint32_t now = HAL_GetTick();

    audioSrc_t current = audioSource;
    uint8_t aux = !ReadPin(AUX_DET);

    if(!usb_available && usb_has_files()){
        usb_available = 1;
        if(now>5000 && current != src_usb)                      // Ignore source changes for the first 5 seconds (Settling down)
            setAudioSource(src_usb);
    }
    else if(usb_available && usb_has_files() == 0){
        usb_available = 0;
        if(current == src_usb)
            setAudioSource(src_bt);
    }

    if(aux_available_last != aux){
        aux_available_last = aux;
        aux_timer = HAL_GetTick();
    }
    else if (aux_available != aux_available_last && (HAL_GetTick()-aux_timer)>500){
        aux_available = aux_available_last;
        if(aux_available && current != src_aux)
            setAudioSource(src_aux);
        else if(!aux_available && current == src_aux)
            setAudioSource(src_auto);
    }
}

void setAudioSource(audioSrc_t new_src) {
    audioSrc_t current = audioSource;

    if(new_src==src_auto){                                  // Auto: Switched from ICS, select next available source
        if(current==src_aux){
            if(usb_available){
                audioSource = src_usb;
            }
            else
                audioSource = src_bt;
        }
        else if(current==src_usb){
            unilink_backup_usb_position();
            audioSource = src_bt;
        }
        else if(current==src_bt){
            if(aux_available)                     // Aux inserted
                audioSource = src_aux;
            else if(usb_available){
                audioSource = src_usb;
            }
        }
    }
    else
        audioSource = new_src;

    if(audioSource == src_usb)                          // Switch to USB, update files
        updateFiles();
    unilink_update_magazine();                          // Update magazine in
    WritePin(AUX_EN, (audioSource == src_aux));
    WritePin(I2S_SEL, (audioSource == src_usb));

    putString("Switch audio source: ");
    if(audioSource==src_aux)
        putString("Aux-in");
    else if(audioSource==src_bt)
        putString("Bluetooth");
    else if(audioSource==src_usb)
        putString("USB");
    putString("\r\n");
}

audioSrc_t getAudioSource(void) {
    return audioSource;
}

uint8_t dac_muted;
void dac_mute(void){
    SetPinLow(I2S_MUTE);
    putString("DAC Muted\r\n");
    dac_muted=1;
}
void dac_unmute(void){
    SetPinHigh(I2S_MUTE);
    putString("DAC Unmuted\r\n");
    dac_muted=0;
}
uint8_t is_dac_muted(void){
    return dac_muted;
}
