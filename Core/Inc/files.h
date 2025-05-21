/*
 * files.h
 *
 *  Created on: Dec 26, 2020
 *      Author: David
 */

#ifndef INC_FILES_H_
#define INC_FILES_H_
#include "main.h"

#define _MAXFILES_  98            // The original charger reports 99 tracks for empty discs
#define _FILETYPES_ 2
#define _FOLDERS_   _DISCS_

typedef enum {
    drive_nodrive = 0,                // No drive on system
    drive_inserted,                // Drive was inserted
    drive_mounted,                // Drive was successfully mounted
    drive_scanning,                // Drive was successfully mounted
    drive_ready,                // Drive is mounted and has files
    drive_nofiles,                // Drive mounted but no files
    drive_removed,                // Drive removed while mounted
    drive_error,                // Drive error
    drive_unmounted,                // Drive unmounted
} driveStatus_t;

typedef enum {
    file_none = 0,                // No file opened
    file_opened,                // File opened
    file_end,                // File reached end
} fileStatus_t;

typedef enum {
    file_mp3,                // .MP3
    file_wav                // .WAV
} filetype_t;

typedef struct {
    uint8_t scan_folder;
    uint8_t files_sorted;                // Flag indicating the current folder is sorted
    result_t usb_has_files;
    driveStatus_t driveStatus;
    fileStatus_t fileStatus;
    filetype_t filetype;                // File status
    //uint32_t lastFsSize;
    char lastFolder[6];                // Ex. "/CD01"
    char lastFile[13];                // Ex "SONG01~1.MP3"
    uint8_t fileCount[_FOLDERS_];                // File count for each folder
} fileStruct_t;

void initFS(void);
void handleFS(void);
uint8_t find_mp3(char *path);
result_t gen_usb_discinfo(void);
uint8_t usb_has_files(void);
void updateFiles(void);
void sortFS(void);
uint8_t openFile(void);
void closeFile(void);
void *getFile(void);            // FIXME: Using FIL* requires including "ff.h", which causes havoc in STM32_USB_Host_Library
void removeDrive(void);
void remountDrive(void);
void setDriveStatus(driveStatus_t s);
driveStatus_t getDriveStatus(void);
void setFileStatus(fileStatus_t s);
fileStatus_t getFileStatus(void);
void setFileType(filetype_t t);
filetype_t getFileType(void);
#endif /* INC_FILES_H_ */

