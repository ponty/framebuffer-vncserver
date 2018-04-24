#pragma once

void init_kbd();
void cleanup_kbd();

int keysym2scancode(rfbBool down, rfbKeySym key, rfbClientPtr cl);
