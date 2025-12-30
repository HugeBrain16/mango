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
        return 0;

    file_node_t *file = heap_alloc(sizeof(file_node_t));
    file->parent = parent;
    file->type = FILE_DATA;
    file->child_head = NULL;
    file->child_next = NULL;
    file->data = heap_alloc(FILE_MAX_SIZE);
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

    return 1;
}

int file_delete(file_node_t *parent, const char *name) {
    file_node_t *prev = NULL;
    file_node_t *current = parent->child_head;

    while (current) {
        if (!strcmp(current->name, name)) {
            if (current->type != FILE_DATA)
                return 0;

            heap_free(current->data);
            heap_free(current->name);

            if (prev) {
                prev->child_next = current->child_next;
            } else {
                parent->child_head = current->child_next;
            }

            heap_free(current);
            return 1;
        }

        prev = current;
        current = current->child_next;
    }

    return 0;
}
