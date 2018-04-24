#pragma once

void init_touch();
void cleanup_touch();
void injectTouchEvent(int down, int x, int y, struct fb_var_screeninfo* scrinfo);

