#include "fio.h"
#include "heap.h"
#include "string.h"

static uint32_t fio_get_block(fio_t *fio) {
    file_node_t file;
    file_node(fio->file, &file);

    uint32_t sector = 1;
    uint32_t current = file.first_block;
    while (fio->seek >= FIO_FS_BLOCKSIZE * sector) {
        file_data_t block;
        file_data(current, &block);
        if (block.next == 0)
            break;

        current = block.next;
        sector++;
    }

    return current;
}

fio_t *fio_open(const char *path, uint8_t mode) {
    if (!path)
        return NULL;

    uint32_t node = file_get_node(path);
    if (!node)
        return NULL;

    file_node_t file;
    file_node(node, &file);

    if (file.flags & FILE_FOLDER)
        return NULL;

    fio_t *fio = heap_alloc(sizeof(fio_t));
    fio->file = node;
    fio->mode = mode;

    if (mode == FIO_APPEND)
        fio->seek = file.size;
    else
        fio->seek = 0;

    return fio;
}

int fio_getc(fio_t *fio) {
    file_node_t file;
    file_node(fio->file, &file);

    if (fio->seek >= file.size)
        return FIO_EOF;

    uint32_t block_sector = fio_get_block(fio);
    file_data_t block;
    file_data(block_sector, &block);

    uint32_t char_at = fio->seek % FIO_FS_BLOCKSIZE;
    char c = block.data[char_at];

    fio->seek++;
    return c;
}

int fio_peek(fio_t *fio) {
    int c = fio_getc(fio);

    if (c != FIO_EOF)
        fio->seek--;

    return c;
}

void fio_putc(fio_t *fio, char c) {
    if (fio->mode == FIO_WRITE || fio->mode == FIO_APPEND) {
        file_node_t file;
        file_node(fio->file, &file);

        uint32_t block_sector = fio_get_block(fio);
        file_data_t block;
        file_data(block_sector, &block);

        uint32_t char_at = fio->seek % FIO_FS_BLOCKSIZE;
        if (char_at == FIO_FS_BLOCKSIZE - 1) {
            uint32_t new_block = file_sector_alloc();
            block.next = new_block;

            file_data_t data = {0};
            data.next = 0;
            data.data[0] = '\0';
            file_data_write(new_block, &data);
        }
        block.data[char_at] = c;
        fio->seek++;

        if (fio->seek > file.size)
            file.size = fio->seek;
        file_node_write(fio->file, &file);

        file_data_write(block_sector, &block);
    }
}

void fio_write(fio_t *fio, const char *str, size_t length) {
    if (fio->mode == FIO_WRITE || fio->mode == FIO_APPEND) {
        for (size_t i = 0; i < length; i++)
            fio_putc(fio, str[i]);
    }
}

void fio_read(fio_t *fio, char *dest, size_t length) {
    if (length == 0 || fio->mode != FIO_READ)
        return;

    char c;
    size_t i = 0;

    file_node_t file;
    file_node(fio->file, &file);

    if (fio->seek >= file.size)
        return;

    while (i < length - 1 && i < file.size - fio->seek) {
        c = fio_getc(fio);
        dest[i++] = c;
    }
}

void fio_close(fio_t *fio) {
    if (!fio) return;
    heap_free(fio);
}
