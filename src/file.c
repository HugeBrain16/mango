#include "file.h"
#include "string.h"
#include "heap.h"

void file_init() {
    file_root = heap_alloc(sizeof(file_node_t));
    file_root->type = FILE_FOLDER;
    file_root->parent = NULL;
    file_root->child_head = NULL;
    file_root->child_next = NULL;
    file_root->name = NULL;
    file_root->data = NULL;

    file_parent = file_root;
}

file_node_t *file_get(file_node_t *parent, const char *name) {
    if (!parent->child_head)
        return NULL;

    file_node_t *current = parent->child_head;
    while (current) {
        if (!strcmp(current->name, name) && current->type == FILE_DATA)
            return current;

        current = current->child_next;
    }

    return NULL;
}

int file_split_path(const char *path, char *out_parent, char *out_name) {
    size_t len = strlen(path);
    if (len == 0)
        return 0;

    size_t i = len - 1;

    while (i > 0 && path[i] == '/')
        i--;

    size_t name_len = 0;
    while (path[i] != '/') {
        if (name_len >= FILE_MAX_NAME - 1)
            return 0;
        out_name[name_len++] = path[i];
        if (i == 0) break;
        i--;
    }

    if (name_len == 0)
        return 0;

    out_name[name_len] = '\0';
    strflip(out_name, 0, name_len - 1);

    if (i == 0 && path[0] != '/') {
        out_parent[0] = '\0';
        return 1;
    }

    if (i == 0) {
        strcpy(out_parent, "/");
        return 1;
    }

    memcpy(out_parent, path, i);
    out_parent[i] = '\0';
    return 1;
}

void file_get_abspath(file_node_t *node, char *path, size_t size) {
    size_t pos;
    size_t out = 0;

    if (!node || !path || size < 2) {
        if (path && size > 0)
            path[0] = '\0';
        return;
    }

    pos = size - 1;
    path[pos] = '\0';

    while (node && node != file_root) {
        size_t len = strlen(node->name);

        if (pos < len + 1) {
            path[0] = '\0';
            return;
        }

        pos -= len;
        memcpy(path + pos, node->name, len);

        path[--pos] = '/';
        node = node->parent;
    }

    if (pos == size - 1) {
        path[0] = '/';
        path[1] = '\0';
        return;
    }

    while (path[pos] != '\0') {
        path[out++] = path[pos++];
    }
    path[out] = '\0';
}

file_node_t *file_get_node(const char *path) {
    size_t name_index = 0;
    char *name = heap_alloc(FILE_MAX_NAME);
    file_node_t *parent = NULL;

    for (size_t i = 0; i < strlen(path); i++) {
        char c = path[i];

        if (c == '/' && i == 0) {
            parent = file_root;
            continue;
        } else if (c == '/') {
            if (name_index < 1)
                continue;

            name[name_index] = '\0';

            file_node_t *current;
            if (!parent)
                current = file_parent->child_head;
            else
                current = parent->child_head;

            while (current) {
                if (!strcmp(current->name, name)) {
                    parent = current;
                    name_index = 0;
                    memset(name, 0, sizeof(name));
                    break;
                }
                current = current->child_next;
            }

            if (!current) {
                heap_free(name);
                return NULL;
            }
        } else
            name[name_index++] = c;
    }

    if (name_index > 0) {
        name[name_index] = '\0';

        file_node_t *current;
        if (!parent)
            current = file_parent->child_head;
        else
            current = parent->child_head;

        while (current) {
            if (!strcmp(current->name, name))
                return current;

            current = current->child_next;
        }
    }

    heap_free(name);
    return parent;
}

file_node_t *file_get_node2(const char *parent, const char *basename) {
    file_node_t *node = NULL;
    char *path = heap_alloc(FILE_MAX_PATH);
    strcat(path, parent);
    strcat(path, basename);

    node = file_get_node(path);
    heap_free(path);
    return node;
}

int file_exists(file_node_t *parent, const char *name) {
    return file_get(parent, name) != NULL;
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
    memset(file->data, 0, FILE_MAX_SIZE);
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

file_node_t *folder_get(file_node_t *parent, const char *name) {
    if (!parent->child_head)
        return NULL;

    file_node_t *current = parent->child_head;
    while (current) {
        if (!strcmp(current->name, name) && current->type == FILE_FOLDER)
            return current;

        current = current->child_next;
    }

    return NULL;
}

int folder_exists(file_node_t *parent, const char *name) {
    return folder_get(parent, name) != NULL;
}

int folder_create(file_node_t *parent, const char *name) {
    if (folder_exists(parent, name) || parent->type != FILE_FOLDER)
        return 0;

    file_node_t *folder = heap_alloc(sizeof(file_node_t));
    folder->parent = parent;
    folder->type = FILE_FOLDER;
    folder->child_head = NULL;
    folder->child_next = NULL;
    folder->data = NULL;
    folder->name = heap_alloc(FILE_MAX_NAME);
    strcpy(folder->name, name);

    if (parent->child_head) {
        file_node_t *current = parent->child_head;

        while(current->child_next) {
            current = current->child_next;
        }
        current->child_next = folder;
    } else {
        parent->child_head = folder;
    }

    return 1;
}

int folder_delete(file_node_t *parent, const char *name) {
    file_node_t *prev = NULL;
    file_node_t *current = parent->child_head;

    while (current) {
        if (!strcmp(current->name, name)) {
            if (current->type != FILE_FOLDER)
                return 0;

            if (current->child_head) {
                file_node_t *current_file = current->child_head;

                while (current_file) {
                    file_delete(current, current_file->name);
                    current_file = current_file->child_next;
                }
            }

            if (prev) {
                prev->child_next = current->child_next;
            } else {
                parent->child_head = current->child_next;
            }

            heap_free(current->name);
            heap_free(current);
            return 1;
        }

        prev = current;
        current = current->child_next;
    }

    return 0;
}
