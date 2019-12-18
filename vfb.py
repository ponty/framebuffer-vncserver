import vagrant
import fabric
# from fabric.api import env, execute, task, run, sudo, settings
from vncdotool import api
from entrypoint2 import entrypoint
import os
from time import sleep
# pip3 install fabric vncdotool python-vagrant entrypoint2

import sys
print(sys.version)


def build(c):
    # with c.cd("/home/vagrant/buildc"): # bug in invoke: https://github.com/pyinvoke/invoke/issues/459
    c.sudo('sh -c "cd /home/vagrant/buildc;cmake /vagrant"')
    c.sudo('sh -c "cd /home/vagrant/buildc;make"')


def set_resolution(c, *res):
    (w, h, depth) = res
    c.sudo('fbset -g %s %s %s %s %s' % (w, h, w, h, depth))
    c.sudo('fb-test')


def start_server(c):
    c.sudo('killall framebuffer-vncserver', warn=True)
    command = 'nohup %s &> /dev/null &' % '/home/vagrant/buildc/framebuffer-vncserver'
    c.sudo(command, pty=False)


def shot(c, png, *res):
    set_resolution(c, *res)
    start_server(c)
    sleep(0.2)
    with api.connect('localhost:0') as client:
        client.timeout = 5
        client.captureScreen(png)


def tshot(c, *res):
    (w, h, depth) = res
    d = 'tests/screenshots/'
    os.makedirs(d, exist_ok=True)
    fname = d+'shot%sx%sd%s.png' % (w, h, depth)
    shot(c, fname, *res)


@entrypoint
def main():
    v = vagrant.Vagrant()
    v.up()
    with fabric.Connection(
        v.user_hostname_port(),
        connect_kwargs={
            "key_filename": v.keyfile(),
        },
    ) as c:
        build(c)
        tshot(c, 640, 480, 32)
        # tshot(c,640, 480, 24)
        tshot(c, 640, 480, 16)
        tshot(c, 640, 480, 8)
        tshot(c, 320, 240, 32)
        # tshot(c,640, 480, 24)
        tshot(c, 320, 240, 16)
        tshot(c, 320, 240, 8)
