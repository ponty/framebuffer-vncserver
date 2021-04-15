# -*- mode: ruby -*-
# vi: set ft=ruby :

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure(2) do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://atlas.hashicorp.com/search.
  config.vm.box = "ubuntu/focal64"

  # Disable automatic box update checking. If you disable this, then
  # boxes will only be checked for updates when the user runs
  # `vagrant box outdated`. This is not recommended.
  # config.vm.box_check_update = false

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine. In the example below,
  # accessing "localhost:8080" will access port 80 on the guest machine.
  config.vm.network "forwarded_port", guest: 5900, host: 5900

  # Create a private network, which allows host-only access to the machine
  # using a specific IP.
  # config.vm.network "private_network", ip: "192.168.33.10"

  # Create a public network, which generally matched to bridged network.
  # Bridged networks make the machine appear as another physical device on
  # your network.
  # config.vm.network "public_network"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  # config.vm.synced_folder "../data", "/vagrant_data"

  # Provider-specific configuration so you can fine-tune various
  # backing providers for Vagrant. These expose provider-specific options.
  # Example for VirtualBox:
  #
  config.vm.provider "virtualbox" do |vb|

    # https://bugs.launchpad.net/cloud-images/+bug/1829625
    vb.customize ["modifyvm", :id, "--uart1", "0x3F8", "4"]
    vb.customize ["modifyvm", :id, "--uartmode1", "file", "./ttyS0.log"]
    
  #   # Display the VirtualBox GUI when booting the machine
  #   vb.gui = true
  #
  #   # Customize the amount of memory on the VM:
  #   vb.memory = "1024"
  end
  #
  # View the documentation for the provider you are using for more
  # information on available options.

  # Define a Vagrant Push strategy for pushing to Atlas. Other push strategies
  # such as FTP and Heroku are also available. See the documentation at
  # https://docs.vagrantup.com/v2/push/atlas.html for more information.
  # config.push.define "atlas" do |push|
  #   push.app = "YOUR_ATLAS_USERNAME/YOUR_APPLICATION_NAME"
  # end

  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  $script = "
  export DEBIAN_FRONTEND=noninteractive
  echo 'export distutils_issue8876_workaround_enabled=1' >> /home/vagrant/.profile
  echo 'export export LC_ALL=C' >> /home/vagrant/.profile
   
  # tools
  sudo apt-get update
  sudo apt-get install -y mc htop
  sudo apt-get install -y evtest
  sudo apt-get install -y xvfb
  sudo apt-get install -y python3-pip
  sudo apt-get install -y libvncserver-dev
  sudo apt-get install -y build-essential flex bison
  sudo apt-get install -y cmake
  sudo apt-get install -y qt5-qmake qt5-default
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
  ln -s /home/vagrant/linux-source-*/drivers/video/fbdev/vfb.c  vfb.c
  ln -s /vagrant/tests/vfb/ins.sh  ins.sh
  ln -s /vagrant/tests/vfb/Makefile  Makefile
  make
  #./ins.sh

  echo '#!/bin/sh
modinfo /home/vagrant/vfb/vfb.ko
modprobe fb_sys_fops
modprobe sysfillrect
modprobe syscopyarea
modprobe sysimgblt
insmod /home/vagrant/vfb/vfb.ko vfb_enable=1 videomemorysize=32000000
  ' >  /usr/local/bin/vfbload.sh
  chmod +x /usr/local/bin/vfbload.sh
  /usr/local/bin/vfbload.sh > /tmp/vfbload.log 2>&1

  echo 'ENV{ID_INPUT_MOUSE}==\"?*\",ENV{ID_PATH}==\"pci-0000:00:04.0\", SYMLINK+=\"input/ms\"' >  /etc/udev/rules.d/98-input.rules
  echo 'ENV{ID_INPUT_KEYBOARD}==\"?*\", SYMLINK+=\"input/kbd\"' >> /etc/udev/rules.d/98-input.rules
  udevadm control --reload-rules
  udevadm trigger

  echo 'SHELL=/bin/sh
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
@reboot   root    vfbload.sh;sleep 0.1;fbset -g 640 480 640 480 16;/home/vagrant/buildc/framebuffer-vncserver -t /dev/input/ms -k /dev/input/kbd > /tmp/framebuffer-vncserver.log 2>&1
  ' >  /etc/cron.d/framebuffer-vncserver

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
  mkdir -p buildc && cd buildc
  cmake /vagrant
  make
  
  # qmake build
  cd /home/vagrant
  mkdir -p buildq && cd buildq
  qmake /vagrant
  make

  fbset -g 640 480 640 480 16
  /home/vagrant/buildc/framebuffer-vncserver -t /dev/input/ms -k /dev/input/kbd > /tmp/framebuffer-vncserver.log 2>&1 &
  "
      config.vm.provision "shell", inline: $script

      config.ssh.extra_args = ["-t", "cd /vagrant; bash --login"]       

end
