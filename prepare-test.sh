#!/bin/sh


cd ./tests/vfb
wget -N https://raw.githubusercontent.com/torvalds/linux/v3.2/drivers/video/vfb.c
make
sudo make ins
