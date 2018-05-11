/*
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * This project is an adaptation of the original fbvncserver for the iPAQ
 * and Zaurus.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>             /* For makedev() */

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>

#include <assert.h>
#include <errno.h>

/* libvncserver */
#include "rfb/rfb.h"
//#include "rfb/keysym.h"

#include "touch.h"
//#include "keyboard.h"
#include "logging.h"

/*****************************************************************************/
//#define LOG_FPS

#define BITS_PER_SAMPLE     5
#define SAMPLES_PER_PIXEL   2

static char fb_device[256] = "/dev/fb0";
static char touch_device[256] = "";

static struct fb_var_screeninfo scrinfo;
static int fbfd = -1;
static unsigned short int *fbmmap = MAP_FAILED;
static unsigned short int *vncbuf;
static unsigned short int *fbbuf;

static int vnc_port = 5900;
static rfbScreenInfoPtr server;
static size_t bytespp;


/* No idea, just copied from fbvncserver as part of the frame differerencing
 * algorithm.  I will probably be later rewriting all of this. */
static struct varblock_t
{
    int min_i;
    int min_j;
    int max_i;
    int max_j;
    int r_offset;
    int g_offset;
    int b_offset;
    int rfb_xres;
    int rfb_maxy;
} varblock;

/*****************************************************************************/


static void init_fb(void)
{
    size_t pixels;

    if ((fbfd = open(fb_device, O_RDONLY)) == -1)
    {
        error_print("cannot open fb device %s\n", fb_device);
        exit(EXIT_FAILURE);
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &scrinfo) != 0)
    {
        error_print("ioctl error\n");
        exit(EXIT_FAILURE);
    }

    pixels = scrinfo.xres * scrinfo.yres;
    bytespp = scrinfo.bits_per_pixel / 8;

    info_print("  xres=%d, yres=%d, xresv=%d, yresv=%d, xoffs=%d, yoffs=%d, bpp=%d\n",
            (int)scrinfo.xres, (int)scrinfo.yres,
            (int)scrinfo.xres_virtual, (int)scrinfo.yres_virtual,
            (int)scrinfo.xoffset, (int)scrinfo.yoffset,
            (int)scrinfo.bits_per_pixel);
    info_print("  offset:length red=%d:%d green=%d:%d blue=%d:%d \n",
            (int)scrinfo.red.offset, (int)scrinfo.red.length,
            (int)scrinfo.green.offset, (int)scrinfo.green.length,
            (int)scrinfo.blue.offset, (int)scrinfo.blue.length
            );

    fbmmap = mmap(NULL, pixels * bytespp, PROT_READ, MAP_SHARED, fbfd, 0);

    if (fbmmap == MAP_FAILED)
    {
        error_print("mmap failed\n");
        exit(EXIT_FAILURE);
    }
}

static void cleanup_fb(void)
{
    if(fbfd != -1)
    {
        close(fbfd);
    }
}

#if 0
static void keyevent(rfbBool down, rfbKeySym key, rfbClientPtr cl)
{
    int scancode;

    debug_print("Got keysym: %04x (down=%d)\n", (unsigned int)key, (int)down);

    if ((scancode = keysym2scancode(down, key, cl)))
    {
        injectKeyEvent(scancode, down);
    }
}
#endif

static void ptrevent(int buttonMask, int x, int y, rfbClientPtr cl)
{
    /* Indicates either pointer movement or a pointer button press or release. The pointer is
now at (x-position, y-position), and the current state of buttons 1 to 8 are represented
by bits 0 to 7 of button-mask respectively, 0 meaning up, 1 meaning down (pressed).
On a conventional mouse, buttons 1, 2 and 3 correspond to the left, middle and right
buttons on the mouse. On a wheel mouse, each step of the wheel upwards is represented
by a press and release of button 4, and each step downwards is represented by
a press and release of button 5.
  From: http://www.vislab.usyd.edu.au/blogs/index.php/2009/05/22/an-headerless-indexed-protocol-for-input-1?blog=61 */

    //debug_print("Got ptrevent: %04x (x=%d, y=%d)\n", buttonMask, x, y);
    // Simulate left mouse event as touch event
    static int pressed = 0;
    if(buttonMask & 1)
    {
        if (pressed == 1)
        {
            // move
            injectTouchEvent(-1, x, y, &scrinfo);
        }
        else
        {
            // press
            pressed = 1;
            injectTouchEvent(1, x, y, &scrinfo);
        }
    }
    if(buttonMask == 0)
    {
        if (pressed == 1)
        {
            // release
            pressed = 0;
            injectTouchEvent(0, x, y, &scrinfo);
        }
    }
}

/*****************************************************************************/

static void init_fb_server(int argc, char **argv, rfbBool enable_touch)
{
    info_print("Initializing server...\n");

    /* Allocate the VNC server buffer to be managed (not manipulated) by
     * libvncserver. */
    vncbuf = calloc(scrinfo.xres * scrinfo.yres, bytespp);
    assert(vncbuf != NULL);

    /* Allocate the comparison buffer for detecting drawing updates from frame
     * to frame. */
    fbbuf = calloc(scrinfo.xres * scrinfo.yres, bytespp);
    assert(fbbuf != NULL);

    /* TODO: This assumes scrinfo.bits_per_pixel is 16. */
    server = rfbGetScreen(&argc, argv, scrinfo.xres, scrinfo.yres, BITS_PER_SAMPLE, SAMPLES_PER_PIXEL, bytespp);
    assert(server != NULL);

    server->desktopName = "framebuffer";
    server->frameBuffer = (char *)vncbuf;
    server->alwaysShared = TRUE;
    server->httpDir = NULL;
    server->port = vnc_port;

    //	server->kbdAddEvent = keyevent;
    if(enable_touch)
    {
        server->ptrAddEvent = ptrevent;
    }

    rfbInitServer(server);

    /* Mark as dirty since we haven't sent any updates at all yet. */
    rfbMarkRectAsModified(server, 0, 0, scrinfo.xres, scrinfo.yres);

    /* No idea. */
    varblock.r_offset = scrinfo.red.offset + scrinfo.red.length - BITS_PER_SAMPLE;
    varblock.g_offset = scrinfo.green.offset + scrinfo.green.length - BITS_PER_SAMPLE;
    varblock.b_offset = scrinfo.blue.offset + scrinfo.blue.length - BITS_PER_SAMPLE;
    varblock.rfb_xres = scrinfo.yres;
    varblock.rfb_maxy = scrinfo.xres - 1;
}

// sec
#define LOG_TIME    5

int timeToLogFPS() {
    static struct timeval now={0,0}, then={0,0};
    double elapsed, dnow, dthen;
    gettimeofday(&now,NULL);
    dnow  = now.tv_sec  + (now.tv_usec /1000000.0);
    dthen = then.tv_sec + (then.tv_usec/1000000.0);
    elapsed = dnow - dthen;
    if (elapsed > LOG_TIME)
      memcpy((char *)&then, (char *)&now, sizeof(struct timeval));
    return elapsed > LOG_TIME;
}

/*****************************************************************************/
//#define COLOR_MASK  0x1f001f
#define COLOR_MASK  (((1 << BITS_PER_SAMPLE) << 1) - 1)
#define PIXEL_FB_TO_RFB(p,r_offset,g_offset,b_offset) \
    ((p>>r_offset)&COLOR_MASK) | (((p>>g_offset)&COLOR_MASK)<<BITS_PER_SAMPLE) | (((p>>b_offset)&COLOR_MASK)<<(2*BITS_PER_SAMPLE))

static void update_screen(void)
{
#ifdef LOG_FPS
    static int frames = 0;
    frames++;
    if(timeToLogFPS())
    {
        double fps = frames / LOG_TIME;
        info_print("  fps: %f\n", fps);
        frames = 0;
    }
#endif

    varblock.min_i = varblock.min_j = 9999;
    varblock.max_i = varblock.max_j = -1;

    uint32_t *f = (uint32_t *)fbmmap;        /* -> framebuffer         */
    uint32_t *c = (uint32_t *)fbbuf;         /* -> compare framebuffer */
    uint32_t *r = (uint32_t *)vncbuf;        /* -> remote framebuffer  */

    int size = scrinfo.xres * scrinfo.yres * bytespp;
    if(memcmp ( fbmmap, fbbuf, size )!=0)
    {
//        memcpy(fbbuf, fbmmap, size);


    int xstep = 4/bytespp;

    int y;
    for (y = 0; y < (int)scrinfo.yres; y++)
    {
        /* Compare every 1/2/4 pixels at a time */
        int x;
        for (x = 0; x < (int)scrinfo.xres; x += xstep)
        {
            uint32_t pixel = *f;

            if (pixel != *c)
            {
                *c = pixel;

#if 0
                /* XXX: Undo the checkered pattern to test the efficiency
                 * gain using hextile encoding. */
                if (pixel == 0x18e320e4 || pixel == 0x20e418e3)
                    pixel = 0x18e318e3;
#endif
                *r = PIXEL_FB_TO_RFB(pixel,
                                     varblock.r_offset, varblock.g_offset, varblock.b_offset);
                if(bytespp==2)
                {
                    uint32_t high_pixel = (0xffff0000 & pixel) >> 16;
                    uint32_t high_r = PIXEL_FB_TO_RFB(high_pixel, varblock.r_offset, varblock.g_offset, varblock.b_offset);
                    *r |=  (0xffff & high_r) << 16;
                }
                else
                {
                    // TODO
                }

                if (x < varblock.min_i)
                    varblock.min_i = x;
                else
                {
                    if (x > varblock.max_i)
                        varblock.max_i = x;

                    if (y > varblock.max_j)
                        varblock.max_j = y;
                    else if (y < varblock.min_j)
                        varblock.min_j = y;
                }
            }

            f++;
            c++;
            r++;
        }
    }
    }

    if (varblock.min_i < 9999)
    {
        if (varblock.max_i < 0)
            varblock.max_i = varblock.min_i;

        if (varblock.max_j < 0)
            varblock.max_j = varblock.min_j;

        debug_print("Dirty page: %dx%d+%d+%d...\n",
                (varblock.max_i+2) - varblock.min_i, (varblock.max_j+1) - varblock.min_j,
                varblock.min_i, varblock.min_j);

        rfbMarkRectAsModified(server, varblock.min_i, varblock.min_j,
                              varblock.max_i + 2, varblock.max_j + 1);

        rfbProcessEvents(server, 10000);
    }
}

/*****************************************************************************/



void print_usage(char **argv)
{
    info_print("%s [-f device] [-p port] [-t touchscreen] [-h]\n"
                    "-p port: VNC port, default is 5900\n"
               "-f device: framebuffer device node, default is /dev/fb0\n"
               "-t device: touchscreen device node (example:/dev/input/event2)\n"
                    "-h : print this help\n"
            , *argv);
}

int main(int argc, char **argv)
{
    if(argc > 1)
    {
        int i=1;
        while(i < argc)
        {
            if(*argv[i] == '-')
            {
                switch(*(argv[i] + 1))
                {
                case 'h':
                    print_usage(argv);
                    exit(0);
                    break;
                case 'f':
                    i++;
                    strcpy(fb_device, argv[i]);
                    break;
                case 't':
                    i++;
                    strcpy(touch_device, argv[i]);
                    break;
                case 'p':
                    i++;
                    vnc_port = atoi(argv[i]);
                    break;
                }
            }
            i++;
        }
    }

    info_print("Initializing framebuffer device %s...\n", fb_device);
    init_fb();
//    init_kbd();

    rfbBool enable_touch = FALSE;
    if(strlen(touch_device) > 0)
    {
        // init touch only if there is a touch device defined
        int ret = init_touch(touch_device);
        enable_touch = (ret>0);
    }
    else
    {
        info_print("No touch device\n");
    }

    info_print("Initializing VNC server:\n");
    info_print("	width:  %d\n", (int)scrinfo.xres);
    info_print("	height: %d\n", (int)scrinfo.yres);
    info_print("	bpp:    %d\n", (int)scrinfo.bits_per_pixel);
    info_print("	port:   %d\n", (int)vnc_port);
    init_fb_server(argc, argv, enable_touch);

    /* Implement our own event loop to detect changes in the framebuffer. */
    while (1)
    {
        while (server->clientHead == NULL)
            rfbProcessEvents(server, 100000);

        rfbProcessEvents(server, 100000);
        update_screen();
    }

    info_print("Cleaning up...\n");
    cleanup_fb();
//    cleanup_kbd();
    cleanup_touch();
}
