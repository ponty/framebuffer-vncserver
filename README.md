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
 
## Run on startup as service

To run at startup as a service using systemd, edit the file `fbvnc.service` make sure the path and command line arguments are correct and then run:

```shell
sudo cp fbvnc.service /etc/systemd/system/
sudo systemctl enable fbvnc.service
sudo systemctl start fbvcn.service
```
