# MKS-H43
MKS H43 is a 4.3-inch high-definition IPS display with a resolution of 800*480 and uses a capacitive touch screen. This is a high-end screen very suitable for 3D printer upgrades.

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
    Mainboard support            BTT SKR Series
                                 ...
                                 (Theoretically support all mainboards with
                                 serial communication and running marlin V2.X firmware)
__________________________________________________________________________
   Communicate Protocol          DWIN DGUS
__________________________________________________________________________
```

# Mainboard support
MKS H43 is a serial LCD, it uses the TTL-uart to communicate with mainboard. So in theory, MKS H43 supports all mainboard with serial communication and running marlin V2.X firmware. As there are so many types of 3d printer mainboards, different mainboard has different uart available and unavailabe, so we make some test of the compatibility of MKS H43 and some mainboards, please refer to the wiki page.

# How to use
## Hardware connect
As we mentioned above, MKS H43 just use TTL-uart to connect to the mainboard, and we make several type of adapter boards to connect different mainboards. Details for the usage please refer to wiki.
## Marlin Config
1. We have added the mks dwin package to Marlin V2 firmware, you can download it from:https://github.com/makerbase-mks/Marlin-2.x.x.
2. Just modify the board type on the "platformio.ini" to your mainboard type, just like:
```
default_envs = mks_robin_nano
```
3. Config the MKS DWIN
Open the Macro definition of "XXX":
```
```
then recompile the source code, and update your mainboard.
After refresh the firmware, you can use the MKS H43 to display and touch now!


## Customize MKS H43 UI and function


# WIKI
## Use MKS H43 with RAMPS 1.4 board
## Use MKS H43 with MKS GEN_L board
## Use MKS H43 with MKS SGEN_L board
## Use MKS H43 with MKS Robin Nano V1.2/V2.0 board
## Use MKS H43 with MKS Robin Nano V3.0 board
## Use MKS H43 with MKS Robin E3/E3D/E3P board






