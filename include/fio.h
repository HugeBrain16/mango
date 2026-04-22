#ifndef FIO_H
#define FIO_H

#include "file.h"

#define FIO_FS_BLOCKSIZE 508
#define FIO_EOF -1

#define FIO_READ 114 // 'r' - read
#define FIO_WRITE 119 // 'w' - write
#define FIO_APPEND 97 // 'a' - append

typedef struct {
    uint32_t file;
    uint32_t seek;
    uint8_t mode;
    uint32_t last_sector;
    uint32_t last_block;
} fio_t;

extern fio_t *fio_open(const char *path, uint8_t mode);
extern int fio_getc(fio_t *fio);
extern int fio_peek(fio_t *fio);
extern void fio_putc(fio_t *fio, char c);
extern void fio_read(fio_t *fio, char *dest, size_t length);
extern void fio_write(fio_t *fio, const char *str, size_t length);
extern void fio_close(fio_t *fio);

#endif
