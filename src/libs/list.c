#include "list.h"
#include "heap.h"

void list_init(list_t *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

int list_push(list_t *list, void *data) {
    list_node_t *node = heap_alloc(sizeof(list_node_t));
    if (!node) return 1;

    node->data = data;
    node->next = NULL;
    node->prev = list->tail;

    if (list->tail)
        list->tail->next = node;
    else
        list->head = node;

    list->tail = node;
    list->size++;
    return 0;
}

void *list_pop(list_t *list) {
    if (!list->head) return NULL;

    list_node_t *node = list->head;
    void *data = node->data;

    list->head = node->next;
    if (list->head)
        list->head->prev = NULL;
    else
        list->tail = NULL;

    heap_free(node);
    list->size--;

    return data;
}

void *list_get(list_t *list, size_t index) {
    if (index >= list->size) return NULL;

    list_node_t *current;
    size_t size = list->size;

    if (index <= size / 2) {
        current = list->head;
        for (size_t i = 0; i < index; i++)
            current = current->next;
    } else {
        current = list->tail;
        for (size_t i = size - 1; i > index; i--)
            current = current->prev;
    }

    return current->data;
}

int list_remove(list_t *list, size_t index) {
    if (index >= list->size) return 1;

    list_node_t *target;
    size_t size = list->size;

    if (index <= size / 2) {
        target = list->head;
        for (size_t i = 0; i < index; i++)
            target = target->next;
    } else {
        target = list->tail;
        for (size_t i = size - 1; i > index; i--)
            target = target->prev;
    }

    if (target->prev)
        target->prev->next = target->next;
    else
        list->head = target->next;

    if (target->next)
        target->next->prev = target->prev;
    else
        list->tail = target->prev;

    heap_free(target);
    list->size--;

    return 0;
}

void list_clear(list_t *list) {
    list_node_t *current = list->head;
    while (current) {
        list_node_t *next = current->next;
        heap_free(current);
        current = next;
    }

    // for reset
    list_init(list);
}

void list_free(list_t *list) {
    if (!list) return;
    heap_free(list);
}
