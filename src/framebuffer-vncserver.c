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
#include "rfb/keysym.h"

/*****************************************************************************/
#define BITS_PER_SAMPLE     5
#define SAMPLES_PER_PIXEL   2

static char fb_device[256] = "/dev/fb0";
static struct fb_var_screeninfo scrinfo;
static int fbfd = -1;
static unsigned short int *fbmmap = MAP_FAILED;
static unsigned short int *vncbuf;
static unsigned short int *fbbuf;

static int vnc_port = 5900;
static rfbScreenInfoPtr server;


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
    size_t bytespp;

    if ((fbfd = open(fb_device, O_RDONLY)) == -1)
    {
        fprintf(stderr, "cannot open fb device %s\n", fb_device);
        exit(EXIT_FAILURE);
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &scrinfo) != 0)
    {
        fprintf(stderr, "ioctl error\n");
        exit(EXIT_FAILURE);
    }

    pixels = scrinfo.xres * scrinfo.yres;
    bytespp = scrinfo.bits_per_pixel / 8;

    fprintf(stderr, "xres=%d, yres=%d, xresv=%d, yresv=%d, xoffs=%d, yoffs=%d, bpp=%d\n",
            (int)scrinfo.xres, (int)scrinfo.yres,
            (int)scrinfo.xres_virtual, (int)scrinfo.yres_virtual,
            (int)scrinfo.xoffset, (int)scrinfo.yoffset,
            (int)scrinfo.bits_per_pixel);
    fprintf(stderr, "offset:length red=%d:%d green=%d:%d blue=%d:%d \n",
            (int)scrinfo.red.offset, (int)scrinfo.red.length,
            (int)scrinfo.green.offset, (int)scrinfo.green.length,
            (int)scrinfo.blue.offset, (int)scrinfo.blue.length
            );

    fbmmap = mmap(NULL, pixels * bytespp, PROT_READ, MAP_SHARED, fbfd, 0);

    if (fbmmap == MAP_FAILED)
    {
        fprintf(stderr, "mmap failed\n");
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

/*****************************************************************************/

static void init_fb_server(int argc, char **argv)
{
    fprintf(stderr, "Initializing server...\n");

    /* Allocate the VNC server buffer to be managed (not manipulated) by
     * libvncserver. */
    vncbuf = calloc(scrinfo.xres * scrinfo.yres, scrinfo.bits_per_pixel / 8);
    assert(vncbuf != NULL);

    /* Allocate the comparison buffer for detecting drawing updates from frame
     * to frame. */
    fbbuf = calloc(scrinfo.xres * scrinfo.yres, scrinfo.bits_per_pixel / 8);
    assert(fbbuf != NULL);

    /* TODO: This assumes scrinfo.bits_per_pixel is 16. */
    server = rfbGetScreen(&argc, argv, scrinfo.xres, scrinfo.yres, BITS_PER_SAMPLE, SAMPLES_PER_PIXEL, (scrinfo.bits_per_pixel / 8));
    assert(server != NULL);

    server->desktopName = "framebuffer";
    server->frameBuffer = (char *)vncbuf;
    server->alwaysShared = TRUE;
    server->httpDir = NULL;
    server->port = vnc_port;

    //	server->kbdAddEvent = keyevent;
    //	server->ptrAddEvent = ptrevent;

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

/*****************************************************************************/
#define COLOR_MASK  0x1f001f
#define PIXEL_FB_TO_RFB(p,r_offset,g_offset,b_offset) ((p>>r_offset)&COLOR_MASK) | (((p>>g_offset)&COLOR_MASK)<<BITS_PER_SAMPLE) | (((p>>b_offset)&COLOR_MASK)<<(2*BITS_PER_SAMPLE))

static void update_screen(void)
{
    unsigned int *f, *c, *r;
    int x, y;

    varblock.min_i = varblock.min_j = 9999;
    varblock.max_i = varblock.max_j = -1;

    f = (unsigned int *)fbmmap;        /* -> framebuffer         */
    c = (unsigned int *)fbbuf;         /* -> compare framebuffer */
    r = (unsigned int *)vncbuf;        /* -> remote framebuffer  */

    int multiplier = 1;
    if (scrinfo.bits_per_pixel == 32)
    {
        // HACK: support for 32 bit
        multiplier = 2;
    }
    for (y = 0; y < (int)(scrinfo.yres * multiplier); y++)
    {
        /* Compare every 2 pixels at a time, assuming that changes are likely
         * in pairs. */
        for (x = 0; x < (int)scrinfo.xres; x += 2)
        {
            unsigned int pixel = *f;

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

    if (varblock.min_i < 9999)
    {
        if (varblock.max_i < 0)
            varblock.max_i = varblock.min_i;

        if (varblock.max_j < 0)
            varblock.max_j = varblock.min_j;

        fprintf(stderr, "Dirty page: %dx%d+%d+%d...\n",
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
    fprintf(stderr, "%s [-f device] [-p port] [-h]\n"
                    "-p port: VNC port, default is 5900\n"
                    "-f device: framebuffer device node, default is /dev/fb0\n"
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
                case 'p':
                    i++;
                    vnc_port = atoi(argv[i]);
                    break;
                }
            }
            i++;
        }
    }

    fprintf(stderr, "Initializing framebuffer device %s...\n", fb_device);
    init_fb();

    fprintf(stderr, "Initializing VNC server:\n");
    fprintf(stderr, "	width:  %d\n", (int)scrinfo.xres);
    fprintf(stderr, "	height: %d\n", (int)scrinfo.yres);
    fprintf(stderr, "	bpp:    %d\n", (int)scrinfo.bits_per_pixel);
    fprintf(stderr, "	port:   %d\n", (int)vnc_port);
    init_fb_server(argc, argv);

    /* Implement our own event loop to detect changes in the framebuffer. */
    while (1)
    {
        while (server->clientHead == NULL)
            rfbProcessEvents(server, 100000);

        rfbProcessEvents(server, 100000);
        update_screen();
    }

    fprintf(stderr, "Cleaning up...\n");
    cleanup_fb();
}
