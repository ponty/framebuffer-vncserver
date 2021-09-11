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

#ifndef input_event_sec
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
#endif

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

    struct timeval time;
    gettimeofday(&time, 0);
    ev.input_event_sec = time.tv_sec;
    ev.input_event_usec = time.tv_usec;

    ev.type = EV_KEY;
    ev.code = code;
    ev.value = value;
    if (write(kbdfd, &ev, sizeof(ev)) < 0)
    {
        error_print("write event failed, %s\n", strerror(errno));
    }

    // Finally send the SYN
    gettimeofday(&time, 0);
    ev.input_event_sec = time.tv_sec;
    ev.input_event_usec = time.tv_usec;

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
        case XK_space:          scancode = KEY_SPACE;   break;  //20
        case XK_exclam:         scancode = KEY_1;       break;  //21
        case XK_quotedbl:       scancode = KEY_APOSTROPHE; break; //22
        case XK_numbersign:     scancode = KEY_3;       break;  //23
        case XK_dollar:         scancode = KEY_4;       break;  //24
        case XK_percent:        scancode = KEY_5;       break;  //25
        case XK_ampersand:      scancode = KEY_7;       break;  //26
        case XK_apostrophe:     scancode = KEY_APOSTROPHE; break; //27
        case XK_parenleft:      scancode = KEY_9;       break;  //28
        case XK_parenright:     scancode = KEY_0;       break;  //29
        case XK_asterisk:       scancode = KEY_8;       break;  //2a
        case XK_plus:           scancode = KEY_MINUS;   break;  //2b
        case XK_comma:          scancode = KEY_EQUAL;   break;  //2c
        case XK_minus:          scancode = KEY_MINUS;   break;  //2d
        case XK_period:         scancode = KEY_DOT;     break;  //2e
        case XK_slash:          scancode = KEY_SLASH;   break;  //2f
        // XK_0 .. XK_9         // 30 .. 39
        case XK_colon:          scancode = KEY_SEMICOLON; break; //3a
        case XK_semicolon:      scancode = KEY_SEMICOLON; break; //3b
        case XK_less:           scancode = KEY_COMMA;   break;  //3c
        case XK_equal:          scancode = KEY_EQUAL;   break;  //3d
        case XK_greater:        scancode = KEY_DOT;     break;  //3e
        case XK_question:       scancode = KEY_SLASH;   break;  //3f
        case XK_at:             scancode = KEY_2;       break;  //40
        // XK_A .. XK_Z         // 41 .. 5a
        case XK_bracketleft:    scancode = KEY_LEFTBRACE; break; //5b
        case XK_backslash:      scancode = KEY_BACKSLASH; break; //5c
        case XK_bracketright:   scancode = KEY_RIGHTBRACE; break; //5d
        case XK_asciicircum:    scancode = KEY_6;       break;  //5e
        case XK_underscore:     scancode = KEY_MINUS;   break;  //5f
        case XK_grave:          scancode = KEY_GRAVE;   break;  //60
        // XK_a .. XK_z         // 61 .. 7a
        case XK_braceleft:      scancode = KEY_LEFTBRACE; break; //7b
        case XK_bar:            scancode = KEY_BACKSLASH; break; //7c
        case XK_braceright:     scancode = KEY_RIGHTBRACE; break; //7d
        case XK_asciitilde:     scancode = KEY_GRAVE;   break;  //7e

        case XK_BackSpace:      scancode = KEY_BACKSPACE;       break;  //ff08
        case XK_Escape:         scancode = KEY_ESC;     break;  //ff1b
        case XK_Tab:            scancode = KEY_TAB;     break;  //ff09
        case XK_Return:         scancode = KEY_ENTER;   break;  //ff0d
        case XK_F1:             scancode = KEY_F1;      break;  //ffbe
        case XK_F2:             scancode = KEY_F2;      break;  //ffbf
        case XK_F3:             scancode = KEY_F3;      break;  //ffc0
        case XK_F4:             scancode = KEY_F4;      break;  //ffc1
        case XK_F5:             scancode = KEY_F5;      break;  //ffc2
        case XK_F6:             scancode = KEY_F6;      break;  //ffc3
        case XK_F7:             scancode = KEY_F7;      break;  //ffc4
        case XK_F8:             scancode = KEY_F8;      break;  //ffc5
        case XK_F9:             scancode = KEY_F9;      break;  //ffc6
        case XK_F10:            scancode = KEY_F10;     break;  //ffc7
        //case XK_F11:          scancode = KEY_F11;     break;  //ffc8
        case XK_F12:            scancode = KEY_F12;     break;  //ffc9
        case XK_F11:
            rfbShutdownServer(cl->screen, TRUE);
            break;      //ffc8

        case XK_Home:           scancode = KEY_HOME;    break;  //ff50
        case XK_Left:           scancode = KEY_LEFT;    break;  //ff51
        case XK_Up:             scancode = KEY_UP;      break;  //ff52
        case XK_Right:          scancode = KEY_RIGHT;   break;  //ff53
        case XK_Down:           scancode = KEY_DOWN;    break;  //ff54
        case XK_Page_Up:        scancode = KEY_PAGEUP;  break;  //ff55
        case XK_Page_Down:      scancode = KEY_PAGEDOWN; break; //ff56
        case XK_End:            scancode = KEY_END;     break;  //ff57
        case XK_Begin:          scancode = KEY_HOME;    break;  //ff58

        case XK_Shift_L:        scancode = KEY_LEFTSHIFT; break; //ffe1
        case XK_Shift_R:        scancode = KEY_RIGHTSHIFT; break; //ffe2
        case XK_Control_L:      scancode = KEY_LEFTCTRL; break; //ffe3
        case XK_Control_R:      scancode = KEY_RIGHTCTRL; break; //ffe4
        case XK_Alt_L:          scancode = KEY_LEFTALT; break;  //ffe9
        case XK_Alt_R:          scancode = KEY_RIGHTALT; break; //ffea
        }
    }

    return scancode;
}
