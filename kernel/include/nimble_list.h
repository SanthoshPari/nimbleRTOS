#ifndef NIMBLE_LIST_H
#define NIMBLE_LIST_H

#include "nimble_types.h"

/*
 * Minimal intrusive circular doubly-linked list.
 *
 * "Intrusive" means the link node lives *inside* the object being
 * linked (see nimble_tcb_t.link in nimble_task.h) rather than the list
 * owning separately-allocated nodes. This is the standard kernel-space
 * technique (Linux list.h uses the same idea) — it means list
 * insertion/removal never allocates, which matters when you're
 * running inside an ISR or with interrupts disabled.
 */

typedef struct nimble_list_node {
    struct nimble_list_node *next;
    struct nimble_list_node *prev;
} nimble_list_node_t;

/* A list head is just a node that points to itself when empty. */
typedef nimble_list_node_t nimble_list_t;

static inline void nimble_list_init(nimble_list_t *list)
{
    list->next = list;
    list->prev = list;
}

static inline bool nimble_list_is_empty(const nimble_list_t *list)
{
    return list->next == list;
}

/* Insert `node` immediately before `head` (i.e. at the tail of the list). */
static inline void nimble_list_insert_tail(nimble_list_t *head, nimble_list_node_t *node)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

/* Insert `node` immediately after `head` (i.e. at the front of the list). */
static inline void nimble_list_insert_head(nimble_list_t *head, nimble_list_node_t *node)
{
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

static inline void nimble_list_remove(nimble_list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    /* Defensive: make the removed node safe to re-insert or inspect. */
    node->next = node;
    node->prev = node;
}

#define NIMBLE_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* NIMBLE_LIST_H */
