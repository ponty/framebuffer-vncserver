Vagrant.configure(2) do |config|
  config.vm.box = "ubuntu/focal64"

  config.vm.network "forwarded_port", guest: 5900, host: 5900

  config.vm.provider "virtualbox" do |vb|
    vb.name = "framebuffer-vncserver.20.04"
    #   vb.gui = true
    #   vb.memory = "1024"

    # https://bugs.launchpad.net/cloud-images/+bug/1829625
    # vb.customize ["modifyvm", :id, "--uart1", "0x3F8", "4"]
    # vb.customize ["modifyvm", :id, "--uartmode1", "file", "./ttyS0.log"]
  end

  config.vm.provision "shell", path: "vagrant.sh"

  config.ssh.extra_args = ["-t", "cd /vagrant;sudo su"]
end

# export VAGRANT_VAGRANTFILE=Vagrantfile.20.04.rb;export VAGRANT_DOTFILE_PATH=.vagrant_${VAGRANT_VAGRANTFILE}
