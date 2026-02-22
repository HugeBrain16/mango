#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stddef.h>

#define FILE_MAGIC 0x4F474E4D
#define FILE_VERSION 1
#define FILE_SECTOR_SUPERBLOCK 2048
#define FILE_SECTOR_ROOT 2049

#define FILE_DATA (1 << 0)
#define FILE_FOLDER (1 << 1)

#define FILE_MAX_NAME 32
#define FILE_MAX_PATH 1024

typedef struct file_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t sectors;
    uint32_t used;
    uint32_t free;
    uint32_t free_list;
} file_superblock_t;

typedef struct file_node {
    uint64_t time_created;
    uint64_t time_changed;

    uint32_t parent;
    uint32_t child_head;
    uint32_t child_next;
    
    uint32_t size;
    uint32_t first_block;
    char name[FILE_MAX_NAME];
    uint8_t flags;
} file_node_t;

typedef struct file_data {
    uint32_t next;
    uint8_t data[508];
} file_data_t;

uint16_t file_port;
uint8_t file_drive;
uint32_t file_current;

void file_init(uint16_t base, uint8_t drive);
void file_format();
int file_is_formatted();
void file_read_sb(file_superblock_t *sb);
void file_write_sb(file_superblock_t *sb);
void file_node(uint32_t sector, file_node_t *node);
void file_node_write(uint32_t sector, file_node_t *node);
void file_data(uint32_t sector, file_data_t *node);
void file_data_write(uint32_t sector, file_data_t *node);
uint32_t file_get(uint32_t parent, const char *name);
uint32_t file_get_node(const char *path);
uint32_t file_get_node2(const char *parent, const char *basename);
int file_exists(uint32_t parent, const char *name);
int file_create(uint32_t parent, const char *name);
int file_delete(uint32_t parent, const char *name);
uint32_t folder_get(uint32_t parent, const char *name);
int folder_exists(uint32_t parent, const char *name);
int folder_create(uint32_t parent, const char *name);
int folder_delete(uint32_t parent, const char *name);
int file_split_path(const char *path, char *out_parent, char *out_name);
void file_get_abspath(uint32_t parent, char *path, size_t size);
void file_sector_free(uint32_t sector);
uint32_t file_sector_alloc();
int file_write(uint32_t sector, const char *data, size_t size);
char *file_read(uint32_t sector);

#endif
