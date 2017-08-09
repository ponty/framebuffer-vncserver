# framebuffer-vncserver

VNC server for Linux framebuffer devices.

[![Build Status](https://travis-ci.org/ponty/framebuffer-vncserver.svg?branch=master)](https://travis-ci.org/ponty/framebuffer-vncserver)

The goal is to check remote embedded Linux systems without X, so only the remote display is implemented. 
(no input, no file transfer,..)

The code is based on a LibVNC example for Android:
https://github.com/LibVNC/libvncserver/blob/master/examples/android/jni/fbvncserver.c

All input handling was removed, command-line parameters port and fbdev were added.
In 32 bit color only half of the screen was displayed, so I hacked the code to show the full screen,
but I don't know how it works.

### build

Dependency:

	sudo apt-get install libvncserver-dev

There are 2 options: CMake or qmake

Using cmake:

	mkdir -p build && cd build
	cmake ..
	make
	
Using qmake:

	mkdir -p build && cd build
	qmake ../framebuffer-vncserver.pro
	make

 

### command-line help 

	# framebuffer-vncserver -h
	framebuffer-vncserver [-f device] [-p port] [-h]
	-p port: VNC port, default is 5900
	-f device: framebuffer device node, default is /dev/fb0
	-h : print this help
 