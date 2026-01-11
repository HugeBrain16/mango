#include <stdint.h>
#include <stddef.h>

#define FILE_DATA 0
#define FILE_FOLDER 1

#define FILE_MAX_SIZE 1024
#define FILE_MAX_NAME 512
#define FILE_MAX_PATH 4096

typedef struct file_node {
    uint8_t type;

    struct file_node *parent;
    struct file_node *child_head;
    struct file_node *child_next;

    char *name;
    void *data;
} file_node_t;

file_node_t *file_root;
file_node_t *file_parent;

void file_init();
file_node_t *file_get(file_node_t *parent, const char *name);
file_node_t *file_get_node(const char *path);
int file_exists(file_node_t *parent, const char *name);
int file_create(file_node_t *parent, const char *name);
int file_delete(file_node_t *parent, const char *name);
file_node_t *folder_get(file_node_t *parent, const char *name);
int folder_exists(file_node_t *parent, const char *name);
int folder_create(file_node_t *parent, const char *name);
int folder_delete(file_node_t *parent, const char *name);
int file_split_path(const char *path, char *out_parent, char *out_name);
void file_get_abspath(file_node_t *parent, char *path, size_t size);
