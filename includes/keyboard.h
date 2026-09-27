#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "stdint.h"

#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_CTRL_PORT    0x64 /* 8042 command register (write) */

#define MAX_SCANCODE 128

// Arrow key scancodes
enum {
  SC_UP    = 0x48,
  SC_DOWN  = 0x50,
  SC_LEFT  = 0x4B,
  SC_RIGHT = 0x4D,
  SC_ALT   = 0x38,
  SC_ALT_RELEASE = 0xB8,
  SC_LSHIFT = 0x2A,
  SC_LSHIFT_RELEASE = 0xAA,
  SC_RSHIFT = 0x36,
  SC_RSHIFT_RELEASE = 0xB6,
  SC_F1 = 0x3B, 
  SC_F3 = 0x3D,
  SC_F9 = 0x43
};

void poll_keyboard(void);
void handle_scancode(uint8_t scancode);

#endif // KEYBOARD_H
