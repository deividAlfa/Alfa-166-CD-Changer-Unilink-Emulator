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
If you want to learn how Unilink works, check [DOCS](/DOCS) folder, everything I learned came from there.<br>
There's little information about Unilink on the net, most sites are long gone, some were dated back to the year 2000!<br>
I could recover some thanks to www.archive.org.<br>

Credits:
  - Sander Huijsen for [alfa166unilink](https://sourceforge.net/projects/alfa166unilink-git/)<br>
  - Michael Wolf for [Becker Unilink site](https://web.archive.org/web/20060214051232/http://www.mictronics.de/unilink_resource.php)
  - Cleggy for [Understanding the Unilink Bus](https://web.archive.org/web/20060211113837/http://www.cus.org.uk/~cleggy/)
  - Rubke for [UniLink Information](https://web.archive.org/web/20031217094219/http://members.home.nl/r.aerts/index2.htm) 
  
<a id="description"></a>
## Project description
This project enables communication with the ICS emulating the presence of the CD changer.<br>
It uses a STM32 "blackpill" board with STM32F411 running at 96MHz.<br>
It's currently working pretty well, but there might be some bugs or wrong / unimplemented functions.<br>
The project is done in STM32 Cube IDE and uses ST's HAL Library.<br>
Part of the configuration is done in the .ioc file (CubeMX configuration).<br>

### Features

 - Unilink is handled using the SPI peripheral with interrupts. 
 - Handles most of the Unilink protocol used in the ICS.
 - Uses the USB OTG function and it's able to play MP3 files from a USB drive.
   - Must be FAT32-formatted.
   - Songs must be stored inside CD01 ... CD06 folders.
   - Automatically scans these folders and its contents, and makes a listing for the ICS.
   - Tested up to 320Kbps without issues.
 - It's able to change tracks, discs, notify empty CD slots, etc, just like the original CD changer.
 - Repeat / Shuffle / Intro modes are not implemented.
 - Send the decoded audio to a I2S DAC.<br>
 I used a PCM5102A, but better use a UDA1334A as it has lower output signal, see Issues. 

### Issues 

The PCM5102A outputs 2Vrms, which is too much for the ICS input and will cause distortion.<br>
Use the UDA1334A instead, it's a readily available alternative which should work much better, it outputs 0.9Vrms.<br>
To fix the PCM5102A level, you need to halve the amplitude and buffer the signal.<br>
Driving the audio input directly with the resistor divider will lead to a very poor sound quality!<br>
Check the circuit below the PCM5102A connections.<br>
<br>

<a id="compiling"></a>
### Compiling

To compile:<br>
- Download STM32 Cube IDE. This project was made with v1.12.1, so better use that.
- Clone or download the repository.
- Open STM32 Cube IDE, import existing project and select the folder where the code is.

It should recognize it and be ready for compiling or modifying for your own needs.<br>

  ![IMAGE](/DOCS/build.png)
  
It can be programmed with any programmers supporting SWD (ST-Link, JFlash, DAP-Link...).<br>
You can also using the embedded bootloader (DFU): Download [Cube Programmer](https://www.st.com/en/development-tools/stm32cubeprog.html).<br>
Hold BOOT0 button down, connect the usb to the computer, release the button when detected.<br>
Then flash the compiled binary with the tool after compiling, the binaries are in `Release` folder, (.hex, .elf, .bin).<br>
<br>  
 
<a id="working"></a>
### Working options

The settings are placed in `config.h`.<br>

  - PASSIVE_MODE  - Will disable the slave interface and set the device in sniffer mode.<br>
    In this mode it can output the dialog between the ICS and the CD changer.
  - AUDIO_SUPPORT - Enable USB handling and MP3 decoding, fully integrated into Unilink.<br>
  Needs a DAC to decode the I2S stream and possibly some signal conditioning.<br>
  - BT_SUPPORT - Adds support for controlling a bluetooth module.
  Ideally this would be used with a module supporting AT commands, but this wasn't my case, so it's very specific.<br>
  In my case I modified a RRD-305 module and customized both button inputs and LED outputs for my needs.<br>  
  - Alternatively, it can be used as Aux-in enabler and can connect any audio source into the CD input.<br>
    For this, disable PASSIVE_MODE, AUDIO_SUPPORT and BT_SUPPORT (Add `//` before each `#define`).<br>
    Connect power and unilink signals, this will be enough to keep the ICS happy.<br>

<a id="debugging"></a>
### Debugging

  - UNILINK_LOG_ENABLE - Prints Unilink frames.
  - UNILINK_LOG_DETAILED - Decodes and prints what each frame means.
  - UNILINK_LOG_TIMESTAMP - Adds timestamps to each frame.  
  
  - UART_PRINT - Enables serial port logging to PA9 pin, 1Mbit baudrate.
  - SWO_PRINT - Enables SWO pin logging.<br>
    Connect the ST-Link to the STM32 (SWC=PA14 SWD=PA13 SWO=PB3).<br>
    Open Printf SWO viewer in ST-Link utility, set core clock to 96000000Hz.<br>
  - USB_LOG - Enables USB logging, creating a file `log.txt`.
       
####  UNILINK_LOG_DETAILED disabled: 
    31 10 01 13 55
    10 31 97 01 D9 79 99 59 10 54
    31 10 01 13 55
    70 31 90 00 31 02 00 41 1A 8E
    31 10 84 95 5A 10 00 00 00 6A
    10 31 95 B0 86 00 00 00 06 8C
    31 10 01 12 54
    10 31 00 00 41
    
####  UNILINK_LOG_DETAILED enabled: 
    << [31 10 01 13][55]                                    MASTER REQUEST: Slave poll
      #[10 31 97 01][D9][79 99 59 10][54]                   DISC INFO: Disc:1 Tracks:79 99m:59s
    << [31 10 01 13][55]                                    MASTER REQUEST: Slave poll
      #[70 31 90 00][31][02 00 41 1A][8E]                   TIME: Disc:1 Track:02 00:41
    << [31 10 84 95][5A][10 00 00 00][6A]                   REQUEST FIELD: MAGAZINE INFO
      #[10 31 95 B0][86][00 00 00 06][8C]                   MAGAZINE INFO: CD1:Y CD2:Y CD3:N CD4:Y CD5:N CD6:N
    << [31 10 01 12][54]                                    MASTER REQUEST: Time poll
      #[10 31 00 00][41]                                    STATUS: Playing

####  UNILINK_LOG_TIMESTAMPS enabled:
    00:02:04.957    << [31 10 01 13][55]                                    MASTER REQUEST: Slave poll
    00:02:04.970      #[10 31 97 01][D9][79 99 59 10][54]                   DISC INFO: Disc:1 Tracks:79 99m:59s
    00:02:05.087    << [31 10 01 13][55]                                    MASTER REQUEST: Slave poll
    00:02:05.100      #[70 31 90 00][31][02 00 41 1A][8E]                   TIME: Disc:1 Track:02 00:41
    00:02:05.166    << [31 10 84 95][5A][10 00 00 00][6A]                   REQUEST FIELD: MAGAZINE INFO
    00:02:05.181      #[10 31 95 B0][86][00 00 00 06][8C]                   MAGAZINE INFO: CD1:Y CD2:Y CD3:N CD4:Y CD5:N CD6:N
    00:02:05.206    << [31 10 01 12][54]                                    MASTER REQUEST: Time poll
    00:02:05.218      #[10 31 00 00][41]                                    STATUS: Playing
         
<br><br>
There's an Excel sheet full of captured data in [DOCS](/DOCS) folder: [ICS logs.xlsm](/DOCS/ICS%20logs.xlsm).<br>
It uses VBA to parse the data, so you need to enable macros. The function is called `ParseUnilink()`.<br>
  
    01 MASTER REQUEST - BUS RESET    18 10 01 00 29 00
    01 MASTER REQUEST - ANYONE?      18 10 01 02 2B 00
    8C DEVICE INFO                   10 30 8C 11 DD 14 A8 17 60 10 00

 
<a id="connections"></a>
### Connections

The ICS connection is as follows:
<br>

![IMAGE](/DOCS/ICS_pinout.jpg)
![IMAGE](/DOCS/ICS_pinout2.png)
  
  - Audio
    - Audio gnd: `C3-18` - Don't use other grounds as it might induce noise.
    - Left input: `C3-19`
    - Right input: `C3-20`
    
  - Unilink interface (Unilink Reset and Enable signals are not used)
    - Power: `C2-8`, permanent 12V. You'll need a 5V voltage regulator for the STM32 board.
    - BUS_ON: `C2-7`, makes some pulses at power-on to wake up the slave. It's used to turn a mosfet on and power our device on.<br>
      Then the stm32 will maintain the power win PWR_ON pin, and will shutdown after 10 seconds with no communication with the ICS.<br>
    - Ground: `C2-9`, it's  missing in the ICS pinout, but fully correct. Don't use this ground for audio.
    - Data: `C2-10`, connect to STM32 PA6 (UNILINK_DATA).
    - Clk: `C2-11`, connect to STM32 PA5(UNILINK_CLOCK).
    



### Basic connection
  
  ![IMAGE](/DOCS/sch.png)

### Stm32 pinout
  
  ![IMAGE](/DOCS/stm32_pinout.png)

### Power switch circuit

 These are generic components, any P-channel mosfet capable of driving 500mA and 30V will work, same for the npn transistor or the DC/DC module.
  
  ![IMAGE](/DOCS/power.png)
  
### DAC connection (PCM5102A example)
 
  ![IMAGE](/DOCS/PCM5102A.jpg)
  
### PCM5102A output level fix (For each channel)

 Most operational amplifiers with low noise will work here, make sure the output can swing between 0.5 and 3.5V with a 5V supply voltage or it will cause audio clipping.<br>
 Normally they have more problems getting the output close to V+ than to Gnd, that's why the input is biased a bit lower than VCC/2.<br>
 
  ![IMAGE](/DOCS/amp.png)

<br>
Some STM32F411 boards have an issue with USB OTG not working.<br>
This is caused by diode, not allowing the power to go from the board to the USB.<br>
The diode was removed on later revisions of the board. The fix is easy, just replace the diode with a jumper:

![IMAGE](/DOCS/411_OTGFIX.jpg)
