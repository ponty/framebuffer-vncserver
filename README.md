# framebuffer-vncserver

VNC server for Linux framebuffer devices.

[![Build Status](https://travis-ci.org/ponty/framebuffer-vncserver.svg?branch=master)](https://travis-ci.org/ponty/framebuffer-vncserver)

The goal is to access remote embedded Linux systems without X.
Implemented features: remote display, touchscreen
Not implemented: keyboard, file transfer, ..

Remote display image is not perfect in some color configurations.

The code is based on a LibVNC example for Android:
https://github.com/LibVNC/libvncserver/blob/master/examples/androidvncserver.c

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
        framebuffer-vncserver [-f device] [-p port] [-t touchscreen] [-h]
        -p port: VNC port, default is 5900
        -f device: framebuffer device node, default is /dev/fb0
        -t device: touchscreen device node (example:/dev/input/event2)
        -h : print this help
 
