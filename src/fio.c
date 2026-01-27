#include "fio.h"
#include "heap.h"
#include "string.h"

static uint32_t fio_get_block(fio_t *fio) {
    file_node_t file;
    file_node(fio->file, &file);

    uint32_t sector = 1;

    while (fio->seek >= FIO_FS_BLOCKSIZE * sector)
        sector++;

    return file.first_block + (sector - 1);
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

    if (mode == FIO_APPEND) {
        uint32_t block_sector = file.first_block + (file.size - 1);
        file_data_t block;
        file_data(block_sector, &block);

        uint32_t length = 0;
        while(length < FIO_FS_BLOCKSIZE && block.data[length] != '\0')
            length++;

        if (length == FIO_FS_BLOCKSIZE)
            fio->seek = file.size * FIO_FS_BLOCKSIZE;
        else
            fio->seek = (file.size - 1) * FIO_FS_BLOCKSIZE + length;
    } else {
        fio->seek = 0;
    }

    return fio;
}

char fio_getc(fio_t *fio) {
    if (fio->mode != FIO_READ)
        return '\0';

    uint32_t block_sector = fio_get_block(fio);
    file_data_t block;
    file_data(block_sector, &block);

    uint32_t char_at = fio->seek % FIO_FS_BLOCKSIZE;
    char c = block.data[char_at];

    if (c != '\0')
        fio->seek++;

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
        char ca = block.data[char_at];

        if (ca == '\0') {
            if (char_at == FIO_FS_BLOCKSIZE - 1) {
                uint32_t new_block = file_sector_alloc();
                block.next = new_block;

                file.size++;
                file_node_write(fio->file, &file);

                file_data_t data = {0};
                data.next = 0;
                data.data[0] = '\0';
                file_data_write(new_block, &data);
            } else
                block.data[char_at + 1] = '\0';
        }
        block.data[char_at] = c;
        fio->seek++;

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

    while (i < length - 1) {
        c = fio_getc(fio);
        if (c == '\0')
            break;

        dest[i++] = c;
    }
    dest[i] = '\0';
}

void fio_close(fio_t *fio) {
    if (!fio) return;
    heap_free(fio);
}
