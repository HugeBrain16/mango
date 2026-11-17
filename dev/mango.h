#ifndef MANGO_H
#define MANGO_H

#include <stdint.h>
#include <stddef.h>
#include <_syscall.h>

static inline void serial_write(const char *msg) {
    asm volatile("int $0x30" ::
        "a"(CALL_SERIAL_WRITE),
        "b"(msg)
    : "memory");
}

static inline int file_write(uint32_t file, const char *data, size_t size) {
    uint32_t r;

    asm volatile("int $0x30" : "=a"(r) :
        "a"(CALL_FILE_WRITE),
        "b"(file),
        "c"(data), 
        "d"(size)
    : "memory");

    return r;
}

static inline int file_read(uint32_t file, char *data, size_t size) {
    uint32_t r;

    asm volatile("int $0x30" : "=a"(r) :
        "a"(CALL_FILE_READ),
        "b"(file),
        "c"(data), 
        "d"(size)
    : "memory");

    return r;
}

static inline double sys_perf() {
    double r;

    asm volatile("int $0x30" : "=t"(r) :
        "a"(CALL_SYS_PERF)
    : "memory");

    return r;
}

static inline uint8_t key_down() {
    uint8_t r;

    asm volatile("int $0x30" : "=a"(r) :
        "a"(CALL_KEY_DOWN)
    : "memory");

    return r;
}

static inline void screen_flush() {
    asm volatile("int $0x30" ::
        "a"(CALL_SCREEN_FLUSH)
    : "memory");
}

static inline void screen_clear(uint32_t color) {
    asm volatile("int $0x30" ::
        "a"(CALL_SCREEN_CLEAR),
        "b"(color)
    : "memory");
}

static inline void screen_draw(int x, int y, uint32_t color, int direct) {
    asm volatile("int $0x30" ::
        "a"(CALL_SCREEN_DRAW),
        "b"(x),
        "c"(y), 
        "d"(color),
        "S"(direct)
    : "memory");
}

static inline void screen_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, float scale) {
    asm volatile("flds %0" :: "m"(scale));
    asm volatile("int $0x30" ::
        "a"(CALL_SCREEN_DRAW_CHAR),
        "b"(x),
        "c"(y),
        "d"(c),
        "S"(fg_color),
        "D"(bg_color)
    : "memory");
}

static inline void screen_info(int *width, int *height, uint32_t *pitch, float *scale) {
    float _scale;
    asm volatile("int $0x30" : "=t"(_scale) :
        "a"(CALL_SCREEN_INFO),
        "b"(width),
        "c"(height),
        "d"(pitch)
    : "memory");

    if (scale)
        *scale = _scale;
}

#endif
