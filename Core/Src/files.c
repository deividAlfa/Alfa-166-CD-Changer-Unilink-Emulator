/*
 * files.c
 *
 *  Created on: Dec 26, 2020
 *      Author: David
 */


#include "files.h"
#include <time.h>
#if defined AUDIO_SUPPORT || defined USB_LOG
#include "fatfs.h"
#include "spiritMP3Dec.h"
#include "i2sAudio.h"
#endif


#ifdef AUDIO_SUPPORT
DIR dir;         /* Directory object */
FILINFO fil;    /* File information */
DWORD clmt[32];

extern system_t systemStatus;
char fileList[MAXFILES][13];
const char *filetypes[FILETYPES] = { "*.MP3", "*.WAV" };                                    // Files we are interested in
const char *folders[FOLDERS] = { "/CD01", "/CD02", "/CD03", "/CD04", "/CD05", "/CD06" };    // Folder names
#endif

#if defined AUDIO_SUPPORT || defined USB_LOG

void removeDrive(void){
  f_mount(0, "", 1);
}

void handleFS(void){
  FRESULT res;
  if(systemStatus.driveStatus==drive_inserted){           // Drive present
    for(uint8_t i=0;i<10;i++){
      res = f_mount(systemStatus.fat, "", 1);
      if(res==FR_OK) break;
    }
    if(res!=FR_OK){
        iprintf("SYSTEM: Failed to mount volume\r\n");
        systemStatus.driveStatus=drive_error;           //Failure on mount
    }
    else{
      for(uint8_t i=0;i<10;i++){
        res = f_chdir("/");
        if(res==FR_OK) break;
      }
      if(res!=FR_OK){
         iprintf("SYSTEM: Failed to open root dir\r\n");
         systemStatus.driveStatus=drive_error;           //Failure on mount
      }
      else{
        iprintf("SYSTEM: Drive mounted\r\n");
        systemStatus.driveStatus=drive_mounted;
      }
    }
  }
#ifdef AUDIO_SUPPORT
  if(systemStatus.driveStatus==drive_mounted){
    systemStatus.fileStatus   = file_none;
    scanFS();
    mag_data.cmd2=0;                                // Clear disc presence flags
    for(uint8_t i=0;i<FOLDERS;i++){                       // Transfer file count to cd info (for unilink)

      if(systemStatus.fileCount[i]){
        systemStatus.driveStatus = drive_ready;     // If any folder with audio files is found, set drive ready
        cd_data[i].tracks=systemStatus.fileCount[i];
        cd_data[i].mins=99;
        cd_data[i].secs=59;
        cd_data[i].inserted=1;
        switch(i){                                  // Set mag data disc presence
          case 0:
            mag_data.CD1_present=1;
            break;
          case 1:
            mag_data.CD2_present=1;
            break;
          case 2:
            mag_data.CD3_present=1;
            break;
          case 3:
            mag_data.CD4_present=1;
            break;
          case 4:
            mag_data.CD5_present=1;
            break;
          case 5:
            mag_data.CD6_present=1;
            break;
        }
      }
      else{
        cd_data[i].inserted=0;
        cd_data[i].tracks=99;
        cd_data[i].mins=0;
        cd_data[i].secs=0;
      }
    }
    unilink.mag_changed=1;                          // set flag to update

    if(systemStatus.driveStatus != drive_ready){    // Something went wrong
      iprintf("SYSTEM: No files found\r\n");
      systemStatus.driveStatus=drive_error;         // No files
      //unilink_clear_slave_break_queue();
      //mag_data.status==mag_removed;                       //FIXME: Untested
      //unilink_add_slave_break(cmd_cartridgeinfo);
      //unilink.status=unilink_ejecting;
      return;
    }
  }
#endif

  if((systemStatus.driveStatus==drive_error) || (systemStatus.driveStatus==drive_removed)){ // if drive removed or error
    iprintf("SYSTEM: Removing mounting point\r\n");
    f_mount(0, "", 1);                              // remove mount point
    if(systemStatus.driveStatus==drive_removed){
      systemStatus.driveStatus=drive_nodrive;
    }
    else{
      systemStatus.driveStatus=drive_unmounted;
    }

#ifdef AUDIO_SUPPORT
    AudioStop();
    mag_data.cmd2=0;
    mag_data.CD1_present=1;                               // Set only cd 1 ("aux" mode)
    cd_data[0].tracks=1;
    cd_data[0].mins=80;
    cd_data[0].secs=0;
    for(uint8_t i=1;i<6;i++){
      cd_data[i].tracks=99;
      cd_data[i].mins=0;
      cd_data[i].secs=0;
    }
    unilink.mag_changed=1;                            // set flag to update
#endif
  }
}
#endif

#ifdef AUDIO_SUPPORT
void iprintfiles(void){
  iprintf("File list:\r\n");
  for(uint8_t t=0; t<systemStatus.fileCount[unilink.disc-1];t++){
    iprintf("\t%s\r\n",fileList[t]);
  }
  iprintf("\r\n\r\n");
}


void sortFS(void){
  FRESULT res = FR_OK;
  char temp[13];
  uint32_t Min;
  uint8_t c=0;
  strcpy(systemStatus.lastFolder,folders[unilink.disc-1]);
  for(uint8_t t=0;t<FILETYPES;t++){
    res = f_findfirst(&dir, &fil, folders[unilink.disc-1], filetypes[t]);     // Find first file of the current type in current folder (unilink disc)
    if(res!= FR_OK){ continue; }                                              // If not OK, continue with next filetype
    while(fil.fname[0] && c<systemStatus.fileCount[unilink.disc-1]){          // Stop when no file found, last file or exceeded max file count
#if   (_USE_LFN)
      strcpy(fileList[c],fil.altname);                                          // Copy filename string
#else
      strcpy(fileList[c],fil.fname);                                          // Copy filename string
#endif
      f_findnext(&dir, &fil);                                                 // Find next file
      c++;
    }
  }

  for(uint32_t j=0; j<systemStatus.fileCount[unilink.disc-1]-1; j++){         // Sort alphabetically
    Min = j;
    for(uint32_t i=j+1; i<systemStatus.fileCount[unilink.disc-1]; i++){
      if(strcmp(fileList[Min], fileList[i]) > 0){  // List[Min] > List[i]
        Min = i;
      }
    }
    if(Min!=j){
      strcpy(temp, fileList[j]);
      strcpy(fileList[j], fileList[Min]);
      strcpy(fileList[Min], temp);
    }
  }
}

void scanFS(void){
  uint8_t c, i=0;
  while(i<FOLDERS){
    systemStatus.fileCount[i]=0;                                      // Clear old value
    c=0;
    for(uint8_t t=0;t<FILETYPES;t++){
      f_findfirst(&dir, &fil, folders[i], filetypes[t]);      // Find first file of the current type
      while(fil.fname[0] && c<MAXFILES){                    // Stop when no file found, last file or exceeded max file count
        f_findnext(&dir, &fil);                                     // Find next file
        c++;
      }
    }
    systemStatus.fileCount[i]=c;                                      // Store found file count
    iprintf("%s: Found %3d files\r\n",folders[i],c);                   // Debug number of files found in folder
    i++;
  }
}




uint8_t openFile(void){
  FRESULT res;

  if(systemStatus.driveStatus != drive_ready){
    return FR_NOT_READY;
  }
  systemStatus.lastFile[0] = 0;

  if(fileList[unilink.track-1][0]){                             // Detect file extension
    char currentType[6] = "*.\0\0\0";                           // Empty extension
    char *namePtr = (char*)fileList[unilink.track-1];           // Point to current filename
    while (*(namePtr+3)){ namePtr++; }                          // Find last char
    currentType[2] = *namePtr++;                                // Copy extension
    currentType[3] = *namePtr++;
    currentType[4] = *namePtr;

    for(uint8_t t=0;t<=FILETYPES;t++){                                  // Compare
      if(t>=FILETYPES){ return 0; }                                     // Exceeded filetypes (Unknown file extension!)
#if   (_USE_LFN)
      if(stricmp(currentType,  filetypes[t])==0){
#else
        if(strcmp(currentType,  filetypes[t])==0){
#endif
        if(t==file_mp3){
          systemStatus.filetype = file_mp3;
          break;
        }
        else if (t==file_wav){
          systemStatus.filetype = file_wav;
          break;
        }
      }
    }
  }
  for(uint8_t i=0;i<10;i++){
    res = f_chdir(folders[unilink.disc-1]);
    if(res==FR_OK) break;
  }
  if(res!=FR_OK){               // Change dir
    iprintf("Error opening folder \"%s\"\r\n",folders[unilink.disc-1]);
    return FR_DISK_ERR;
  }

  for(uint8_t i=0;i<10;i++){
    res = f_open(systemStatus.file, (char *)fileList[unilink.track-1], FA_READ);
    if(res==FR_OK) break;
  }
  if(res!=FR_OK){        // Open file
    iprintf("SYSTEM: Error opening file\r\n");
    systemStatus.driveStatus=drive_error;                                     // No files
    return FR_DISK_ERR;
  }
  clmt[0] = 32;                                                               // Set table size
  systemStatus.file->cltbl = clmt;                                                      // Enable fast seek feature (cltbl != NULL)

  for(uint8_t i=0;i<10;i++){
    res = f_lseek(systemStatus.file, CREATE_LINKMAP);
    if(res==FR_OK) break;
  }
  if( res != FR_OK ){                          // Create CLMT
    f_close( systemStatus.file );
    iprintf("SYSTEM: FatFS Seek error\r\n");
    return FR_DISK_ERR;
  }
#if   (_USE_LFN)
  f_findfirst(&dir, &fil, folders[unilink.disc-1], "*.*");      // Find first file of the current type
  while(fil.fname[0] && strcmp(fil.altname, (char *)fileList[unilink.track-1])!=0){                    // Stop when no file found, last file or exceeded max file count
    f_findnext(&dir, &fil);                                     // Find next file
  }
  iprintf("SYSTEM: Opened file: %s\r\n", fil.fname);
#else
  iprintf("SYSTEM: Opened file: %s\r\n", (char *)fileList[unilink.track-1]);
#endif
  systemStatus.audioChannels=audio_stereo;
  systemStatus.audioRate = audio_44KHz;
  systemStatus.audioBits = audio_16bit;
  systemStatus.fileStatus = file_opened;                                      // Valid file
  strcpy(systemStatus.lastFile, fileList[unilink.track-1]);
  return FR_OK;
}

void closeFile(void){
  f_close(systemStatus.file);
}


/*
uint8_t restorelast(void){                      // not needed, the ICS remembers disc and track
  #define SZ_TBL  8
  FRESULT res;
  DIR dp;
  FILINFO fno;
  FIL fp;
  DWORD clmt[SZ_TBL];

  //strcpy(systemStatus.lastPath,"/CD01");
  //strcpy(systemStatus.lastFile,"1.TXT");

  if((systemStatus.lastPath[0]!=0)&&(systemStatus.lastFile[0]!=0)){   // If stored path and file
    iprintf("Stored path: \"%s\"\r\n",systemStatus.lastPath);
    iprintf("Stored file: \"%s\"\r\n",systemStatus.lastFile);
  }
  else if( (systemStatus.lastFile[0]==0)||(systemStatus.lastPath[0]==0)){ // If any not stored, reset stored data
    systemStatus.lastFile[0]=0;
    systemStatus.lastPath[0]=0;
    iprintf("No previous stored file\r\n");
    uint8_t i=0;
    while(i<6){
      if(systemStatus.fileCnt[i]){                  // search first CD folder with files
        strcpy(systemStatus.lastPath,paths[i]);           // store folder path
        iprintf("Found %3d files in \"%s\"\r\n",systemStatus.fileCnt[i],paths[i]);
        break;
      }
      i++;
    }
    if(systemStatus.lastPath[0]==0){
      iprintf("No files found in filesystem");           // No compatible files in the filesystem
      return FR_DISK_ERR;
    }
  }
  res = f_chdir(systemStatus.lastPath);               // Change dir
  if(res){
    iprintf("Error opening folder \"%s\"\r\n",systemStatus.lastPath);
    return FR_DISK_ERR;
  }
  if(systemStatus.lastFile[0]!=0){
    res = f_open(&fp, systemStatus.lastFile, FA_READ);        // Open stored file
    if(res==FR_OK){

      iprintf("Opened File: \"%s\"\r\n",systemStatus.lastFile);
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
      iprintf("Error opening \"%s\"\r\n",systemStatus.lastFile);   // Error reading stored filenamew
    }
  }
  res = f_findfirst(&dp, &fno, systemStatus.lastPath, "*.MP3");       // Search first mp3 file
  if(res!=FR_OK){
    iprintf("Error: No files found\r\n");                // No files found
    return FR_DISK_ERR;                       // We should have files from previous scan
  }
  strcpy(systemStatus.lastFile,fno.fname);              // OK, store filename
  iprintf("Found  File: \"%s\"\r\n",fno.fname);
  res = f_open(&fp, systemStatus.lastFile, FA_READ);          // Open stored file
  if(res==FR_OK){
    iprintf("Opened File: \"%s\"\r\n",systemStatus.lastFile);
    return FR_OK;
  }
  iprintf("Error opening \"%s\"\r\n",systemStatus.lastFile);   // Error
  return FR_DISK_ERR;
}
*/
#endif
