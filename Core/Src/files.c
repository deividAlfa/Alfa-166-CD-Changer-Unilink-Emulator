    /*
 * files.c
 *
 *  Created on: Dec 26, 2020
 *      Author: David
 */

#include "files.h"
#include <time.h>
#include "fatfs.h"
#include "spiritMP3Dec.h"
#include "audioDecode.h"
#include "serial.h"


DIR dir; /* Directory object */
FILINFO fil; /* File information */
FATFS *fat;                // Pointer to FAT
FIL *file;                // Pointer to file
DWORD clmt[32];

char fileList[MAXFILES][13];
const char *filetypes[FILETYPES] = { "*.MP3", "*.WAV" };                // Files we are interested in
const char *folders[FOLDERS] = { "/CD01", "/CD02", "/CD03", "/CD04", "/CD05","/CD06" };                // Folder names

extern FIL USBHFile;
extern FATFS USBHFatFS;
fileStruct_t FileStruct;

void initFS(void) {
    file = &USBHFile;
    fat = &USBHFatFS;
}

void removeDrive(void) {
    f_mount(0, "", 1);
    setDriveStatus(drive_removed);
}

result_t gen_usb_discinfo(void){
    result_t have_files = ERR;
    for (uint8_t i = 0; i < FOLDERS; i++) {                // Transfer file count to cd info (for unilink)
        if (FileStruct.fileCount[i]) {
            cd_data[i].tracks = FileStruct.fileCount[i];
            cd_data[i].mins = 99;
            cd_data[i].secs = 59;
            cd_data[i].inserted = 1;
            have_files = OK;
        }
    }
    return have_files;
}
result_t usb_has_files(void){
    return FileStruct.usb_has_files;
}
void handleFS(void) {
    FRESULT res;
    if (getDriveStatus() == drive_inserted) {                // Drive present
        for (uint8_t i = 0; i < 10; i++) {
            res = f_mount(fat, "", 1);
            if (res == FR_OK)
                break;
        }
        if (res != FR_OK) {
            iprintf("SYSTEM: Failed to mount volume\r\n");
            setDriveStatus(drive_error);                //Failure on mount
        }
        else {
            iprintf("SYSTEM: Volume mounted\r\n");
            for (uint8_t i = 0; i < 10; i++) {
                res = f_chdir("/");
                if (res == FR_OK)
                    break;
            }
            if (res != FR_OK) {
                iprintf("SYSTEM: Failed to open root dir\r\n");
                setDriveStatus(drive_error);                //Failure on mount
            }
            else {
                iprintf("SYSTEM: Opened root folder\r\n");
                setDriveStatus(drive_mounted);
            }
        }
    }
    if (getDriveStatus() == drive_mounted) {
        setDriveStatus(drive_ready);
#ifdef USB_LOG
    reset_usb_log();
#endif
        setFileStatus(file_none);           // FIXME: Handle file list update (
        scanFS();                           // Find and count available folders/files // XXX: Filenames are not obtained yet, done in SortFS()
        unilink_clear_backup_position();    // New usb, clear existing backup
        if(getAudioSource() == src_usb)
            unilink_update_magazine();          // Update magazine so it matches the scan results
    }

    if ((getDriveStatus() == drive_error)
        || (getDriveStatus() == drive_removed)) {                // if drive removed or error

        if (getDriveStatus() == drive_error)
            iprintf("SYSTEM: Drive error!\r\n");

        iprintf("SYSTEM: Removing mounting point\r\n");
        f_mount(0, "", 1);                              // remove mount point
        setDriveStatus(drive_nodrive);
        if(getAudioSource() == src_usb){
            AudioStop();
            unilink_update_magazine();
        }
    }
}

void iprintfiles(void) {
    iprintf("File list:\r\n");
    for (uint8_t t = 0; t < FileStruct.fileCount[unilink_disc()- 1]; t++) {
        iprintf("\t%s\r\n", fileList[t]);
    }
    iprintf("\r\n\r\n");
}

void sortFS(void) {
    if (FileStruct.fileCount[unilink_disc()- 1] == 0 ||
        FileStruct.files_sorted == 1)
        return;

    FRESULT res = FR_OK;
    char temp[13];
    uint32_t Min;
    uint8_t c = 0;
    strcpy(FileStruct.lastFolder, folders[unilink_disc()- 1]);
    for (uint8_t t = 0; t < FILETYPES; t++) {
        res = f_findfirst(&dir, &fil, folders[unilink_disc()- 1], filetypes[t]);                // Find first file of the current type in current folder (unilink disc)
        if (res != FR_OK) {
            continue;
        }                              // If not OK, continue with next filetype
        while (fil.fname[0] && c < FileStruct.fileCount[unilink_disc()- 1]) {                // Stop when no file found, last file or exceeded max file count
#if   (_USE_LFN)
      strcpy(fileList[c],fil.altname);                                          // Copy filename string
#else
            strcpy(fileList[c], fil.fname);                // Copy filename string
#endif
            f_findnext(&dir, &fil);                            // Find next file
            c++;
        }
    }

    for (uint32_t j = 0; j < FileStruct.fileCount[unilink_disc()- 1] - 1;
        j++) {                // Sort alphabetically
        Min = j;
        for (uint32_t i = j + 1; i < FileStruct.fileCount[unilink_disc()- 1];
            i++) {
            if (strcmp(fileList[Min], fileList[i]) > 0) {                // List[Min] > List[i]
                Min = i;
            }
        }
        if (Min != j) {
            strcpy(temp, fileList[j]);
            strcpy(fileList[j], fileList[Min]);
            strcpy(fileList[Min], temp);
        }
    }
    FileStruct.files_sorted = 1;
}

void scanFS(void) {
    if(getDriveStatus() != drive_ready) return;
    uint8_t c, i = 0;

    FileStruct.usb_has_files = 0;
    while (i < FOLDERS) {
        FileStruct.fileCount[i] = 0;                        // Clear old value
        c = 0;
        for (uint8_t t = 0; t < FILETYPES; t++) {
            f_findfirst(&dir, &fil, folders[i], filetypes[t]);                // Find first file of the current type
            while (fil.fname[0] && c < MAXFILES) {                // Stop when no file found, last file or exceeded max file count
                f_findnext(&dir, &fil);                        // Find next file
                c++;
            }
        }
        FileStruct.fileCount[i] = c;                 // Store found file count
        if(c) FileStruct.usb_has_files = 1;
        iprintf("%s: Found %3d files\r\n", folders[i], c);                // Debug number of files found in folder
        i++;
    }
}

uint8_t openFile(void) {
    if (getDriveStatus() != drive_ready) {
        return FR_NOT_READY;
    }
    FileStruct.lastFile[0] = 0;

    if (fileList[unilink_track()- 1][0]) {                // Detect file extension
        char currentType[6] = "*.\0\0\0";                     // Empty extension
        char *namePtr = (char*) fileList[unilink_track()- 1];                // Point to current filename
        while (*(namePtr + 3)) {
            namePtr++;
        }                          // Find last char
        currentType[2] = *namePtr++;                           // Copy extension
        currentType[3] = *namePtr++;
        currentType[4] = *namePtr;

        for (uint8_t t = 0; t <= FILETYPES; t++) {                    // Compare
            if (t >= FILETYPES) {
                return 0;
            }                    // Exceeded filetypes (Unknown file extension!)
#if   (_USE_LFN)
      if(stricmp(currentType,  filetypes[t])==0){
#else
            if (strcmp(currentType, filetypes[t]) == 0) {
#endif
                if (t == file_mp3) {
                    setFileType(file_mp3);
                    break;
                }
                else if (t == file_wav) {
                    setFileType(file_wav);
                    break;
                }
            }
        }
    }
    if (f_chdir(folders[unilink_disc()- 1]) != FR_OK) {                // Change dir
        iprintf("Error opening folder \"%s\"\r\n", folders[unilink_disc()- 1]);
        return FR_DISK_ERR;
    }
    if (f_open(file, (char*) fileList[unilink_track()- 1], FA_READ)
        != FR_OK) {                // Open file
        iprintf("SYSTEM: Error opening file\r\n");
        setDriveStatus(drive_error);                      // No files
        return FR_DISK_ERR;
    }

    clmt[0] = 32;                                              // Set table size
    file->cltbl = clmt;                // Enable fast seek feature (cltbl != NULL)

    if (f_lseek(file, CREATE_LINKMAP) != FR_OK) {                // Create CLMT
        f_close(file);
        iprintf("SYSTEM: FatFS Seek error\r\n");
        return FR_DISK_ERR;
    }
#if   (_USE_LFN)
  f_findfirst(&dir, &fil, folders[unilink_disc()-1], "*.*");      // Find first file of the current type
  while(fil.fname[0] && strcmp(fil.altname, (char *)fileList[unilink_track()-1])!=0){                    // Stop when no file found, last file or exceeded max file count
    f_findnext(&dir, &fil);                                     // Find next file
  }
  iprintf("SYSTEM: Opened file: %s\r\n", fil.fname);
#else
    iprintf("SYSTEM: Opened file: %s\r\n", (char*) fileList[unilink_track()- 1]);
#endif
    setAudioDecodeInfo(audio_stereo, audio_44KHz, audio_16bit);
    setFileStatus(file_opened);                         // Valid file
    strcpy(FileStruct.lastFile, fileList[unilink_track()- 1]);
    return FR_OK;
}

void closeFile(void) {
    f_close(file);
}

void *getFile(void){
    return file;
}

void updateFiles(void) {
    FileStruct.files_sorted = 0;
}
void setDriveStatus(driveStatus_t s){
    FileStruct.driveStatus = s;
}
driveStatus_t getDriveStatus(void){
    return FileStruct.driveStatus;
}

void setFileStatus(fileStatus_t s){
    FileStruct.fileStatus = s;
}
fileStatus_t getFileStatus(void){
    return FileStruct.fileStatus;
}

void setFileType(filetype_t t){
    FileStruct.filetype = t;
}
filetype_t getFileType(void){
    return FileStruct.filetype;
}
/*
 uint8_t restorelast(void){                      // FIXME: Needed? Does the ICS remember disc and track?
 #define SZ_TBL  8
 FRESULT res;
 DIR dp;
 FILINFO fno;
 FIL fp;
 DWORD clmt[SZ_TBL];

 //strcpy(FileStruct.lastPath,"/CD01");
 //strcpy(FileStruct.lastFile,"1.TXT");

 if((FileStruct.lastPath[0]!=0)&&(FileStruct.lastFile[0]!=0)){   // If stored path and file
 iprintf("Stored path: \"%s\"\r\n",FileStruct.lastPath);
 iprintf("Stored file: \"%s\"\r\n",FileStruct.lastFile);
 }
 else if( (FileStruct.lastFile[0]==0)||(FileStruct.lastPath[0]==0)){ // If any not stored, reset stored data
 FileStruct.lastFile[0]=0;
 FileStruct.lastPath[0]=0;
 iprintf("No previous stored file\r\n");
 uint8_t i=0;
 while(i<6){
 if(fileCnt[i]){                  // search first CD folder with files
 strcpy(FileStruct.lastPath,paths[i]);           // store folder path
 iprintf("Found %3d files in \"%s\"\r\n",fileCnt[i],paths[i]);
 break;
 }
 i++;
 }
 if(FileStruct.lastPath[0]==0){
 iprintf("No files found in filesystem");           // No compatible files in the filesystem
 return FR_DISK_ERR;
 }
 }
 res = f_chdir(FileStruct.lastPath);               // Change dir
 if(res){
 iprintf("Error opening folder \"%s\"\r\n",FileStruct.lastPath);
 return FR_DISK_ERR;
 }
 if(FileStruct.lastFile[0]!=0){
 res = f_open(&fp, FileStruct.lastFile, FA_READ);        // Open stored file
 if(res==FR_OK){

 iprintf("Opened File: \"%s\"\r\n",FileStruct.lastFile);
 fp.cltbl = clmt;
 clmt[0] = SZ_TBL;
 res = f_lseek(&fp, CREATE_LINKMAP);
 if(res){
 iprintf("Error creating linkmap\r\n");
 return FR_DISK_ERR;
 }
 iprintf("Linkmap file created\r\n");
 return FR_OK;                       // OK, done here
 }
 else{
 iprintf("Error opening \"%s\"\r\n",FileStruct.lastFile);   // Error reading stored filenamew
 }
 }
 res = f_findfirst(&dp, &fno, FileStruct.lastPath, "*.MP3");       // Search first mp3 file
 if(res!=FR_OK){
 iprintf("Error: No files found\r\n");                // No files found
 return FR_DISK_ERR;                       // We should have files from previous scan
 }
 strcpy(FileStruct.lastFile,fno.fname);              // OK, store filename
 iprintf("Found  File: \"%s\"\r\n",fno.fname);
 res = f_open(&fp, FileStruct.lastFile, FA_READ);          // Open stored file
 if(res==FR_OK){
 iprintf("Opened File: \"%s\"\r\n",FileStruct.lastFile);
 return FR_OK;
 }
 iprintf("Error opening \"%s\"\r\n",FileStruct.lastFile);   // Error
 return FR_DISK_ERR;
 }
 */
