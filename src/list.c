#include "list.h"
#include "heap.h"

void list_init(list_t *list) {
    list->head = NULL;
    list->size = 0;
}

int list_push(list_t *list, void *data) {
    list_node_t *node = heap_alloc(sizeof(list_node_t));
    if (!node) return 1;

    node->data = data;
    node->next = list->head;
    list->head = node;
    list->size++;

    return 0;
}

void *list_pop(list_t *list) {
    if (!list->head) return NULL;

    list_node_t *node = list->head;
    void *data = node->data;

    list->head = node->next;
    heap_free(node);
    list->size--;

    return data;
}

void *list_get(list_t *list, size_t index) {
    if (index >= list->size) return NULL;

    list_node_t *current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }

    return current->data;
}

int list_remove(list_t *list, size_t index) {
    if (index >= list->size) return 1;

    if (index == 0) {
        list_pop(list);
        return 0;
    }

    list_node_t *current = list->head;
    for (size_t i = 0; i < index - 1; i++) {
        current = current->next;
    }

    list_node_t *target = current->next;
    current->next = target->next;
    heap_free(target);
    list->size--;

    return 0;
}

void list_clear(list_t *list) {
    while(list->size > 0)
        list_pop(list);
}

void list_free(list_t *list) {
    heap_free(list);
}
