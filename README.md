# MKS-H43
MKS H43 is a 4.3-inch high-definition IPS display with a resolution of 800*480 and uses a capacitive touch screen. It is a high-end screen, which be adapted to most FDM 3D printer motherboards on the market， very suitable for 3D printer upgrades.

# Feathers
```
—————————————————————————————————————————————————————————————————————————
   Display Size                  4.3 inches
_________________________________________________________________________
   Resolution                    800*480
_________________________________________________________________________
   LCD material                  IPS
_________________________________________________________________________
   Touch screen                  Capacitive touch screen
_________________________________________________________________________
   Interface with motherboard    RJ11/AUX-1(Uart TTL)
_________________________________________________________________________
                                 MKS SGEN_L
                                 MKS Robin Nano Series          
                                 MKS Robin E3/E3D/E3P
                                 Creality3D V1.1.4(Ender3/Ender5 raw board)
    Motherboards support         BTT SKR Series
                                 ...
                                 (Theoretically support all motherboards with
                                 serial communication and running marlin V2.X firmware)
__________________________________________________________________________
   Communicate Protocol          DWIN DGUS
__________________________________________________________________________
```

# Motherboards support
MKS H43 is a serial LCD, it uses the TTL-UART to communicate with motherboards. So in theory, MKS H43 supports all motherboards with serial communication and running marlin V2.X firmware. As there are so many types of 3d printer motherboards, different motherboard has different uart available and unavailabe, so we make some test of the compatibility of MKS H43 and some motherboards, please refer to the **wiki page**.

# How to use
## Hardware connect
As we mentioned above, MKS H43 just uses TTL-UART to connect to the motherboard, in fact the 4 pin signals are:
```
DC5V
GND
UART-TX
UART-RX
```
And we designed two types of uart sockets on MKS H43: one AUX and one RJ11. We also made two type of adapter boards for connecting different motherboards: 
```
MKS H43 Apdator-A: Convert from RJ11/AUX to EXP1/EXP2 interface of most motherboards, it also extends a SD socket
MKS H43 Apdator-B: Convert from RJ11 to AUX-1 interface of most motherboards, and EXP1 of Creality3D V1.1.4 
```
The reason we added RJ11 socket is to allow users to use a spring wire (the microphone cable of an old telephone) to connect to the screen, so that the screen can be easily manipulated and placed.

What have to be aware of is: As the MKS H43 communicates with the motherboard using DWIN DGUS protocol, which is different from the simple gcode commands, so the motherboard should use a serial port different from the PC connection to connect to MKS H43, unless you don’t need PC control. So maybe your motherboard has the "AUX-1" socket, but if it shares the same serial port with the PC connection, you cannot connect at the same time.

There are many situations for different motherboards:
- With independent serial on AUX-1 interface, such as MKS SGEN_L V1/V2, you can use Adaptor-B adapter board
- With independent serial on EXP1/EXP2 interface, such as MKS GEN_L / RAMPS1.4, which can use the Adaptor-A adapter board
- Without independent serial ports on AUX-1 or EXP1/EXP2, but there are independent serial ports with other forms of interfaces. You can use the Adaptor-B adapter board and use the corresponding cable to switch, such as Creality3D V1.1.4 / MKS Robin Nano V1/V2/V3 / BTT SKR V1.3/V1.4
- Only with the shared serial with PC connection, you can only use MKS H43 without PC connection.
- No serial interface(very rare), cannot use MKS H43

And for different motherboards, you can refer to the detail connection and configuration on **WIKI**.

The hardware information of MKS H43 and adaptor boards, you can refer to : https://github.com/makerbase-mks/MKS-H43/tree/main/hardware.


## Marlin Config
1. We have added the support to Marlin V2 firmware and uploaded the source code here: https://github.com/makerbase-mks/Marlin-V2.X-MKS-H43. At the time of writing this article, Marlin officially has not merged the support of MKS H43, after Marlin merge, you can directly use the official one.
2. Using VSCode to open Marlin source, and modify the motherboard type on the "platformio.ini" according to yours, just like:
```
default_envs = mks_robin_nano
```
3. Config the MKS DWIN
- Open the "Configuration.h" file, find "DGUS_LCD_UI_MKS" and enable it,
- Open the "Configuration_adv.h" file, find "LCD_SERIAL_PORT", and configure the serial port number used to connect to H43. Please make sure that "LCD_SERIAL_PORT" should be different with the "SERIAL_PORT" in "Configuration.h", as "SERIAL_PORT" is used to communicate with the PC. And the baudrate should be set to 115200 by default.

After config other options according to your machine, compile the source code, and update your motherboard. Then you can use the MKS H43 to display and touch!

## Update the firmware of MKS H43
The firmware of MKS H43 has been burned before leaving the factory, so it is generally not necessary to update the firmware. But if you need to update the firmware version, or you want to customize the display interface, you can update according to the following method:
1. Prepare a TF card, format the TF card into FAT32 format, with 4096 bytes aligned.
2. The firmware of MKS H43 can be found on https://github.com/makerbase-mks/MKS-H43/tree/main/DWIN_SET_For_H43, download the version you need. Copy "DWIN_SET" folder to TF card after decompression.
3. Power off the MKS H43, insert the TF card into MKS H43, then power on. An automatic update interface will appear on LCD. Waiting for the screen to display the word “end” which indicates that the update is successful. Generally, the update time is within 1 min.

Note:
1. The insertion and removal of the TF card needs to be done when the power is off.
2. During the TF card update process, do not directly unplug the TF card, which will easily cause the firmware to be destroyed.
3. If the LCD doesn't display the blue update interface after the update begin, you should check whether the name of folder is wrong. 

## Customize MKS H43 UI and functions
If you want to customize your own UI on MKS H43, or modify some functions, you can using the "DGUS Tool" to make it, only support Windows OS so far:
1. Download the "DGUS_Tool_Vxxx.rar" from https://github.com/makerbase-mks/MKS-H43/tree/main/Tool, this is the tool use to edit the firmware of H43. Decompress it and run the "DGUS Tool Vxxx.exe". The default language is Simplified Chinese, you can config to English on the menu of "配置"(Setting)->Language.
2. Download the source code of MKS H43 from https://github.com/makerbase-mks/MKS-H43-firmware, open the project file "DWprj.hmi" file with the DGUS Tool above.
3. Now you can change the size of images/replace your own images or modify the execute functions and so on. More details about how the operate steps, please download the [T5L_DGUSII Application Development Guide](https://github.com/makerbase-mks/MKS-H43/blob/main/Tool/T5L_DGUSII%20Application%20Development%20Guide20200902.pdf).
4. After your modification, select the "DWIN ICL Generator" on DGUS TOOL's welcome page, it will generate the files for burned.
5. Copy the "DWIN SET" folder to the TF card(make sure it has been formated as FAT32 format with 4096 bytes aligned before, and insert it into MKS H43 board, reboot the board and it would update automatically.

Note: About the detail about customizing the UI and functions, please download the [T5L_DGUSII Application Development Guide](https://github.com/makerbase-mks/MKS-H43/blob/main/Tool/T5L_DGUSII%20Application%20Development%20Guide20200902.pdf).







