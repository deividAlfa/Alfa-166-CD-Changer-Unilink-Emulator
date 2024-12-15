## Alfa romeo 166 unilink CD changer emulator.

<!-- MarkdownTOC -->

* [Introduction](#intro)
* [Project description](#description)
* [Compiling](#compiling)
* [Debugging](#debugging)
* [Connections](#connections)

<!-- /MarkdownTOC -->

<a id="intro"></a>
## Introduction
This project aims to make a CD changer emulator for the Alfa Romeo 166.<br>
If you want to learn how Unilink works, check DOCS folder.<br>
There is a lot of data. All I learned from is there.<br>
Most of it is actually gone from Internet, as the sites were very old. Some were dated back to 2001!<br>
I could recover some thanks to www.archive.org.<br>
The code is very valuable, there's almost no information about this on the net.<br>

<a id="description"></a>
## Project description
This project enables communication with the ICS emulating the presence of the CD changer.<br>
It uses a STM32 "blackpill" board with STM32F411 running at 96MHz.<br>
The project is done in STM32 Cube IDE and uses ST's HAL Library.<br>
Part of the configuration is done in the .ioc file (CubeMX configuration).<br>

The unilink line is handled using the SPI peripheral with interrupts.<br> 
The current state of the firmare handles almost all the unilink protocol used in the ICS.<br>
It also uses the USB OTG function. The CDs are stored as CD01 ... CD06 folders in the USB drive, must use FAT32.<br>
It automatically scans these folders and its contents, and makes a listing for the ICS.<br>
It's able to change tracks, also to tell the ICS when the selected disc is not present.<br>
Now it can decode MP3 and send the audio to a I2S DAC (I used a PCM5102A).<br>

It's currently workign but pretty beta, there might be some bugs or wrongly unimplemented functions.<br> 

<a id="compiling"></a>
## Compiling

To compile:<br>
- Download STM32 Cube IDE.
- Clone or download the repository.
- Open STM32 Cube IDE, import existing project and select the folder where the code is.

It should recognize it and be ready for compiling or modifying for your own needs.<br>
Check "unilink.h" and "unilink.c". 
  
 
  
<a id="debugging"></a>
## Debugging

The firmware has different levels of debugging the Unilink data, see `unilink.h`, `unilink_log.h`.<br>
- "DebugLog" will display the unilink frames in a readable format. It has two additional modifiers:
	- "Detail_Log" will split the data frames within brackets, so the data and checksums can be read easier.
	- "OnlyLog" will disable the slave interface and set the device in sniffer mode.<br>
	In this mode it can output the dialog between the ICS and the CD changer.
	
 Example log outputs:<br>
 
	Detail_Log disabled:
	31 10 01 13 55
  10 31 97 01 D9 20 79 46 10 C8
  31 10 01 13 55
  10 31 00 00 41
  31 11 B0 12 04 00 00 00 00 04 
	
	Detail_Log enabled:	
	[31 10 01 13][55]                                    MASTER REQUEST: Slave poll
  [10 31 97 01][D9][20 79 46 10][C8]                   DISC INFO: Disc:1 Tracks:20 79m:46s
  [31 10 01 13][55]                                    MASTER REQUEST: Slave poll
  [10 31 00 00][41]                                    STATUS: Playing
  [31 11 B0 12][04][00 00 00 00][04]                   CHANGE TO: Disc:2 Track:00 

- There's an Excel sheet in Documentation folder full of captured data, "Parse unilink data.xlsm"..<br>
  It uses VBA, so you need to enable macros. The function is called "ParseUnilink".<br>
  The result would be:
  
		01 MASTER REQUEST - BUS RESET		18 10 01 00 29 00
		01 MASTER REQUEST - ANYONE?		18 10 01 02 2B 00
		8C DEVICE INFO			10 30 8C 11 DD 14 A8 17 60 10 00
	  
		

The debugging data is sent using the serial port, and also through the SWO pin.<br>
 
<a id="connections"></a>
## Connections

The ICS connection is as follows:<br>
![IMAGE](https://github.com/deividAlfa/Alfa-166-Unilink-CD-emulator/blob/main/DOCS/ICS_pinout.jpg)
![IMAGE](https://github.com/deividAlfa/Alfa-166-Unilink-CD-emulator/blob/main/DOCS/ICS_pinout2.jpg)

  - All "terminal 30" pins = 12V power
  - Use any gnd pin - One is enough.
  
  - Audio:
    - Audio gnd: ICS F18 - Don't use other grounds as it might induce noise in the audio.
    - Left input: ICS F19
    - Right input: ICS F20
    
  - Unilink interface:
    - Only Data and Clock are used. Reset and Enable are not needed.
    - Clk: ICS F11, STM32 PA5.
    - Data: ICS F10, STM32 PA6.

The stm32 pinout is as follows:<br>

![IMAGE](https://github.com/deividAlfa/Alfa-166-Unilink-CD-emulator/blob/main/DOCS/stm32_pinout.png)


Some STM32F411 boards have an issue with USB OTG not working.<br>
This is caused by diode, not allowing the power to go from the board to the USB.<br>
The diode was removed on later revisions of the board. The fix is easy, just replace the diode with a jumper:

![IMAGE](https://github.com/deividAlfa/Alfa-166-Unilink-CD-emulator/blob/main/DOCS/411_OTGFIX.jpg)
