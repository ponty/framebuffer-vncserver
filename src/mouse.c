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

#include "mouse.h"
#include "logging.h"


static int mousefd = -1;

static int xmin, xmax;
static int ymin, ymax;
static int rotate;
static int trkg_id = -1;
static bool is_wheel_hires = false;

#ifndef input_event_sec
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
#endif

typedef struct
{
    const char *name;
    const int value;
} map_t;

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MAP(x) { #x,x }
#define WHEEL_UP 3
#define WHEEL_DOWN 4

int init_mouse(const char *touch_device, int vnc_rotate)
{
	int size = (REL_CNT/32) + 1;
	unsigned int evtype_bitmask[size];
	/* Clean evtype_bitmask structure */
	memset(evtype_bitmask, 0, sizeof(evtype_bitmask));

    info_print("Initializing mouse device %s ...\n", touch_device);
    struct input_absinfo info;
    if ((mousefd = open(touch_device, O_RDWR)) == -1)
    {
        error_print("cannot open mouse device %s\n", touch_device);
        return 0;
    }

	//REL_WHEEL_HI_RES
	if (ioctl(mousefd, EVIOCGBIT(EV_REL, sizeof(evtype_bitmask)), evtype_bitmask) < 0) {
		error_print("%s  can't get evdev features: %s",touch_device, strerror(errno));
		return 0;
	}

#ifdef REL_WHEEL_HI_RES
	int index = REL_WHEEL_HI_RES/32;
    int offset = REL_WHEEL_HI_RES - (index*32);
	if(CHECK_BIT(evtype_bitmask[index],offset))
	{
		info_print("%s has hi res wheel.\n",touch_device);
		is_wheel_hires = true;
	}
#endif 

    // Get the Range of X and Y
    if (ioctl(mousefd, EVIOCGABS(ABS_X), &info))
    {
        error_print("cannot get ABS_X info, %s\n", strerror(errno));
        return 0;
    }
    xmin = info.minimum;
    xmax = info.maximum;
    if (ioctl(mousefd, EVIOCGABS(ABS_Y), &info))
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

void cleanup_mouse()
{
    if (mousefd != -1)
    {
        close(mousefd);
    }
}

void injectMouseEvent(struct fb_var_screeninfo *scrinfo, int buttonMask, int x, int y)
{
    /* Indicates either pointer movement or a pointer button press or release. The pointer is
    now at (x-position, y-position), and the current state of buttons 1 to 8 are represented
    by bits 0 to 7 of button-mask respectively, 0 meaning up, 1 meaning down (pressed).
    On a conventional mouse, buttons 1, 2 and 3 correspond to the left, middle and right
    buttons on the mouse. On a wheel mouse, each step of the wheel upwards is represented
    by a press and release of button 4, and each step downwards is represented by
    a press and release of button 5.
      From: http://www.vislab.usyd.edu.au/blogs/index.php/2009/05/22/an-headerless-indexed-protocol-for-input-1?blog=61 */
    
    static map_t mouseButtonMap[] = {
        MAP(BTN_LEFT),
        MAP(BTN_MIDDLE),
        MAP(BTN_RIGHT),
    };
    static int buttonsNumber = sizeof(mouseButtonMap) / sizeof(mouseButtonMap[0]);
    static int last_buttonMask;
    static int last_x;
    static int last_y;
    static int wheel_tick;
	int last_wheel_tick = 0;
    
    
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
    struct timeval time;

    // Send buttons state
    int buttonsChanged = last_buttonMask ^ buttonMask;
    if (buttonsChanged)
    {
        for (int bi = 0; bi < buttonsNumber; bi++)
        {
            int isChanged = CHECK_BIT(buttonsChanged,bi);
            int isPressed = CHECK_BIT(buttonMask,bi);

            if(isChanged)
            {
                // Then send a BTN_* 
                gettimeofday(&time, 0);
                ev.input_event_sec = time.tv_sec;
                ev.input_event_usec = time.tv_usec;
                ev.type = EV_KEY;
                ev.code =  mouseButtonMap[bi].value;
                ev.value = isPressed;
                if (write(mousefd, &ev, sizeof(ev)) < 0)
                {
                    error_print("write event failed, %s\n", strerror(errno));
                }
                info_print("Button %s=%04X\n",mouseButtonMap[bi].name, mouseButtonMap[bi].value);
            }
        }
        
        if(CHECK_BIT(buttonMask,WHEEL_UP))
        {
            wheel_tick++;
        }
        else if(CHECK_BIT(buttonMask,WHEEL_DOWN))
        {
            wheel_tick--;
        }

        if(wheel_tick)
        {
            // Then send the WHEEL
            gettimeofday(&time, 0);
            ev.input_event_sec = time.tv_sec;
            ev.input_event_usec = time.tv_usec;
            ev.type = EV_REL;
			if(is_wheel_hires)
			{
#ifdef REL_WHEEL_HI_RES

				info_print("HI RES WHEEL %d\n", wheel_tick);
            	ev.code = REL_WHEEL_HI_RES;
            	ev.value = wheel_tick*120;
#endif 
			}
			else
			{
				info_print("WHEEL %d\n", wheel_tick);
    	        ev.code = REL_WHEEL;
        	    ev.value = wheel_tick;
			}	
            if (write(mousefd, &ev, sizeof(ev)) < 0)
            {
                error_print("write event failed, %s\n", strerror(errno));
            }
			last_wheel_tick = wheel_tick;
            wheel_tick = 0;
        }
        last_buttonMask = buttonMask;
    }

    int dx = x - last_x;
    int dy = y - last_y;

    if (dx)
    {
        // Then send the X
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;
        ev.input_event_usec = time.tv_usec;
        ev.type = EV_ABS;
        ev.code = ABS_X;
        ev.value = x;
        if (write(mousefd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }
        last_x = x;
    }

    if(dy)
    {
        // Then send the Y
        gettimeofday(&time, 0);
        ev.input_event_sec = time.tv_sec;
        ev.input_event_usec = time.tv_usec;
        ev.type = EV_ABS;
        ev.code = ABS_Y;
        ev.value = y;
        if (write(mousefd, &ev, sizeof(ev)) < 0)
        {
            error_print("write event failed, %s\n", strerror(errno));
        }
        last_y = y;
    }

    // Finally send the SYN
    gettimeofday(&time, 0);
    ev.input_event_sec = time.tv_sec;
    ev.input_event_usec = time.tv_usec;
    ev.type = EV_SYN;
    ev.code = 0;
    ev.value = 0;
    if (write(mousefd, &ev, sizeof(ev)) < 0)
    {
        error_print("write event failed, %s\n", strerror(errno));
    }
    debug_print("injectMouseEvent (screen(%d,%d) -> mouse(%d,%d), button=%d, wheel tick=%d)\n", xin, yin, x, y, buttonMask, last_wheel_tick);
}
