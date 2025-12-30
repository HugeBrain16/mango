#include "file.h"
#include "string.h"
#include "heap.h"

file_node_t *file_root = NULL;
file_node_t *file_parent = NULL;

void file_init() {
    file_root = heap_alloc(sizeof(file_node_t));
    file_root->type = FILE_FOLDER;
    file_root->child_head = NULL;
    file_root->child_next = NULL;
    file_root->data = NULL;

    file_parent = file_root;
}

int file_exists(file_node_t *parent, const char *name) {
    if (!parent->child_head)
        return 0;

    file_node_t *current = parent->child_head;
    while (current) {
        if (!strcmp(current->name, name))
            return 1;

        current = current->child_next;
    }

    return 0;
}

int file_create(file_node_t *parent, const char *name) {
    if (file_exists(parent, name) || parent->type != FILE_FOLDER)
        return 1;

    file_node_t *file = heap_alloc(sizeof(file_node_t));
    file->parent = parent;
    file->type = FILE_DATA;
    file->child_head = NULL;
    file->child_next = NULL;
    file->name = heap_alloc(FILE_MAX_NAME);
    strcpy(file->name, name);

    if (parent->child_head) {
        file_node_t *current = parent->child_head;

        while(current->child_next) {
            current = current->child_next;
        }
        current->child_next = file;
    } else {
        parent->child_head = file;
    }

    return 0;
}
