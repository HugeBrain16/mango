#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>

#include "media.h"

#define DESKTOP_WINDOW_ACTIVE (1 << 0)

typedef struct {
	int x; int y;
	int w; int h;
	int flags;
	char *name;
} desktop_window_t;

typedef struct {
	int x; int y;
	image_t *image;
	char *name;
} desktop_icon_t;

extern int desktop_active;
extern void desktop_init();
extern void desktop_handle_type(uint8_t scancode);
extern void desktop_update();

#endif