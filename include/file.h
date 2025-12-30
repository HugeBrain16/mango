#include <stdint.h>

#define FILE_DATA 0
#define FILE_FOLDER 1

#define FILE_MAX_SIZE 1024
#define FILE_MAX_NAME 512

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
int file_exists(file_node_t *parent, const char *name);
int file_create(file_node_t *parent, const char *name);
