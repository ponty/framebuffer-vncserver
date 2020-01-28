#pragma once

int init_kbd(const char *);
void cleanup_kbd();

void injectKeyEvent(uint16_t, uint16_t);
int keysym2scancode(rfbKeySym key, rfbClientPtr cl);
