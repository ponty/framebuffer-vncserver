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


def build(conn):
    # with c.cd("/home/vagrant/buildc"): # bug in invoke: https://github.com/pyinvoke/invoke/issues/459
    conn.sudo('sh -c "cd /home/vagrant/buildc;cmake /vagrant"')
    conn.sudo('sh -c "cd /home/vagrant/buildc;make"')


def set_resolution(conn, *res):
    (w, h, depth) = res
    conn.sudo("fbset -g %s %s %s %s %s" % (w, h, w, h, depth))


def start_server(conn, rotation):
    conn.sudo("killall framebuffer-vncserver", warn=True)
    command = f"nohup /home/vagrant/buildc/framebuffer-vncserver -r {rotation}  &> /dev/null &"
    print(f"command: {command}")
    conn.sudo(command, pty=False)


def shot(conn, directory, png, rotation, *res):
    (w, h, depth) = res

    set_resolution(conn, *res)
    start_server(conn, rotation)
    sleep(0.2)

    if depth != 8:
        conn.sudo("killall qmlscene", warn=True)
        conn.sudo("qmlscene -platform linuxfb  &> /dev/null &", pty=False)
        sleep(0.2)
        with api.connect("localhost:0") as client:
            client.timeout = 5
            client.captureScreen(directory + "qmlscene_" + png)
        conn.sudo("killall qmlscene", warn=True)

    if depth > 1:
        conn.sudo("fb-test")
        with api.connect("localhost:0") as client:
            client.timeout = 5
            client.captureScreen(directory + "fbtest_" + png)

        conn.sudo(
            f"python3 /vagrant/gradient.py --width {w} --height {h} --colorbit {depth}"
        )
        with api.connect("localhost:0") as client:
            client.timeout = 5
            client.captureScreen(directory + "gradient_" + png)


def tshot(conn, rotation, *res):
    (w, h, depth) = res
    d = "img/"
    os.makedirs(d, exist_ok=True)
    fname = f"{w}x{h}_c{depth}_rot{rotation}.png"
    shot(conn, d, fname, rotation, *res)


w, h = 160, 120


@entrypoint
def main():
    v = vagrant.Vagrant()
    v.up()
    with fabric.Connection(
        v.user_hostname_port(), connect_kwargs={"key_filename": v.keyfile(),},
    ) as conn:
        build(conn)
        for rot in [90, 180, 270]:
            tshot(conn, rot, w, h, 16)
        for rot in [0]:
            tshot(conn, rot, w, h, 1)
            tshot(conn, rot, w, h, 8)
            tshot(conn, rot, w, h, 16)
            tshot(conn, rot, w, h, 24)
            tshot(conn, rot, w, h, 32)
