import os
import sys
import threading
import time
from pathlib import Path
from tempfile import TemporaryDirectory
from time import sleep

import fabric
import vagrant
from entrypoint2 import entrypoint
from PIL import Image, ImageChops

from vncdotool import api

# pip3 install fabric vncdotool python-vagrant entrypoint2


print(sys.version)


def build(conn):
    # with c.cd("/home/vagrant/build"): # bug in invoke: https://github.com/pyinvoke/invoke/issues/459
    conn.sudo('sh -c "cd /home/vagrant/build;cmake /vagrant"')
    conn.sudo('sh -c "cd /home/vagrant/build;make"')


def set_resolution(conn, *res):
    (w, h, depth) = res
    conn.sudo("fbset -g %s %s %s %s %s" % (w, h, w, h, depth))


def run_in_background(conn, cmd):
    # https://docs.fabfile.org/en/1.3.8/faq.html#why-can-t-i-run-programs-in-the-background-with-it-makes-fabric-hang

    def thread_function(a):
        conn.sudo(cmd, pty=False, warn=True)

    x = threading.Thread(target=thread_function, args=(1,))
    x.start()


def start_server(conn, rotation, res):
    conn.sudo("killall framebuffer-vncserver", warn=True)
    command = f"/home/vagrant/build/framebuffer-vncserver -r {rotation}"
    print(f"command: {command}")
    run_in_background(conn, command)


def start_server_x11vnc(conn, rotation, res):
    (w, h, depth) = res
    conn.sudo("killall framebuffer-vncserver", warn=True)
    conn.sudo("killall x11vnc", warn=True)
    command = f"/usr/bin/x11vnc  -shared -forever -rawfb map:/dev/fb0@{w}x{h}x{depth}"
    print(f"command: {command}")
    run_in_background(conn, command)


def cls(conn):
    conn.sudo("dd if=/dev/zero of=/dev/fb0", warn=True)


def grab():
    with TemporaryDirectory() as tmpdirname:
        f = Path(tmpdirname) / "grab.png"
        with api.connect("localhost:0") as client:
            client.timeout = 5
            client.captureScreen(f)
        im = Image.open(f)
        return im


def _grab_and_sleep(blink_time):
    start = time.time()
    im = grab()
    dt = time.time() - start
    t = blink_time / 4.0 - dt
    if t > 0:
        sleep(t)
    return im


def img_list_min(ls):
    immin = ls[0]
    for im in ls[1:]:
        # ImageChops.subtract= max((a-b),0)
        diff = ImageChops.subtract(immin, im)
        immin = ImageChops.subtract(immin, diff)
    return immin


def grab_no_blink(blink_time=1.2):
    lsim = [_grab_and_sleep(blink_time) for _ in range(4)]
    im = img_list_min(lsim)
    return im


def shot(conn, directory, png, rotation, *res):
    (w, h, depth) = res

    set_resolution(conn, *res)
    start_server(conn, rotation, res)

    if depth != 8:
        cls(conn)
        run_in_background(conn, "qmlscene -platform linuxfb")
        sleep(0.5)
        grab_no_blink().save(directory + "qmlscene_" + png)
        conn.sudo("killall qmlscene")

    if depth > 1:
        cls(conn)
        conn.sudo("fb-test")
        grab().save(directory + "fbtest_" + png)

        cls(conn)
        conn.sudo(
            f"python3 /vagrant/gradient.py --width {w} --height {h} --colorbit {depth}"
        )
        grab().save(directory + "gradient_" + png)


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
        conn.sudo("killall qmlscene", warn=True)

        for rot in [0]:
            tshot(conn, rot, w, h, 1)
            tshot(conn, rot, w, h, 8)
            tshot(conn, rot, w, h, 16)
            tshot(conn, rot, w, h, 24)
            tshot(conn, rot, w, h, 32)
        for rot in [90, 180, 270]:
            tshot(conn, rot, w, h, 16)
