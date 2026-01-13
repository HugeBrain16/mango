#ifndef LIST_H
#define LIST_H

#include <stddef.h>

typedef struct list_node {
    void *data;
    struct list_node *next;
} list_node_t;

typedef struct list {
    list_node_t *head;
    size_t size;
} list_t;

void list_init(list_t *list);
int list_push(list_t *list, void *data);
void *list_pop(list_t *list);
void *list_get(list_t *list, size_t index);
int list_remove(list_t *list, size_t index);
void list_clear(list_t *list);
void list_free(list_t *list);

#endif
