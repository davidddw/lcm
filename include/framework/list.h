#ifndef _FRAMEWORK_LIST_H
#define _FRAMEWORK_LIST_H

/*
 * This is one copy from linux kernel source with less modification.
 */
#include "framework/utils.h"

struct list_head {
    struct list_head *next, *prev;
};

struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next, **pprev;
};


static inline void list_init(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *list,
        struct list_head *prev, struct list_head *next)
{
    next->prev = list;
    list->next = next;
    list->prev = prev;
    prev->next = list;
}
#if 0
static inline void list_add(struct list_head *list, struct list_head *head)
{
    __list_add(list, head, head->next);
}
#endif
static inline void list_add_tail(struct list_head *list,
        struct list_head *head)
{
    __list_add(list, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *list)
{
    __list_del(list->prev, list->next);
    list->next = NULL;
    list->prev = NULL;
}

static inline void list_del_init(struct list_head *list)
{
    __list_del(list->prev, list->next);
    list_init(list);
}
#if 0
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}
#endif
static inline void list_move_tail(struct list_head *list,
        struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

static inline void list_replace(struct list_head *ancient,
        struct list_head *fresh)
{
    fresh->next = ancient->next;
    fresh->next->prev = fresh;
    fresh->prev = ancient->prev;
    fresh->prev->next = fresh;
}

static inline void list_replace_init(struct list_head *ancient,
        struct list_head *fresh)
{
    list_replace(ancient, fresh);
    list_init(ancient);
}

static inline bool list_is_last(const struct list_head *list,
        const struct list_head *head)
{
    return list->next == head;
}

static inline bool list_is_empty(const struct list_head *head)
{
    return head->next == head;
}

static inline bool list_is_empty_careful(const struct list_head *head)
{
    struct list_head *next = head->next;
    return (next == head) && (next == head->prev);
}

static inline bool list_is_singular(const struct list_head *head)
{
    return !list_is_empty(head) && (head->next == head->prev);
}

static inline void list_rotate_left(struct list_head *head)
{
    struct list_head *first;

    if (!list_is_empty(head)) {
        first = head->next;
        list_move_tail(first, head);
    }
}

static inline void __list_cut_position(struct list_head *list,
        struct list_head *head, struct list_head *entry)
{
    struct list_head *new_first = entry->next;
    list->next = head->next;
    list->next->prev = list;
    list->prev = entry;
    entry->next = list;
    head->next = new_first;
    new_first->prev = head;
}

static inline void list_cut_position(struct list_head *list,
        struct list_head *head, struct list_head *entry)
{
    if (list_is_empty(head))
        return;
    if (list_is_singular(head) &&
            (head->next != entry && head != entry))
        return;
    if (entry == head)
        list_init(list);
    else
        __list_cut_position(list, head, entry);
}

static inline void __list_splice(const struct list_head *list,
        struct list_head *prev, struct list_head *next)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

static inline void list_splice(const struct list_head *list,
        struct list_head *head)
{
    if (!list_is_empty(list))
        __list_splice(list, head, head->next);
}

static inline void list_splice_tail(struct list_head *list,
        struct list_head *head)
{
    if (!list_is_empty(list))
        __list_splice(list, head->prev, head);
}

static inline void list_splice_init(struct list_head *list,
        struct list_head *head)
{
    if (!list_is_empty(list)) {
        __list_splice(list, head, head->next);
        list_init(list);
    }
}

static inline void list_splice_tail_init(struct list_head *list,
        struct list_head *head)
{
    if (!list_is_empty(list)) {
        __list_splice(list, head->prev, head);
        list_init(list);
    }
}

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
            pos = n, n = pos->next)

#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev; \
            pos != (head); \
            pos = n, n = pos->prev)

#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
            &pos->member != (head); \
            pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_reverse(pos, head, member) \
    for (pos = list_entry((head)->prev, typeof(*pos), member); \
            &pos->member != (head); \
            pos = list_entry(pos->member.prev, typeof(*pos), member))

#define list_prepare_entry(pos, head, member) \
    ((pos) ? : list_entry(head, typeof(*pos), member))

#define list_for_each_entry_continue(pos, head, member) \
    for (pos = list_entry(pos->member.next, typeof(*pos), member); \
            &pos->member != (head); \
            pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_continue_reverse(pos, head, member) \
    for (pos = list_entry(pos->member.prev, typeof(*pos), member); \
            &pos->member != (head); \
            pos = list_entry(pos->member.prev, typeof(*pos), member))

#define list_for_each_entry_from(pos, head, member) \
    for (; &pos->member != (head); \
            pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member), \
            n = list_entry(pos->member.next, typeof(*pos), member); \
            &pos->member != (head); \
            pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_for_each_entry_safe_continue(pos, n, head, member) \
    for (pos = list_entry(pos->member.next, typeof(*pos), member), \
            n = list_entry(pos->member.next, typeof(*pos), member); \
            &pos->member != (head); \
            pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_for_each_entry_safe_from(pos, n, head, member) \
    for (n = list_entry(pos->member.next, typeof(*pos), member); \
            &pos->member != (head); \
            pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_for_each_entry_safe_reverse(pos, n, head, member) \
    for (pos = list_entry((head)->prev, typeof(*pos), member), \
            n = list_entry(pos->member.prev, typeof(*pos), member); \
            &pos->member != (head); \
            pos = n, n = list_entry(n->member.prev, typeof(*n), member))

#define list_safe_reset_next(pos, n, member) \
    n = list_entry(pos->member.next, typeof(*pos), member)


static inline void hlist_head_init(struct hlist_head *head)
{
    head->first = NULL;
}

static inline void hlist_node_init(struct hlist_node *node)
{
    node->next = NULL;
    node->pprev = NULL;
}

static inline bool hlist_is_unhashed(const struct hlist_node *node)
{
    return !node->pprev;
}

static inline bool hlist_is_empty(const struct hlist_head *head)
{
    return !head->first;
}

static inline void hlist_add_head(struct hlist_node *node,
        struct hlist_head *head)
{
    struct hlist_node *first = head->first;
    node->next = first;
    if (first)
        first->pprev = &node->next;
    head->first = node;
    node->pprev = &head->first;
}

static inline void hlist_add_before(struct hlist_node *node,
        struct hlist_node *next)
{
    node->pprev = next->pprev;
    node->next = next;
    next->pprev = &node->next;
    *(node->pprev) = node;
}

static inline void hlist_add_after(struct hlist_node *node,
        struct hlist_node *next)
{
    next->next = node->next;
    node->next = next;
    next->pprev = &node->next;

    if(next->next)
        next->next->pprev  = &next->next;
}

static inline void hlist_add_fake(struct hlist_node *node)
{
    node->pprev = &node->next;
}

static inline void __hlist_del(struct hlist_node *node)
{
    struct hlist_node *next = node->next;
    struct hlist_node **pprev = node->pprev;
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *node)
{
    __hlist_del(node);
    node->next = NULL;
    node->pprev = NULL;
}

static inline void hlist_del_init(struct hlist_node *node)
{
    if (!hlist_is_unhashed(node)) {
        __hlist_del(node);
        hlist_node_init(node);
    }
}

static inline void hlist_move_list(struct hlist_head *ancient,
        struct hlist_head *fresh)
{
    fresh->first = ancient->first;
    if (fresh->first)
        fresh->first->pprev = &fresh->first;
    ancient->first = NULL;
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos ; pos = pos->next)

#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
            pos = n)

#define hlist_for_each_entry(tpos, pos, head, member) \
    for (pos = (head)->first; \
            pos && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
            pos = pos->next)

#define hlist_for_each_entry_continue(tpos, pos, member) \
    for (pos = (pos)->next; \
            pos && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
            pos = pos->next)

#define hlist_for_each_entry_from(tpos, pos, member) \
    for (; pos && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
            pos = pos->next)

#define hlist_for_each_entry_safe(tpos, pos, n, head, member) \
    for (pos = (head)->first; \
            pos && ({ n = pos->next; 1; }) && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
            pos = n)

#endif /* _FRAMEWORK_LIST_H */
