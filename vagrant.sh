#!/bin/bash
export DEBIAN_FRONTEND=noninteractive
sudo update-locale LANG=en_US.UTF-8 LANGUAGE=en.UTF-8

# tools
sudo apt-get update
sudo apt-get install -y mc htop
sudo apt-get install -y evtest
sudo apt-get install -y xvfb
sudo apt-get install -y python3-pip
sudo apt-get install -y libvncserver-dev
sudo apt-get install -y build-essential flex bison
sudo apt-get install -y cmake
sudo apt-get install -y qt5-qmake
sudo apt-get install -y qt5-qmake-bin
sudo apt-get install -y qtbase5-dev
sudo apt-get install -y linux-source libssl-dev libelf-dev
sudo apt-get install -y fbset fbcat fbterm fbi
sudo apt-get install -y qmlscene
sudo apt-get install -y x11vnc
sudo pip3 install entrypoint2

# fb-test
cd /home/vagrant
git clone https://github.com/ponty/fb-test-app.git
cd fb-test-app
make
cp perf /usr/local/bin
cp rect /usr/local/bin
cp fb-test /usr/local/bin
cp fb-string /usr/local/bin

# vfb
cd /home/vagrant
tar xaf /usr/src/linux-source-*.tar.*
#cd linux-source-*/
mkdir -p vfb && cd vfb
ln -s /home/vagrant/linux-source-*/drivers/video/fbdev/vfb.c vfb.c
ln -s /vagrant/tests/vfb/ins.sh ins.sh
ln -s /vagrant/tests/vfb/Makefile Makefile
make
#./ins.sh

echo '#!/bin/bash
modinfo /home/vagrant/vfb/vfb.ko
modprobe fb_sys_fops
modprobe sysfillrect
modprobe syscopyarea
modprobe sysimgblt
insmod /home/vagrant/vfb/vfb.ko vfb_enable=1 videomemorysize=32000000
  ' >/usr/local/bin/vfbload.sh
chmod +x /usr/local/bin/vfbload.sh
/usr/local/bin/vfbload.sh >/tmp/vfbload.log 2>&1

echo 'ENV{ID_INPUT_MOUSE}=="?*",ENV{ID_PATH}=="pci-0000:00:04.0", SYMLINK+="input/ms"' >/etc/udev/rules.d/98-input.rules
echo 'ENV{ID_INPUT_KEYBOARD}=="?*", SYMLINK+="input/kbd"' >>/etc/udev/rules.d/98-input.rules
udevadm control --reload-rules
udevadm trigger

echo 'SHELL=/bin/sh
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
@reboot   root    vfbload.sh;sleep 0.1;fbset -g 640 480 640 480 16;/home/vagrant/build/framebuffer-vncserver -t /dev/input/ms -k /dev/input/kbd > /tmp/framebuffer-vncserver.log 2>&1
  ' >/etc/cron.d/framebuffer-vncserver

# https://askubuntu.com/questions/168279/how-do-i-build-a-single-in-tree-kernel-module
# https://askubuntu.com/questions/515407/how-recipe-to-build-only-one-kernel-module
#cd /usr/src/
#tar xaf linux-source-*.tar.*
#cd linux-source-*/
#make oldconfig # it copies .config to ./
#echo 'CONFIG_FB_VIRTUAL=m' >>  .config
#make scripts
#make drivers/video/fbdev/vfb.ko

# cmake build
cd /home/vagrant
mkdir -p build && cd build
cmake /vagrant
make

# qmake build
cd /home/vagrant
mkdir -p buildqt && cd buildqt
qmake /vagrant
make

fbset -g 640 480 640 480 16
/home/vagrant/build/framebuffer-vncserver -t /dev/input/ms -k /dev/input/kbd >/tmp/framebuffer-vncserver.log 2>&1 &
