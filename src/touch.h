#pragma once

enum MouseAction
{
    MouseDrag = -1,
    MouseRelease,
    MousePress
};

int init_touch(const char *touch_device, int vnc_rotate);
void cleanup_touch();
void injectTouchEvent(enum MouseAction mouseAction, int x, int y, struct fb_var_screeninfo *scrinfo);
