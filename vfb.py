import vagrant
import fabric
from fabric.api import env, execute, task, run, sudo, settings
from vncdotool import api


@task
def mytask(w, h, depth):
    sudo('cd /home/vagrant/buildc/ && cmake /vagrant && make')
    with fabric.context_managers.settings(warn_only=True):
        sudo('killall framebuffer-vncserver')
        sudo('cd /home/vagrant/vfb;./ins.sh')
    sudo('fbset -g %s %s %s %s %s' % (w, h, w, h, depth))
    sudo('fb-test')


def background_run(command):
    command = 'nohup %s &> /dev/null &' % command
    sudo(command, pty=False)


def shot(png, w, h, depth):
    v = vagrant.Vagrant()
    v.up()
    env.hosts = [v.user_hostname_port()]
    env.key_filename = v.keyfile()
    # useful for when the vagrant box ip changes.
    env.disable_known_hosts = True
    execute(mytask, w, h, depth)  # run a fabric task on the vagrant host.
    # run a fabric task on the vagrant host.
    execute(background_run, '/home/vagrant/buildc/framebuffer-vncserver')

    with api.connect('localhost:0') as client:
        client.captureScreen(png)


def tshot(w, h, depth):
    fname='shot%sx%sd%s.png' % (w, h, depth)
    shot(fname, w, h, depth)
tshot(640, 480, 32)
# tshot(640, 480, 24)
tshot(640, 480, 16)
tshot(640, 480, 8)
tshot(320, 240, 32)
# tshot(640, 480, 24)
tshot(320, 240, 16)
tshot(320, 240, 8)
