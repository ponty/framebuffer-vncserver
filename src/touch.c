#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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

#include "touch.h"
#include "logging.h"

//static char TOUCH_DEVICE[256] = "/dev/input/event2";
static int touchfd = -1;

static int xmin, xmax;
static int ymin, ymax;
static int rotate;
static int trkg_id = -1;

#ifndef input_event_sec
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
#endif

int init_touch(const char *touch_device, int vnc_rotate)
{
    info_print("Initializing touch device %s ...\n", touch_device);
    struct input_absinfo info;
    if ((touchfd = open(touch_device, O_RDWR)) == -1)
    {
        error_print("cannot open touch device %s\n", touch_device);
        return 0;
    }
    // Get the Range of X and Y
    if (ioctl(touchfd, EVIOCGABS(ABS_X), &info))
    {
        error_print("cannot get ABS_X info, %s\n", strerror(errno));
        return 0;
    }
    xmin = info.minimum;
    xmax = info.maximum;
    if (ioctl(touchfd, EVIOCGABS(ABS_Y), &info))
    {
        error_print("cannot get ABS_Y, %s\n", strerror(errno));
        return 0;
    }
    ymin = info.minimum;
    ymax = info.maximum;
    rotate = vnc_rotate;

    info_print("  x:(%d %d)  y:(%d %d) \n", xmin, xmax, ymin, ymax);
    return 1;
}

void cleanup_touch()
{
    if (touchfd != -1)
    {
        close(touchfd);
    }
}

void injectTouchEvent(enum MouseAction mouseAction, int x, int y, struct fb_var_screeninfo *scrinfo)
{
    struct input_event ev;
    int xin = x;
    int yin = y;

    switch (rotate)
    {
    case 90:
        x = yin;
        y = scrinfo->yres - 1 - xin;
        break;
    case 180:
        x = scrinfo->xres - 1 - xin;
        y = scrinfo->yres - 1 - yin;
        break;
    case 270:
        x = scrinfo->xres - 1 - yin;
        y = xin;
        break;
    }

    // Calculate the final x and y
    /* Fake touch screen always reports zero */
    //???//if (xmin != 0 && xmax != 0 && ymin != 0 && ymax != 0)
    {
        x = xmin + (x * (xmax - xmin)) / (scrinfo->xres);
        y = ymin + (y * (ymax - ymin)) / (scrinfo->yres);
    }

    memset(&ev, 0, sizeof(ev));

    bool sendPos;
    bool sendTouch;
    int trkIdValue;
    int touchValue;
    struct timeval time;

    switch (mouseAction)
    {
    case MousePress:
        sendPos = true;
        sendTouch = true;
        trkIdValue = ++trkg_id;
        touchValue = 1;
        break;
    case MouseRelease:
        sendPos = false;
        sendTouch = true;
        trkIdValue = -1;
        touchValue = 0;
        break;
    case MouseDrag:
        sendPos = true;
        sendTouch = false;
        break;
    default:
        error_print("invalid mouse action\n");
        exit(EXIT_FAILURE);
    }

    if (sendTouch)
    {
        // Then send a ABS_MT_TRACKING_ID
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;

        ev.type = EV_ABS;
        ev.code = ABS_MT_TRACKING_ID;
        ev.value = trkIdValue;
        if (write(touchfd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }

        // Then send a BTN_TOUCH
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;
        ev.input_event_usec = time.tv_usec;
        ev.type = EV_KEY;
        ev.code = BTN_TOUCH;
        ev.value = touchValue;
        if (write(touchfd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }
    }

    if (sendPos)
    {
        // Then send a ABS_MT_POSITION_X
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;
        ev.input_event_usec = time.tv_usec;
        ev.type = EV_ABS;
        ev.code = ABS_MT_POSITION_X;
        ev.value = x;
        if (write(touchfd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }

        // Then send a ABS_MT_POSITION_Y
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;
        ev.input_event_usec = time.tv_usec;
        ev.type = EV_ABS;
        ev.code = ABS_MT_POSITION_Y;
        ev.value = y;
        if (write(touchfd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }

        // Then send the X
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;
        ev.input_event_usec = time.tv_usec;
        ev.type = EV_ABS;
        ev.code = ABS_X;
        ev.value = x;
        if (write(touchfd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }

        // Then send the Y
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;
        ev.input_event_usec = time.tv_usec;
        ev.type = EV_ABS;
        ev.code = ABS_Y;
        ev.value = y;
        if (write(touchfd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }
    }

    // Finally send the SYN
    gettimeofday(&time, 0);
    ev.input_event_sec = time.tv_sec;
    ev.input_event_usec = time.tv_usec;
    ev.type = EV_SYN;
    ev.code = 0;
    ev.value = 0;
    if (write(touchfd, &ev, sizeof(ev)) < 0)
    {
        error_print("write event failed, %s\n", strerror(errno));
    }
    debug_print("injectTouchEvent (screen(%d,%d) -> touch(%d,%d), mouse=%d)\n", xin, yin, x, y, mouseAction);
}
