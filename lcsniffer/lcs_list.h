#ifndef __LCS_LIST_H__
#define __LCS_LIST_H__

#include <sys/time.h>
#include <stdlib.h>

typedef struct list_node_s {
    struct list_node_s *next;
    void *data;
    time_t timestamp;
}list_node_t;

typedef struct list_s {
    list_node_t *head;
    unsigned int count;
    int (*cmp) (void *val1, void * val2);
    void (*del) (void *val);
    time_t timestamp;
}list_t;

#define LCSNFD_LIST_NODE_MAX_TIME 600

static inline void listnode_add(list_t *list, void *val)
{
    list_node_t *node;

    node = (list_node_t *)malloc(sizeof(list_node_t));
    node->data = val;
    node->next = list->head;
    node->timestamp = time(NULL);
    list->head = node;
    list->count++;
}

static inline list_node_t *listnode_search(list_t *list, void *val)
{
    list_node_t *node;

    if (list->cmp) {
        for (node=list->head; node; node=node->next) {
            if (list->cmp(node->data, val) == 0) {
                return node;
            }
        }
    }

    return NULL;
}

static inline void list_aged(list_t *list)
{
    list_node_t **node, *tmp;
    time_t crr = time(NULL);

    if (crr - list->timestamp < 120) {
        return;
    }
    for (node=&list->head; *node;) {
        tmp = *node;
        if (crr - tmp->timestamp >= LCSNFD_LIST_NODE_MAX_TIME) {
            *node = tmp->next;
            if (list->del)  {
                list->del(tmp->data);
            }
            free(tmp);
            list->count--;
        } else {
            node = &tmp->next;
        }
    }
}

#endif /* __LCS_LIST_H__ */
