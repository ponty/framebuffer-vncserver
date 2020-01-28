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

#include "keyboard.h"
#include "logging.h"

//static char KBD_DEVICE[256] = "/dev/input/event1";
static int kbdfd = -1;

int init_kbd(const char *kbd_device)
{
    info_print("Initializing keyboard device %s ...\n", kbd_device);
    if ((kbdfd = open(kbd_device, O_RDWR)) == -1)
    {
        error_print("cannot open kbd device %s\n", kbd_device);
        return 0;
    }
    else
    {
        return 1;
    }
}

void cleanup_kbd()
{
    if (kbdfd != -1)
    {
        close(kbdfd);
    }
}

void injectKeyEvent(uint16_t code, uint16_t value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));

    gettimeofday(&ev.time, 0);
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = value;
    if (write(kbdfd, &ev, sizeof(ev)) < 0)
    {
        error_print("write event failed, %s\n", strerror(errno));
    }

    // Finally send the SYN
    gettimeofday(&ev.time, 0);
    ev.type = EV_SYN;
    ev.code = 0;
    ev.value = 0;
    if (write(kbdfd, &ev, sizeof(ev)) < 0)
    {
        error_print("write event failed, %s\n", strerror(errno));
    }

    debug_print("injectKey (%d, %d)\n", code, value);
}

int keysym2scancode(rfbKeySym key, rfbClientPtr cl)
{
    int scancode = 0;

    int code = (int)key;
    if (code >= '0' && code <= '9')
    {
        scancode = (code & 0xF) - 1;
        if (scancode < 0)
            scancode += 10;
        scancode += KEY_1;
    }
    else if (code >= 0xFF50 && code <= 0xFF58)
    {
        static const uint16_t map[] =
            {KEY_HOME, KEY_LEFT, KEY_UP, KEY_RIGHT, KEY_DOWN,
             KEY_END, 0};
        scancode = map[code & 0xF];
    }
    else if (code >= 0xFFE1 && code <= 0xFFEE)
    {
        static const uint16_t map[] =
            {KEY_LEFTSHIFT, KEY_LEFTSHIFT,
             KEY_COMPOSE, KEY_COMPOSE,
             KEY_LEFTSHIFT, KEY_LEFTSHIFT,
             0, 0,
             KEY_LEFTALT, KEY_RIGHTALT,
             0, 0, 0, 0};
        scancode = map[code & 0xF];
    }
    else if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z'))
    {
        static const uint16_t map[] = {
            KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,
            KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
            KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
            KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
            KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
        scancode = map[(code & 0x5F) - 'A'];
    }
    else
    {
        switch (code)
        {
        case 0x0020:
            scancode = KEY_SPACE;
            break;
        case 0x002C:
            scancode = KEY_COMMA;
            break;
        case 0x003C:
            scancode = KEY_COMMA;
            break;
        case 0x002E:
            scancode = KEY_DOT;
            break;
        case 0x003E:
            scancode = KEY_DOT;
            break;
        case 0x002F:
            scancode = KEY_SLASH;
            break;
        case 0x003F:
            scancode = KEY_SLASH;
            break;
        case 0x0032:
            scancode = KEY_EMAIL;
            break;
        case 0x0040:
            scancode = KEY_EMAIL;
            break;
        case 0xFF08:
            scancode = KEY_BACKSPACE;
            break;
        case 0xFF1B:
            scancode = KEY_BACK;
            break;
        case 0xFF09:
            scancode = KEY_TAB;
            break;
        case 0xFF0D:
            scancode = KEY_ENTER;
            break;
        case 0xFFBE:
            scancode = KEY_F1;
            break; // F1
        case 0xFFBF:
            scancode = KEY_F2;
            break; // F2
        case 0xFFC0:
            scancode = KEY_F3;
            break; // F3
        case 0xFFC5:
            scancode = KEY_F4;
            break; // F8
        case 0xFFC8:
            rfbShutdownServer(cl->screen, TRUE);
            break; // F11
        }
    }

    return scancode;
}
