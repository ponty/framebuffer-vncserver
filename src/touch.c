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

#include "touch.h"

static char TOUCH_DEVICE[256] = "/dev/input/event2";
static int touchfd = -1;

static int xmin, xmax;
static int ymin, ymax;

void init_touch()
{
    fprintf(stderr, "Initializing touch device %s ...\n", TOUCH_DEVICE);
    struct input_absinfo info;
    if((touchfd = open(TOUCH_DEVICE, O_RDWR)) == -1)
    {
        fprintf(stderr, "cannot open touch device %s\n", TOUCH_DEVICE);
        exit(EXIT_FAILURE);
    }
    // Get the Range of X and Y
    if(ioctl(touchfd, EVIOCGABS(ABS_X), &info)) {
        fprintf(stderr, "cannot get ABS_X info, %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    xmin = info.minimum;
    xmax = info.maximum;
    if(ioctl(touchfd, EVIOCGABS(ABS_Y), &info)) {
        fprintf(stderr, "cannot get ABS_Y, %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    ymin = info.minimum;
    ymax = info.maximum;

    fprintf(stderr, "  x:(%d %d)  y:(%d %d) \n", xmin, xmax, ymin, ymax );
}

void cleanup_touch()
{
    if(touchfd != -1)
    {
        close(touchfd);
    }
}

void injectTouchEvent(int down, int x, int y, struct fb_var_screeninfo* scrinfo)
{
    struct input_event ev;
    int xin = x;
    int yin = y;

    // Calculate the final x and y
    /* Fake touch screen always reports zero */
//???//    if (xmin != 0 && xmax != 0 && ymin != 0 && ymax != 0)
    {
        x = xmin + (x * (xmax - xmin)) / (scrinfo->xres);
        y = ymin + (y * (ymax - ymin)) / (scrinfo->yres);
    }

    memset(&ev, 0, sizeof(ev));

    if(down >= 0)
    {
        // Then send a BTN_TOUCH
        gettimeofday(&ev.time,0);
        ev.type = EV_KEY;
        ev.code = BTN_TOUCH;
        ev.value = down;
        if(write(touchfd, &ev, sizeof(ev)) < 0)
        {
            fprintf(stderr, "write event failed, %s\n", strerror(errno));
        }
    }

    // Then send the X
    gettimeofday(&ev.time,0);
    ev.type = EV_ABS;
    ev.code = ABS_X;
    ev.value = x;
    if(write(touchfd, &ev, sizeof(ev)) < 0)
    {
        fprintf(stderr, "write event failed, %s\n", strerror(errno));
    }

    // Then send the Y
    gettimeofday(&ev.time,0);
    ev.type = EV_ABS;
    ev.code = ABS_Y;
    ev.value = y;
    if(write(touchfd, &ev, sizeof(ev)) < 0)
    {
        fprintf(stderr, "write event failed, %s\n", strerror(errno));
    }

    // Finally send the SYN
    gettimeofday(&ev.time,0);
    ev.type = EV_SYN;
    ev.code = 0;
    ev.value = 0;
    if(write(touchfd, &ev, sizeof(ev)) < 0)
    {
        fprintf(stderr, "write event failed, %s\n", strerror(errno));
    }

    fprintf(stderr, "injectTouchEvent (screen(%d,%d) -> touch(%d,%d), down=%d)\n", xin , yin, x , y, down);
}
