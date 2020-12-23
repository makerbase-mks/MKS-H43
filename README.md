# MKS-H43
MKS H43 is a 4.3-inch high-definition IPS display with a resolution of 800*480 and uses a capacitive touch screen. It is a high-end screen, which is very suitable for 3D printer upgrades.

# Feathers
```
—————————————————————————————————————————————————————————————————————————
   Display Size                  4.3 inches
_________________________________________________________________________
   Resolution                    800*480
_________________________________________________________________________
   LCD material                  IPS
_________________________________________________________________________
   Interface with mainboard      RJ11/AUX-1(Uart TTL)
_________________________________________________________________________
                                 MKS SGEN_L
                                 MKS Robin Nano Series          
                                 MKS Robin E3/E3D/E3P
                                 Creality V1.1.4(Ender3/Ender5 raw board)
    Mainboard support            BTT SKR Series
                                 ...
                                 (Theoretically support all mainboards with
                                 serial communication and running marlin V2.X firmware)
__________________________________________________________________________
   Communicate Protocol          DWIN DGUS
__________________________________________________________________________
```

# Mainboard support
MKS H43 is a serial LCD, it uses the TTL-UART to communicate with mainboard. So in theory, MKS H43 supports all mainboard with serial communication and running marlin V2.X firmware. As there are so many types of 3d printer mainboards, different mainboard has different uart available and unavailabe, so we make some test of the compatibility of MKS H43 and some mainboards, please refer to the wiki page.

# How to use
## Hardware connect
As we mentioned above, MKS H43 just use TTL-uart to connect to the mainboard, and we make several type of adapter boards to connect different mainboards. Details for the usage please refer to wiki.
## Marlin Config
1. We have added the mks dwin package to Marlin V2 firmware, you can download it from : https://github.com/makerbase-mks/Marlin-V2.X-MKS-H43. At the time of writing this article, Marlin officially has not merged the support of MKS H43. 
2. Just modify the board type on the "platformio.ini" according to your mainboard type, just like:
```
default_envs = mks_robin_nano
```
3. Config the MKS DWIN
- Open the "Configuration.h" file, find "DGUS_LCD_UI_MKS" and enable it,
- Open the "Configuration_adv.h" file, find "LCD_SERIAL_PORT", and configure the serial port number used to connect to H43. Please make sure that "LCD_SERIAL_PORT" should be different with the "SERIAL_PORT" in "Configuration.h", as "SERIAL_PORT" is used to communicate with the PC. And the baudrate should be set to 115200 by default.

then re-compile the source code, and update your mainboard.
After refresh the firmware, you can use the MKS H43 to display and touch now!

## Update the firmware of MKS H43
The firmware of MKS H43 has been burned before leaving the factory, so it is generally not necessary to update the firmware. But if you need to change the firmware version, or you want to customized the display interface, you can update according to the following method:
1. Prepare a TF card, format the TF card into FAT32 format, with 4096 bytes aligned.
2. Download the MKS_DGUS_Desier file on GitHub xxx, enter the folder and there will be a "DEWIN_SET" folder, copy "DEWIN_SET" into the TF card.
3. Power off the MKS H43, insert the TF card into MKS H43, and reboot it. An automatic update interface will appear on LCD. Waiting for the screen to display the word “end” which indicates that the update is successful. Generally, the update time is within 1 min.

note:
1. The insertion and removal of the TF card needs to be done when the power is off.
2. During the TF card update process, do not directly unplug the TF card, which will easily cause the firmware to be destroyed.
3. If the LCD doesn't display the blue update interface after the update begin, you should check whether the file name is wrong. 

## Customize MKS H43 UI and function
If you want to customize your own UI on MKS H43, or modify some functions, you can using the "DGUS designer" to make it:
1. Download the two folders "MKS_H43_DGUS" and "MKS_H43_Tool" from https://github.com/makerbase-mks/MKS-H43-firmware.
2. In the "MKS_H43_Tool" folder, unzip the DGUS designer, find "DGUS Tool V7.597.exe" and run it directly. Open the .hmi file in the "MKS_H43_DGUS" folder through the designer, you can directly open the H43 interface project.
3. Open "MKS_H43_DGUS", find the folder DWIN_SET and open it.
4. Modify the corresponding name and replace the picture you have prepared.
5. Open the designer
Select the Select Pictures button, select all the pictures in the folder, and the motor Generate ICL will replace 40.icl.
6. In the TF card, create a folder named "DWIN_SET", and copy the just generated 40.icl into the "DWIN_SET" folder in the TF card, then insert the TF card into the TF card socket of H43 Just power on and update. After waiting for the word “end” to appear on the interface, power off, pull out the TF card, and power on again.
7. For the function modification of H43, you can continue to add or delete corresponding function controls based on our DGUS project through the DGUS designer. In the H43 project opened by VScode, you can find the two files in the "Marlin/src/lcd/extui/lib/dgus/mks" path and the six files in the upper-level directory, all of which are H43. The functions are mainly concentrated in the file "DGUSScreenHandle.cpp".
8. In the DGUS designer, the size and position of the control can be freely placed, but the information of the control cannot be changed at will. After changing the size and position of the control, you need to find Generate and click in the File at the top of the designer. After generation, find the files "13TouchFile.bin", "14ShowFile.bin" and "22_Config.bin" in the DWIN_SET folder under the project, and put these files into the DWIN_SET folder in the TF card. Insert the TF card when H43 is powered off, then power on H43 and wait for END to appear! If the word is displayed, the TF card can be removed after power off, and the update is completed when power on again.

note:
1). When you are in the picture, you need to open the project first, otherwise the path you go to when you click DWIN ICL Generator is not in the DWIN_SET folder.
2) During the update process, H43 cannot be powered off directly, and can be powered off only after the update is completed.
3) When H43 modifies the control by itself, do not modify the important attributes such as the ID of the control, and only drag or zoom in and out of the control will not affect the motherboard code.


# WIKI
## Use MKS H43 with RAMPS 1.4 board
## Use MKS H43 with MKS GEN_L board
## Use MKS H43 with MKS SGEN_L board
## Use MKS H43 with MKS Robin Nano V1.2/V2.0 board
## Use MKS H43 with MKS Robin Nano V3.0 board
## Use MKS H43 with MKS Robin E3/E3D/E3P board






