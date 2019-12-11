#!/bin/sh
modinfo vfb.ko
sudo modprobe fb_sys_fops
sudo modprobe sysfillrect
sudo modprobe syscopyarea
sudo modprobe sysimgblt  
sudo insmod vfb.ko vfb_enable=1 videomemorysize=32000000
