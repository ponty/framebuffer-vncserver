import os
import sys
import threading
from pathlib import Path
from time import sleep

import fabric
import vagrant
from entrypoint2 import entrypoint

# from fabric.api import env, execute, task, run, sudo, settings
from vncdotool import api

# pip3 install fabric vncdotool python-vagrant entrypoint2


print(sys.version)


def build(conn):
    # with c.cd("/home/vagrant/buildc"): # bug in invoke: https://github.com/pyinvoke/invoke/issues/459
    conn.sudo('sh -c "cd /home/vagrant/buildc;cmake /vagrant"')
    conn.sudo('sh -c "cd /home/vagrant/buildc;make"')


def set_resolution(conn, *res):
    (w, h, depth) = res
    conn.sudo("fbset -g %s %s %s %s %s" % (w, h, w, h, depth))


def run_in_background(conn, cmd):
    # https://docs.fabfile.org/en/1.3.8/faq.html#why-can-t-i-run-programs-in-the-background-with-it-makes-fabric-hang
    # conn.sudo(f"nohup {cmd} >& /dev/null < /dev/null &", pty=False)

    def thread_function(a):
        conn.sudo(cmd, pty=False, warn=True)

    x = threading.Thread(target=thread_function, args=(1,))
    x.start()


def start_server(conn, rotation, res):
    conn.sudo("killall framebuffer-vncserver", warn=True)
    # command = f"nohup /home/vagrant/buildc/framebuffer-vncserver -r {rotation}  &> /dev/null &"
    command = f"/home/vagrant/buildc/framebuffer-vncserver -r {rotation}"
    print(f"command: {command}")
    # conn.sudo(command, pty=False)
    run_in_background(conn, command)


def start_server_x11vnc(conn, rotation, res):
    (w, h, depth) = res
    conn.sudo("killall framebuffer-vncserver", warn=True)
    conn.sudo("killall x11vnc", warn=True)
    # command = f"nohup /usr/bin/x11vnc  -shared -forever -rawfb map:/dev/fb0@{w}x{h}x{depth}  &"
    command = f"/usr/bin/x11vnc  -shared -forever -rawfb map:/dev/fb0@{w}x{h}x{depth}"
    print(f"command: {command}")
    # conn.sudo(command, pty=False)
    run_in_background(conn, command)


def shot(conn, directory, png, rotation, *res):
    (w, h, depth) = res

    set_resolution(conn, *res)
    start_server(conn, rotation, res)
    sleep(1.2)

    if depth != 8:
        conn.sudo("killall qmlscene", warn=True)
        # conn.sudo("qmlscene -platform linuxfb  &> /dev/null &", pty=False)
        run_in_background(conn, "qmlscene -platform linuxfb")
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


IMGDIR = "img/"


def tshot(conn, rotation, *res):
    (w, h, depth) = res
    fname = f"{w}x{h}_c{depth}_rot{rotation}.png"
    shot(conn, IMGDIR, fname, rotation, *res)


w, h = 160, 120


@entrypoint
def main():
    v = vagrant.Vagrant()
    v.up()
    with fabric.Connection(
        v.user_hostname_port(),
        connect_kwargs={
            "key_filename": v.keyfile(),
        },
    ) as conn:
        build(conn)
        os.makedirs(IMGDIR, exist_ok=True)
        for f in Path(IMGDIR).glob("*.png"):
            f.unlink()

        for rot in [90, 180, 270]:
            tshot(conn, rot, w, h, 16)
        for rot in [0]:
            tshot(conn, rot, w, h, 1)
            tshot(conn, rot, w, h, 8)
            tshot(conn, rot, w, h, 16)
            tshot(conn, rot, w, h, 24)
            tshot(conn, rot, w, h, 32)
