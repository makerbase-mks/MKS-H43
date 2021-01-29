# Introduce
Marlin_H43_for_Creality_ender3.hex is an compiled firmware of Marlin firmware for Creality Ender3 raw board Creality V1.1.4. It was compiled from the source code: https://github.com/makerbase-mks/Marlin-V2.X-MKS-H43, and had added the support of MKS H43. 

# How to upload
If your Creality V1.1.4 has bootloader, you can use [xloader](http://www.hobbytronics.co.uk/download/XLoader.zip) to upload the .hex firmware.

If your Creality V1.1.4 doesn't has bootloader, you should use the [ISP USB module](https://www.aliexpress.com/item/4001211961989.html?spm=2114.12010615.8148356.12.6b2d2c13ei8zzR) to upload. 

Hint:Why does some boards have no bootloader? Because the flash of the main control chip mega1284p is too small, if the marlin firmware you use is too large, the bootloader may be flushed after the upgrade.




