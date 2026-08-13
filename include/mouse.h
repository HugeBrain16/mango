#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#include "media.h"

#define MOUSE_ACK 0xFA
#define MOUSE_ADDRESS_ME 0xD4
#define MOUSE_SET_RATE 0xF3
#define MOUSE_ENABLE_REPORTING 0xF4

extern int mouse_rate;
extern int mouse_x;
extern int mouse_y;
extern int mouse_middle;
extern int mouse_right;
extern int mouse_left;
extern uint32_t mouse_color;
extern image_t *mouse_cursor;

extern void mouse_init();
extern void mouse_handle();
extern void mouse_draw();
extern int mouse_load_cursor();

#endif