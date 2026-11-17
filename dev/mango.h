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

static inline void screen_flush() {
    asm volatile("int $0x30" ::
        "a"(CALL_SCREEN_FLUSH)
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

static inline void screen_info(int *width, int *height, uint32_t *pitch, float *scale) {
    asm volatile("int $0x30" ::
        "a"(CALL_SCREEN_INFO),
        "b"(width),
        "c"(height),
        "d"(pitch),
        "S"(scale)
    : "memory");
}

#endif
