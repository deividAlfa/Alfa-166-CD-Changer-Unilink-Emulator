/*
 * files.h
 *
 *  Created on: Dec 26, 2020
 *      Author: David
 */

#ifndef INC_FILES_H_
#define INC_FILES_H_
#include "main.h"


#define MAXFILES  100
#define FILETYPES 2
#define FOLDERS   MAXDISCS

typedef enum{
  drive_nodrive = 0,      // No drive on system
  drive_inserted,         // Drive was inserted
  drive_mounted,          // Drive was successfully mounted
  drive_ready,            // Drive is mounted and has files
  drive_nofiles,          // Drive mounted but no files
  drive_removed,          // Drive removed while mounted
  drive_error,            // Drive error
  drive_unmounted,        // Drive unmounted
}driveStatus_t;

typedef enum{
  file_none = 0,          // No file opened
  file_opened,            // File opened
  file_end,               // File reached end
}fileStatus_t;

typedef enum{
  file_mp3,               // .MP3
  file_wav                // .WAV
}filetype_t;

void handleFS(void);
uint8_t find_mp3 (char* path);
void scanFS(void);
void sortFS(void);
uint8_t openFile(void);
void closeFile(void);
void removeDrive(void);
#endif /* INC_FILES_H_ */

