#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_WHITE 0xFFFFFF
#define COLOR_BLACK 0x000000
#define COLOR_PINK 0xFF8DA1
#define COLOR_AQUA 0x00FFF0
#define COLOR_ORANGE 0xFFA500
#define COLOR_PURPLE 0x9D00FF
#define COLOR_DARKGRAY 0x222222
#define COLOR_LIGHTGRAY 0x666666
#define COLOR_TRANSPARENT 0xFFFFFFFF
#define COLOR_INVALID -1

#define COLOR_R(c) (((c) >> 24) & 0xFF)
#define COLOR_G(c) (((c) >> 16) & 0xFF)
#define COLOR_B(c) (((c) >> 8)  & 0xFF)
#define COLOR_A(c) ((c) & 0xFF)

#define COLOR_RGB(r, g, b) \
    (((uint32_t)(r) << 16) | \
    ((uint32_t)(g) << 8) | \
    ((uint32_t)(b)))
#define COLOR_RGBA(r, g, b, a) \
	(((uint32_t)(r) << 24) | \
    ((uint32_t)(g) << 16) | \
    ((uint32_t)(b) << 8)  | \
    ((uint32_t)(a)))

extern int color(const char *name);

#endif
