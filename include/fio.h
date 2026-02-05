#ifndef FIO_H
#define FIO_H

#include "file.h"

#define FIO_FS_BLOCKSIZE 508

#define FIO_READ 114 // 'r' - read
#define FIO_WRITE 119 // 'w' - write
#define FIO_APPEND 97 // 'a' - append

typedef struct {
    uint32_t file;
    uint32_t seek;
    uint8_t mode;
} fio_t;

fio_t *fio_open(const char *path, uint8_t mode);
char fio_getc(fio_t *fio);
char fio_peek(fio_t *fio);
void fio_putc(fio_t *fio, char c);
void fio_read(fio_t *fio, char *dest, size_t length);
void fio_write(fio_t *fio, const char *str, size_t length);
void fio_close(fio_t *fio);

#endif
