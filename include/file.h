#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stddef.h>

#define FILE_MAGIC 0x4F474E4D
#define FILE_VERSION 2
#define FILE_SECTOR_SUPERBLOCK 0
#define FILE_SECTOR_ROOT 1

#define FILE_DATA (1 << 0)
#define FILE_FOLDER (1 << 1)

#define FILE_MAX_NAME 32
#define FILE_MAX_PATH 1024

#define FILE_DRIVE_UNSET -1
#define FILE_DRIVE_OK 0
#define FILE_DRIVE_ABSENT 1
#define FILE_DRIVE_INCOMPATIBLE 2

typedef struct __attribute__((packed)) file_pointer {
    uint8_t offset;
    uint32_t lba;
} file_pointer_t;

#define FILE_ROOT ((file_pointer_t){ .offset = 0, .lba = FILE_SECTOR_ROOT })
#define FILE_NULL(p) ((p).lba == 0)
#define FILE_EQ(a, b) ((a).lba == (b).lba && (a).offset == (b).offset)

typedef struct file_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t sectors;
    uint32_t used;
    uint32_t free;
    uint32_t free_list;
    file_pointer_t free_pointer;
    file_pointer_t free_nodes;
} file_superblock_t;

typedef struct __attribute__((packed)) file_node {
    uint64_t time_created;
    uint64_t time_changed;

    file_pointer_t parent;
    file_pointer_t child_head;
    file_pointer_t child_next;

    uint32_t size;
    uint32_t first_block;
    char name[FILE_MAX_NAME];
    uint8_t flags;
} file_node_t;

#define FILE_MAX_OFFSET (512 / sizeof(file_node_t))

typedef struct file_data {
    uint32_t next;
    uint8_t data[508];
} file_data_t;

typedef struct {
    char serial[21];
    char rev[9];
    char model[41];
} drive_t;

extern uint16_t file_port;
extern uint8_t file_drive;
extern file_pointer_t file_current;
extern int file_drive_status;

extern int file_init(uint16_t base, uint8_t drive);
extern int file_init_by_slot(uint8_t slot);
extern int file_drive_slot();
extern int file_drive_spec(drive_t *drive);

extern void file_format();
extern int file_is_formatted();
extern int file_is_ready();

extern void file_read_sb(file_superblock_t *sb);
extern void file_write_sb(file_superblock_t *sb);

extern void file_node(file_pointer_t *pointer, file_node_t *node);
extern void file_node_write(file_pointer_t *pointer, file_node_t *node);

extern void file_data(uint32_t sector, file_data_t *node);
extern void file_data_write(uint32_t sector, file_data_t *node);

extern file_pointer_t file_get_node(const char *path);
extern file_pointer_t file_get_node2(const char *parent, const char *basename);

extern file_pointer_t file_get(file_pointer_t *parent, const char *name);
extern int file_exists(file_pointer_t *parent, const char *name);
extern int file_create(file_pointer_t *parent, const char *name);
extern int file_delete(file_pointer_t *parent, const char *name);

extern file_pointer_t folder_get(file_pointer_t *parent, const char *name);
extern int folder_exists(file_pointer_t *parent, const char *name);
extern int folder_create(file_pointer_t *parent, const char *name);
extern int folder_delete(file_pointer_t *parent, const char *name);

extern int file_split_path(const char *path, char *out_parent, char *out_name);
extern void file_get_abspath(file_pointer_t *pointer, char *path, size_t size);

extern void file_sector_free_data(uint32_t sector);
extern void file_sector_free_node(file_pointer_t *pointer);
extern int file_sector_alloc_node(file_pointer_t *pointer);
extern uint32_t file_sector_alloc_data();

extern int file_write(file_pointer_t *pointer, const char *data, size_t size);
extern char *file_read(file_pointer_t *pointer);

extern int file_path_isfile(const char *path);
extern int file_path_isfolder(const char *path);

#endif
