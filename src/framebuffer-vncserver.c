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
#include <sys/sysmacros.h> /* For makedev() */

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>

#include <assert.h>
#include <errno.h>

/* libvncserver */
#include "rfb/rfb.h"
#include "rfb/keysym.h"

#include "touch.h"
#include "mouse.h"
#include "keyboard.h"
#include "logging.h"

/*****************************************************************************/
#define LOG_FPS

#define BITS_PER_SAMPLE 5
#define SAMPLES_PER_PIXEL 2

// #define CHANNELS_PER_PIXEL 4

static char fb_device[256] = "/dev/fb0";
static char touch_device[256] = "";
static char kbd_device[256] = "";
static char mouse_device[256] = "";

static struct fb_var_screeninfo var_scrinfo;
static struct fb_fix_screeninfo fix_scrinfo;
static int fbfd = -1;
static unsigned short int *fbmmap = MAP_FAILED;
static unsigned short int *vncbuf;
static unsigned short int *fbbuf;

static int vnc_port = 5900;
static int vnc_rotate = 0;
static int touch_rotate = -1;
static int target_fps = 10;
static rfbScreenInfoPtr server;
static size_t bytespp;
static unsigned int bits_per_pixel;
static unsigned int frame_size;
static unsigned int fb_xres;
static unsigned int fb_yres;
int verbose = 0;

#define UNUSED(x) (void)(x)

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

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &var_scrinfo) != 0)
    {
        error_print("ioctl error\n");
        exit(EXIT_FAILURE);
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fix_scrinfo) != 0)
    {
        error_print("ioctl error\n");
        exit(EXIT_FAILURE);
    }

    /*
     * Get actual resolution of the framebufffer, which is not always the same as the screen resolution.
     * This prevents the screen from 'smearing' on 1366 x 768 displays
     */

    fb_xres = fix_scrinfo.line_length / (var_scrinfo.bits_per_pixel / 8.0);
    fb_yres = var_scrinfo.yres;

    pixels = fb_xres * fb_yres;
    bytespp = var_scrinfo.bits_per_pixel / 8;
    bits_per_pixel = var_scrinfo.bits_per_pixel;
    frame_size = pixels * bits_per_pixel / 8;

    info_print("  xres=%d, yres=%d, xresv=%d, yresv=%d, xoffs=%d, yoffs=%d, bpp=%d\n",
               (int)fb_xres, (int)fb_yres,
               (int)var_scrinfo.xres_virtual, (int)var_scrinfo.yres_virtual,
               (int)var_scrinfo.xoffset, (int)var_scrinfo.yoffset,
               (int)var_scrinfo.bits_per_pixel);
    info_print("  offset:length red=%d:%d green=%d:%d blue=%d:%d \n",
               (int)var_scrinfo.red.offset, (int)var_scrinfo.red.length,
               (int)var_scrinfo.green.offset, (int)var_scrinfo.green.length,
               (int)var_scrinfo.blue.offset, (int)var_scrinfo.blue.length);

    fbmmap = mmap(NULL, frame_size, PROT_READ, MAP_SHARED, fbfd, 0);

    if (fbmmap == MAP_FAILED)
    {
        error_print("mmap failed\n");
        exit(EXIT_FAILURE);
    }
}

static void cleanup_fb(void)
{
    if (fbfd != -1)
    {
        close(fbfd);
        fbfd = -1;
    }
}

static void keyevent(rfbBool down, rfbKeySym key, rfbClientPtr cl)
{
    int scancode;

    debug_print("Got keysym: %04x (down=%d)\n", (unsigned int)key, (int)down);

    if ((scancode = keysym2scancode(key, cl)))
    {
        injectKeyEvent(scancode, down);
    }
}

static void ptrevent_touch(int buttonMask, int x, int y, rfbClientPtr cl)
{
    UNUSED(cl);
    /* Indicates either pointer movement or a pointer button press or release. The pointer is
now at (x-position, y-position), and the current state of buttons 1 to 8 are represented
by bits 0 to 7 of button-mask respectively, 0 meaning up, 1 meaning down (pressed).
On a conventional mouse, buttons 1, 2 and 3 correspond to the left, middle and right
buttons on the mouse. On a wheel mouse, each step of the wheel upwards is represented
by a press and release of button 4, and each step downwards is represented by
a press and release of button 5.
  From: http://www.vislab.usyd.edu.au/blogs/index.php/2009/05/22/an-headerless-indexed-protocol-for-input-1?blog=61 */

    debug_print("Got ptrevent: %04x (x=%d, y=%d)\n", buttonMask, x, y);
    // Simulate left mouse event as touch event
    static int pressed = 0;
    if (buttonMask & 1)
    {
        if (pressed == 1)
        {
            injectTouchEvent(MouseDrag, x, y, &var_scrinfo);
        }
        else
        {
            pressed = 1;
            injectTouchEvent(MousePress, x, y, &var_scrinfo);
        }
    }
    if (buttonMask == 0)
    {
        if (pressed == 1)
        {
            pressed = 0;
            injectTouchEvent(MouseRelease, x, y, &var_scrinfo);
        }
    }
}

static void ptrevent_mouse(int buttonMask, int x, int y, rfbClientPtr cl)
{
    UNUSED(cl);
    /* Indicates either pointer movement or a pointer button press or release. The pointer is
now at (x-position, y-position), and the current state of buttons 1 to 8 are represented
by bits 0 to 7 of button-mask respectively, 0 meaning up, 1 meaning down (pressed).
On a conventional mouse, buttons 1, 2 and 3 correspond to the left, middle and right
buttons on the mouse. On a wheel mouse, each step of the wheel upwards is represented
by a press and release of button 4, and each step downwards is represented by
a press and release of button 5.
  From: http://www.vislab.usyd.edu.au/blogs/index.php/2009/05/22/an-headerless-indexed-protocol-for-input-1?blog=61 */

    debug_print("Got mouse: %04x (x=%d, y=%d)\n", buttonMask, x, y);
    // Simulate left mouse event as touch event
    injectMouseEvent(&var_scrinfo, buttonMask, x, y);
}

/*****************************************************************************/

static void init_fb_server(int argc, char **argv, rfbBool enable_touch, rfbBool enable_mouse)
{
    info_print("Initializing server...\n");

    int rbytespp = bits_per_pixel == 1 ? 1 : bytespp;
    int rframe_size = bits_per_pixel == 1 ? frame_size * 8 : frame_size;
    /* Allocate the VNC server buffer to be managed (not manipulated) by
     * libvncserver. */
    vncbuf = malloc(rframe_size);
    assert(vncbuf != NULL);
    memset(vncbuf, bits_per_pixel == 1 ? 0xFF : 0x00, rframe_size);

    /* Allocate the comparison buffer for detecting drawing updates from frame
     * to frame. */
    fbbuf = calloc(frame_size, 1);
    assert(fbbuf != NULL);

    /* TODO: This assumes var_scrinfo.bits_per_pixel is 16. */
    server = rfbGetScreen(&argc, argv, fb_xres, fb_yres, BITS_PER_SAMPLE, SAMPLES_PER_PIXEL, rbytespp);
    assert(server != NULL);

    server->desktopName = "framebuffer";
    server->frameBuffer = (char *)vncbuf;
    server->alwaysShared = TRUE;
    server->httpDir = NULL;
    server->port = vnc_port;

    server->kbdAddEvent = keyevent;
    if (enable_touch)
    {
        server->ptrAddEvent = ptrevent_touch;
    }

    if (enable_mouse)
    {
        server->ptrAddEvent = ptrevent_mouse;
    }
    

    rfbInitServer(server);

    /* Mark as dirty since we haven't sent any updates at all yet. */
    rfbMarkRectAsModified(server, 0, 0, fb_xres, fb_yres);

    /* No idea. */
    varblock.r_offset = var_scrinfo.red.offset + var_scrinfo.red.length - BITS_PER_SAMPLE;
    varblock.g_offset = var_scrinfo.green.offset + var_scrinfo.green.length - BITS_PER_SAMPLE;
    varblock.b_offset = var_scrinfo.blue.offset + var_scrinfo.blue.length - BITS_PER_SAMPLE;
    varblock.rfb_xres = fb_yres;
    varblock.rfb_maxy = fb_xres - 1;
}

// sec
#define LOG_TIME 5

int timeToLogFPS()
{
    static struct timeval now = {0, 0}, then = {0, 0};
    double elapsed, dnow, dthen;
    gettimeofday(&now, NULL);
    dnow = now.tv_sec + (now.tv_usec / 1000000.0);
    dthen = then.tv_sec + (then.tv_usec / 1000000.0);
    elapsed = dnow - dthen;
    if (elapsed > LOG_TIME)
        memcpy((char *)&then, (char *)&now, sizeof(struct timeval));
    return elapsed > LOG_TIME;
}

/*****************************************************************************/
//#define COLOR_MASK  0x1f001f
#define COLOR_MASK (((1 << BITS_PER_SAMPLE) << 1) - 1)
#define PIXEL_FB_TO_RFB(p, r_offset, g_offset, b_offset) \
    ((p >> r_offset) & COLOR_MASK) | (((p >> g_offset) & COLOR_MASK) << BITS_PER_SAMPLE) | (((p >> b_offset) & COLOR_MASK) << (2 * BITS_PER_SAMPLE))

static void update_screen(void)
{
#ifdef LOG_FPS
    if (verbose)
    {
        static int frames = 0;
        frames++;
        if (timeToLogFPS())
        {
            double fps = frames / LOG_TIME;
            info_print("  fps: %f\n", fps);
            frames = 0;
        }
    }
#endif

    varblock.min_i = varblock.min_j = 9999;
    varblock.max_i = varblock.max_j = -1;

    if (vnc_rotate == 0 && bits_per_pixel == 24)
    {
        uint8_t *f = (uint8_t *)fbmmap; /* -> framebuffer         */
        uint8_t *c = (uint8_t *)fbbuf;  /* -> compare framebuffer */
        uint8_t *r = (uint8_t *)vncbuf; /* -> remote framebuffer  */

        if (memcmp(fbmmap, fbbuf, frame_size) != 0)
        {
            int y;
            for (y = 0; y < (int)fb_yres; y++)
            {
                int x;
                for (x = 0; x < (int)fb_xres; x++)
                {
                    uint32_t pixel = *(uint32_t *)f & 0x00FFFFFF;
                    uint32_t comp = *(uint32_t *)c & 0x00FFFFFF;

                    if (pixel != comp)
                    {
                        *(c + 0) = *(f + 0);
                        *(c + 1) = *(f + 1);
                        *(c + 2) = *(f + 2);
                        uint32_t rem = PIXEL_FB_TO_RFB(pixel,
                                                       varblock.r_offset, varblock.g_offset, varblock.b_offset);
                        *(r + 0) = (uint8_t)((rem >> 0) & 0xFF);
                        *(r + 1) = (uint8_t)((rem >> 8) & 0xFF);
                        *(r + 2) = (uint8_t)((rem >> 16) & 0xFF);

                        if (x < varblock.min_i)
                            varblock.min_i = x;
                        else if (x > varblock.max_i)
                            varblock.max_i = x;

                        if (y > varblock.max_j)
                            varblock.max_j = y;
                        else if (y < varblock.min_j)
                            varblock.min_j = y;
                    }

                    f += bytespp;
                    c += bytespp;
                    r += bytespp;
                }
            }
        }
    }
    else if (vnc_rotate == 0 && bits_per_pixel == 1)
    {
        uint8_t *f = (uint8_t *)fbmmap; /* -> framebuffer         */
        uint8_t *c = (uint8_t *)fbbuf;  /* -> compare framebuffer */
        uint8_t *r = (uint8_t *)vncbuf; /* -> remote framebuffer  */

        int xstep = 8;
        if (memcmp(fbmmap, fbbuf, frame_size) != 0)
        {
            int y;
            for (y = 0; y < (int)fb_yres; y++)
            {
                int x;
                for (x = 0; x < (int)fb_xres; x += xstep)
                {
                    uint8_t pixels = *f;

                    if (pixels != *c)
                    {
                        *c = pixels;

                        for (int bit = 0; bit < 8; bit++)
                        {
                            // *(r+bit) = ((pixels >> (7-bit)) & 0x1) ? 0xFF : 0x00;
                            *(r + bit) = ((pixels >> (7 - bit)) & 0x1) ? 0x00 : 0xFF;
                        }

                        int x2 = x + xstep - 1;
                        if (x < varblock.min_i)
                            varblock.min_i = x;
                        else if (x2 > varblock.max_i)
                            varblock.max_i = x2;

                        if (y > varblock.max_j)
                            varblock.max_j = y;
                        else if (y < varblock.min_j)
                            varblock.min_j = y;
                    }

                    f += 1;
                    c += 1;
                    r += 8;
                }
            }
        }
    }
    else if (vnc_rotate == 0)
    {
        uint32_t *f = (uint32_t *)fbmmap; /* -> framebuffer         */
        uint32_t *c = (uint32_t *)fbbuf;  /* -> compare framebuffer */
        uint32_t *r = (uint32_t *)vncbuf; /* -> remote framebuffer  */

        if (memcmp(fbmmap, fbbuf, frame_size) != 0)
        {
            //        memcpy(fbbuf, fbmmap, size);

            int xstep = 4 / bytespp;

            int y;
            for (y = 0; y < (int)fb_yres; y++)
            {
                /* Compare every 1/2/4 pixels at a time */
                int x;
                for (x = 0; x < (int)fb_xres; x += xstep)
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
                        if (bytespp == 4)
                        {
                            *r = PIXEL_FB_TO_RFB(pixel,
                                                 varblock.r_offset, varblock.g_offset, varblock.b_offset);
                        }
                        else if (bytespp == 2)
                        {
                            *r = PIXEL_FB_TO_RFB(pixel,
                                                 varblock.r_offset, varblock.g_offset, varblock.b_offset);

                            uint32_t high_pixel = (0xffff0000 & pixel) >> 16;
                            uint32_t high_r = PIXEL_FB_TO_RFB(high_pixel, varblock.r_offset, varblock.g_offset, varblock.b_offset);
                            *r |= (0xffff & high_r) << 16;
                        }
                        else if (bytespp == 1)
                        {
                            *r = pixel;
                        }
                        else
                        {
                            // TODO
                        }

                        int x2 = x + xstep - 1;
                        if (x < varblock.min_i)
                            varblock.min_i = x;
                        else if (x2 > varblock.max_i)
                            varblock.max_i = x2;

                        if (y > varblock.max_j)
                            varblock.max_j = y;
                        else if (y < varblock.min_j)
                            varblock.min_j = y;
                    }

                    f++;
                    c++;
                    r++;
                }
            }
        }
    }
    else if (bits_per_pixel == 16)
    {
        uint16_t *f = (uint16_t *)fbmmap; /* -> framebuffer         */
        uint16_t *c = (uint16_t *)fbbuf;  /* -> compare framebuffer */
        uint16_t *r = (uint16_t *)vncbuf; /* -> remote framebuffer  */

        switch (vnc_rotate)
        {
        case 0:
        case 180:
            server->width = fb_xres;
            server->height = fb_yres;
            server->paddedWidthInBytes = fb_xres * bytespp;
            break;

        case 90:
        case 270:
            server->width = fb_yres;
            server->height = fb_xres;
            server->paddedWidthInBytes = fb_yres * bytespp;
            break;
        }

        if (memcmp(fbmmap, fbbuf, frame_size) != 0)
        {
            int y;
            for (y = 0; y < (int)fb_yres; y++)
            {
                /* Compare every pixels at a time */
                int x;
                for (x = 0; x < (int)fb_xres; x++)
                {
                    uint16_t pixel = *f;

                    if (pixel != *c)
                    {
                        int x2, y2;

                        *c = pixel;
                        switch (vnc_rotate)
                        {
                        case 0:
                            x2 = x;
                            y2 = y;
                            break;

                        case 90:
                            x2 = fb_yres - 1 - y;
                            y2 = x;
                            break;

                        case 180:
                            x2 = fb_xres - 1 - x;
                            y2 = fb_yres - 1 - y;
                            break;

                        case 270:
                            x2 = y;
                            y2 = fb_xres - 1 - x;
                            break;
                        default:
                            error_print("rotation is invalid\n");
                            exit(EXIT_FAILURE);
                        }

                        r[y2 * server->width + x2] = PIXEL_FB_TO_RFB(pixel, varblock.r_offset, varblock.g_offset, varblock.b_offset);

                        if (x2 < varblock.min_i)
                            varblock.min_i = x2;
                        else
                        {
                            if (x2 > varblock.max_i)
                                varblock.max_i = x2;

                            if (y2 > varblock.max_j)
                                varblock.max_j = y2;
                            else if (y2 < varblock.min_j)
                                varblock.min_j = y2;
                        }
                    }

                    f++;
                    c++;
                }
            }
        }
    }
    else
    {
        error_print("not supported color depth or rotation\n");
        exit(EXIT_FAILURE);
    }

    if (varblock.min_i < 9999)
    {
        if (varblock.max_i < 0)
            varblock.max_i = varblock.min_i;

        if (varblock.max_j < 0)
            varblock.max_j = varblock.min_j;

        debug_print("Dirty page: %dx%d+%d+%d...\n",
                    (varblock.max_i + 2) - varblock.min_i, (varblock.max_j + 1) - varblock.min_j,
                    varblock.min_i, varblock.min_j);

        rfbMarkRectAsModified(server, varblock.min_i, varblock.min_j,
                              varblock.max_i + 2, varblock.max_j + 1);
    }
}

/*****************************************************************************/

void print_usage(char **argv)
{
    info_print("%s [-f device] [-p port] [-t touchscreen] [-m mouse] [-k keyboard] [-r rotation] [-R touchscreen rotation] [-F FPS] [-v] [-h]\n"
               "-p port: VNC port, default is 5900\n"
               "-f device: framebuffer device node, default is /dev/fb0\n"
               "-k device: keyboard device node (example: /dev/input/event0)\n"
               "-t device: touchscreen device node (example:/dev/input/event2)\n"
               "-m device: mouse device node (example:/dev/input/event2)\n"
               "-r degrees: framebuffer rotation, default is 0\n"
               "-R degrees: touchscreen rotation, default is same as framebuffer rotation\n"
               "-F FPS: Maximum target FPS, default is 10\n"
               "-v: verbose\n"
               "-h: print this help\n",
               *argv);
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        int i = 1;
        while (i < argc)
        {
            if (*argv[i] == '-')
            {
                switch (*(argv[i] + 1))
                {
                case 'h':
                    print_usage(argv);
                    exit(0);
                    break;
                case 'f':
                    i++;
                    if (argv[i])
                        strcpy(fb_device, argv[i]);
                    break;
                case 't':
                    i++;
                    if (argv[i])
                        strcpy(touch_device, argv[i]);
                    break;
                case 'm':
                    i++;
                    if (argv[i])
                        strcpy(mouse_device, argv[i]);
                    break;                    
                case 'k':
                    i++;
                    strcpy(kbd_device, argv[i]);
                    break;
                case 'p':
                    i++;
                    if (argv[i])
                        vnc_port = atoi(argv[i]);
                    break;
                case 'r':
                    i++;
                    if (argv[i])
                        vnc_rotate = atoi(argv[i]);
                    break;
                case 'R':
                    i++;
                    if (argv[i])
                        touch_rotate = atoi(argv[i]);
                    break;
               case 'F':
                    i++;
                    if (argv[i])
                        target_fps = atoi(argv[i]);
                    break;
                case 'v':
                    verbose = 1;
                    break;
                }
            }
            i++;
        }
    }

    if (touch_rotate < 0)
        touch_rotate = vnc_rotate;

    info_print("Initializing framebuffer device %s...\n", fb_device);
    init_fb();
    if (strlen(kbd_device) > 0)
    {
        int ret = init_kbd(kbd_device);
        if (!ret)
            info_print("Keyboard device %s not available.\n", kbd_device);
    }
    else
    {
        info_print("No keyboard device\n");
    }

    rfbBool enable_touch = FALSE;
    rfbBool enable_mouse = FALSE;
    if(strlen(touch_device) > 0 && strlen(mouse_device) > 0)
    {
        error_print("It can't using both mouse and touch device.\n");
        exit(EXIT_FAILURE);
    }
    else if (strlen(touch_device) > 0)
    {
        // init touch only if there is a touch device defined
        int ret = init_touch(touch_device, touch_rotate);
        enable_touch = (ret > 0);
    }
    else if(strlen(mouse_device) > 0)
    {
        // init touch only if there is a mouse device defined
        int ret = init_mouse(mouse_device, touch_rotate);
        enable_mouse = (ret > 0);        
    }
    else
    {
        info_print("No touch or mouse device\n");
    }

    info_print("Initializing VNC server:\n");
    info_print("	width:  %d\n", (int)fb_xres);
    info_print("	height: %d\n", (int)fb_yres);
    info_print("	bpp:    %d\n", (int)var_scrinfo.bits_per_pixel);
    info_print("	port:   %d\n", (int)vnc_port);
    info_print("	rotate: %d\n", (int)vnc_rotate);
    info_print("  mouse/touch rotate: %d\n", (int)touch_rotate);
    info_print("    target FPS: %d\n", (int)target_fps);
    init_fb_server(argc, argv, enable_touch, enable_mouse);

    /* Implement our own event loop to detect changes in the framebuffer. */
    while (1)
    {
        rfbRunEventLoop(server, 100 * 1000, TRUE);
        while (rfbIsActive(server))
        {
            if (server->clientHead != NULL)
                update_screen();

            if (target_fps > 0)
                usleep(1000 * 1000 / target_fps);
            else if (server->clientHead == NULL)
                usleep(100 * 1000);
        }
    }

    info_print("Cleaning up...\n");
    cleanup_fb();
    cleanup_kbd();
    cleanup_touch();
}
