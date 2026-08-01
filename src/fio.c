#include "fio.h"
#include "heap.h"
#include "string.h"

static uint32_t fio_get_block(fio_t *fio) {
    uint32_t target_sector = fio->seek / FIO_FS_BLOCKSIZE;

    uint32_t sector;
    uint32_t current;
    if (fio->last_block != 0 && fio->last_sector <= target_sector) {
        sector = fio->last_sector;
        current = fio->last_block;
    } else {
        sector = 0;
        current = fio->node->first_block;
    }

    while (sector < target_sector) {
        file_data(current, fio->block);
        if (fio->block->next == 0)
            break;

        current = fio->block->next;
        sector++;
    }
    if (fio->block_cache != current) {
        file_data(current, fio->block);
        fio->block_cache = current;
    }

    fio->last_sector = sector;
    fio->last_block = current;

    return current;
}

fio_t *fio_open(const char *path, uint8_t mode) {
    if (!path)
        return NULL;

    uint32_t node = file_get_node(path);
    if (!node)
        return NULL;

    file_node_t *file = heap_alloc(sizeof(file_node_t));
    file_node(node, file);

    if (file->flags & FILE_FOLDER) {
        heap_free(file);
        return NULL;
    }

    fio_t *fio = heap_alloc(sizeof(fio_t));
    fio->file = node;
    fio->mode = mode;
    fio->last_sector = 0;
    fio->last_block = 0;
    fio->block_cache = 0;
    fio->node = file;
    fio->block = heap_alloc(sizeof(file_data_t));

    if (mode == FIO_APPEND)
        fio->seek = file->size;
    else
        fio->seek = 0;

    return fio;
}

int fio_getc(fio_t *fio) {
    if (fio->seek >= fio->node->size)
        return FIO_EOF;

    fio_get_block(fio); // update block

    uint32_t char_at = fio->seek % FIO_FS_BLOCKSIZE;
    char c = fio->block->data[char_at];

    fio->seek++;
    return c;
}

int fio_peek(fio_t *fio) {
    int c = fio_getc(fio);

    if (c != FIO_EOF)
        fio->seek--;

    return c;
}

int fio_eof(fio_t *fio) {
    return fio->seek >= fio->node->size;
}

static int fio_putc2(fio_t *fio, char c, int update) {
    if (fio->mode == FIO_WRITE || fio->mode == FIO_APPEND) {
        uint32_t block_sector = fio_get_block(fio);

        uint32_t char_at = fio->seek % FIO_FS_BLOCKSIZE;
        if (char_at == FIO_FS_BLOCKSIZE - 1) {
            uint32_t new_block = file_sector_alloc();
            fio->block->next = new_block;

            file_data_t data = {0};
            data.next = 0;
            data.data[0] = '\0';
            file_data_write(new_block, &data);
        }
        fio->block->data[char_at] = c;
        fio->seek++;

        if (fio->seek > fio->node->size)
            fio->node->size = fio->seek;
        if (update)
            file_node_write(fio->file, fio->node);

        file_data_write(block_sector, fio->block);
        return 1;
    } else
        return 0;
}

int fio_putc(fio_t *fio, char c) {
    return fio_putc2(fio, c, 1);
}

int fio_write(fio_t *fio, const char *str, size_t length) {
    if (fio->mode == FIO_WRITE || fio->mode == FIO_APPEND) {
        for (size_t i = 0; i < length; i++) {
            if (!fio_putc2(fio, str[i], 0)) {
                file_node_write(fio->file, fio->node);
                return 0;
            }
        }
        file_node_write(fio->file, fio->node);
    }

    return 1;
}

int fio_read(fio_t *fio, char *dest, size_t length) {
    if (length == 0 || fio->mode != FIO_READ)
        return 0;

    char c;
    size_t i = 0;

    if (fio->seek >= fio->node->size)
        return 0;

    size_t seek = fio->seek;
    while (i < length - 1 && i < fio->node->size - seek) {
        c = fio_getc(fio);
        dest[i++] = c;
    }

    return 1;
}

int fio_close(fio_t *fio) {
    if (!fio) return 0;

    heap_free(fio->node);
    heap_free(fio->block);
    heap_free(fio);
    return 1;
}
