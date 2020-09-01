#!/bin/bash
esptool.py -p /dev/ttyUSB0 -b 921600 write_flash -ff keep -fm keep -fs detect 0x00 ~/Documents/PlatformIO/Projects/Doors/.pio/build/hw622/firmware.bin