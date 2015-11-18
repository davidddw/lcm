#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <syslog.h>

#include "lc_allocator.h"

typedef struct lc_head head;
typedef struct lc_node node;
typedef int (*cmprsn_func)(void *a, void *b);

struct lc_rack {
    uint32_t        id;
    struct lc_head  vm_host;
    struct lc_head  vgw_host;
    struct lc_head  shared_storage;
    uint32_t        vm_cpu_total;
    uint32_t        vm_cpu_used;
    uint32_t        vm_mem_total;
    uint32_t        vm_mem_used;
    uint32_t        vm_disk_total;
    uint32_t        vm_disk_used;
    uint32_t        max_vgw_cpu;
    uint32_t        max_vgw_mem;
    uint32_t        max_vgw_disk;
    uint32_t        max_ips;
    struct lc_head  vm_allocated;
    uint32_t        vm_allocated_num;
};

struct lc_storage_with_rack {
    uint32_t            rack_id;
    struct lc_storage   *storage;
};

int ALLOC_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *ALLOC_LOG_LEVEL_P = &ALLOC_LOG_DEFAULT_LEVEL;

char *ALLOC_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

#define MIN(a, b) (a > b ? b : a)
#define MAX(a, b) (a > b ? a : b)

#ifdef UNIT_TEST
#define logger(level, format, ...)  \
    do { \
        char buffer[512] = { 0 }; \
        snprintf(buffer, 512, "[%s] ", __FUNCTION__); \
        strncat(buffer, format, 511 - strlen(buffer)); \
        printf(buffer, ##__VA_ARGS__); \
    } while (0);
#else
#define logger(level, format, ...)  \
    do { \
        char buffer[512] = { 0 }; \
        snprintf(buffer, 512, "[%s] ", __FUNCTION__); \
        strncat(buffer, format, 511 - strlen(buffer)); \
        if(level <= *ALLOC_LOG_LEVEL_P){ \
            syslog(level, buffer, ##__VA_ARGS__); \
        } \
    } while (0);
#endif

static void _sort_slist(node **head, node *end, cmprsn_func cmp)
{
    node *p = 0, *pivot = 0, **left = 0, **right = 0;
    if (*head == end || SLIST_NEXT(*head, chain) == end) {
        return;
    }

    pivot = *head;
    left = head;
    right = &SLIST_NEXT(*left, chain);

	for (p = SLIST_NEXT(*left, chain); p != end; p = SLIST_NEXT(p, chain)) {

        if (cmp(p->element, pivot->element) > 0) {
            *left = p;
            left = &(SLIST_NEXT(p, chain));
        } else {
            *right = p;
            right = &(SLIST_NEXT(p, chain));
        }
    }
    *right = end;
    *left = pivot;
    _sort_slist(head, pivot, cmp);
    _sort_slist(&SLIST_NEXT(pivot, chain), end, cmp);
}

static void sort_slist(head *list, cmprsn_func cmp)
{
    if (list && !SLIST_EMPTY(list)) {
        _sort_slist(&SLIST_FIRST(list), 0, cmp);
    }
}

static int cmprsn_host(void *_a, void *_b)
{
    struct lc_host *a = (struct lc_host *)_a;
    struct lc_host *b = (struct lc_host *)_b;
    node *n = 0;
    struct lc_storage *s = 0;
    int s_max_a = 0, s_max_b = 0;

    if (a->cpu_total - a->cpu_used != b->cpu_total - b->cpu_used) {
        return (a->cpu_total - a->cpu_used) - (b->cpu_total - b->cpu_used);
    }
    if (a->mem_total - a->mem_used != b->mem_total - b->mem_used) {
        return (a->mem_total - a->mem_used) - (b->mem_total - b->mem_used);
    }
    if (!SLIST_EMPTY(&a->storage)) {
        SLIST_FOREACH(n, &a->storage, chain) {
            s = (struct lc_storage *)n->element;
            if (s && s->storage_type == LC_STORAGE_TYPE_LOCAL) {
                if (s->disk_total - s->disk_used > s_max_a) {
                    s_max_a = s->disk_total - s->disk_used;
                }
            }
        }
    }
    if (!SLIST_EMPTY(&b->storage)) {
        SLIST_FOREACH(n, &b->storage, chain) {
            s = (struct lc_storage *)n->element;
            if (s && s->storage_type == LC_STORAGE_TYPE_LOCAL) {
                if (s->disk_total - s->disk_used > s_max_b) {
                    s_max_b = s->disk_total - s->disk_used;
                }
            }
        }
    }
    return s_max_a - s_max_b;
}

static int cmprsn_host_no_cpu(void *_a, void *_b)
{
    struct lc_host *a = (struct lc_host *)_a;
    struct lc_host *b = (struct lc_host *)_b;
    node *n = 0;
    struct lc_storage *s = 0;
    int s_max_a = 0, s_max_b = 0;

    if (a->mem_total - a->mem_used != b->mem_total - b->mem_used) {
        return (a->mem_total - a->mem_used) - (b->mem_total - b->mem_used);
    }
    if (!SLIST_EMPTY(&a->storage)) {
        SLIST_FOREACH(n, &a->storage, chain) {
            s = (struct lc_storage *)n->element;
            if (s && s->storage_type == LC_STORAGE_TYPE_LOCAL) {
                if (s->disk_total - s->disk_used > s_max_a) {
                    s_max_a = s->disk_total - s->disk_used;
                }
            }
        }
    }
    if (!SLIST_EMPTY(&b->storage)) {
        SLIST_FOREACH(n, &b->storage, chain) {
            s = (struct lc_storage *)n->element;
            if (s && s->storage_type == LC_STORAGE_TYPE_LOCAL) {
                if (s->disk_total - s->disk_used > s_max_b) {
                    s_max_b = s->disk_total - s->disk_used;
                }
            }
        }
    }
    return s_max_a - s_max_b;
}

static int cmprsn_group_host_by_rack(void *_a, void *_b)
{
    struct lc_host *a = (struct lc_host *)_a;
    struct lc_host *b = (struct lc_host *)_b;

    return b->rack_id - a->rack_id;
}

static int cmprsn_group_host_by_address(void *_a, void *_b)
{
    struct lc_host *a = (struct lc_host *)_a;
    struct lc_host *b = (struct lc_host *)_b;

    return strcmp(a->address, b->address);
}

static int cmprsn_group_vm_by_server(void *_a, void *_b)
{
    struct lc_vm *a = (struct lc_vm *)_a;
    struct lc_vm *b = (struct lc_vm *)_b;

    return strcmp(a->server, b->server);
}

static int cmprsn_group_ip_by_pool_id(void *_a, void *_b)
{
    struct lc_ip *a = (struct lc_ip *)_a;
    struct lc_ip *b = (struct lc_ip *)_b;

    return b->pool_id - a->pool_id;
}

static int cmprsn_rack(void *_a, void *_b)
{
    struct lc_rack *a = (struct lc_rack *)_a;
    struct lc_rack *b = (struct lc_rack *)_b;

    if (a->vm_cpu_total - a->vm_cpu_used != b->vm_cpu_total - b->vm_cpu_used) {
        return (a->vm_cpu_total - a->vm_cpu_used) - (b->vm_cpu_total - b->vm_cpu_used);
    }
    if (a->vm_mem_total - a->vm_mem_used != b->vm_mem_total - b->vm_mem_used) {
        return (a->vm_mem_total - a->vm_mem_used) - (b->vm_mem_total - b->vm_mem_used);
    }
    return (a->vm_disk_total - a->vm_disk_used) - (b->vm_disk_total - b->vm_disk_used);
}

static int cmprsn_rack_no_cpu(void *_a, void *_b)
{
    struct lc_rack *a = (struct lc_rack *)_a;
    struct lc_rack *b = (struct lc_rack *)_b;

    if (a->vm_mem_total - a->vm_mem_used != b->vm_mem_total - b->vm_mem_used) {
        return (a->vm_mem_total - a->vm_mem_used) - (b->vm_mem_total - b->vm_mem_used);
    }
    return (a->vm_disk_total - a->vm_disk_used) - (b->vm_disk_total - b->vm_disk_used);
}

static int cmprsn_rack_by_vm_allocated_num(void *_a, void *_b)
{
    struct lc_rack *a = (struct lc_rack *)_a;
    struct lc_rack *b = (struct lc_rack *)_b;

    return a->vm_allocated_num - b->vm_allocated_num;
}

static int ip_num_in_pool(struct lc_head *ips, int pool)
{
    node *n_ip = 0;
    struct lc_ip *ip = 0;
    int n = 0;

    if (ips && !SLIST_EMPTY(ips)) {
        sort_slist(ips, cmprsn_group_ip_by_pool_id);
        SLIST_FOREACH(n_ip, ips, chain) {
            ip = (struct lc_ip *)n_ip->element;
            if (ip->pool_id == pool) {
                ++n;
            } else if (n) {
                break;
            }
        }
    }

    return n;
}

int racks_construct(struct lc_head *racks,
        struct lc_head *vm_host, struct lc_head *vgw_host, struct lc_head *ips)
{
    node *n_vm = 0, *n_vgw = 0, *n_r_strg = 0, *n_h_strg = 0, *n_new = 0, *n_ip = 0; /* node rack/host storage */
    struct lc_rack *rack = 0;
    struct lc_host *h_vm = 0, *h_vgw = 0;
    struct lc_storage *r_strg = 0, *h_strg = 0;
    struct lc_ip *ip = 0;
    int pool_num, last_pool;
    int *ip_count = 0;
    int ret, i;

    ret = 0;
    pool_num = 0;
    SLIST_INIT(racks);
    if (ips && !SLIST_EMPTY(ips)) {
        sort_slist(ips, cmprsn_group_ip_by_pool_id);
        last_pool = -1;
        SLIST_FOREACH(n_ip, ips, chain) {
            ip = (struct lc_ip *)n_ip->element;
            if (ip) {
                if (ip->pool_id != last_pool) {
                    ++pool_num;
                }
                last_pool = ip->pool_id;
            }
        }
        ip_count = calloc(2 * pool_num, sizeof(int));
        if (!ip_count) {
            logger(LOG_ERR, "Memory allocate failed\n");
            goto finish;
        }
        i = -1;
        last_pool = -1;
        SLIST_FOREACH(n_ip, ips, chain) {
            ip = (struct lc_ip *)n_ip->element;
            if (ip) {
                if (ip->pool_id != last_pool) {
                    ++i;
                    if (i > pool_num) {
                        break;
                    }
                    ip_count[i << 1] = ip->pool_id;
                    ip_count[(i << 1) + 1] = 1;
                } else {
                    ++ip_count[(i << 1) + 1];
                }
                last_pool = ip->pool_id;
            }
        }
    }
    if ((!vm_host || SLIST_EMPTY(vm_host)) && (!vgw_host || SLIST_EMPTY(vgw_host))) {
        goto finish;
    }
    n_vm = 0;
    h_vm = 0;
    if (vm_host && !SLIST_EMPTY(vm_host)) {
        sort_slist(vm_host, cmprsn_group_host_by_rack);
        n_vm = SLIST_FIRST(vm_host);
        h_vm = (struct lc_host *)n_vm->element;
    }
    n_vgw = 0;
    h_vgw = 0;
    if (vgw_host && !SLIST_EMPTY(vgw_host)) {
        sort_slist(vgw_host, cmprsn_group_host_by_rack);
        n_vgw = SLIST_FIRST(vgw_host);
        h_vgw = (struct lc_host *)n_vgw->element;
    }
    while (1) {
        if (!h_vm && !h_vgw) {
            break;
        }
        n_new = malloc(sizeof(node));
        rack = malloc(sizeof(struct lc_rack));
        if (!n_new || !rack) {
            if (n_new) {
                free(n_new);
                n_new = 0;
            }
            if (rack) {
                free(rack);
                rack = 0;
            }
            logger(LOG_ERR, "Memory allocate failed\n");
            goto finish;
        }
        memset(n_new, 0, sizeof(node));
        memset(rack, 0, sizeof(struct lc_rack));
        n_new->element = rack;
        SLIST_INSERT_HEAD(racks, n_new, chain);
        /* get rack id */
        if ((h_vm && h_vgw && h_vm->rack_id <= h_vgw->rack_id) || (h_vm && !h_vgw)) {
            rack->id = h_vm->rack_id;
        } else if ((h_vm && h_vgw && h_vm->rack_id > h_vgw->rack_id) || (!h_vm && h_vgw)) {
            rack->id = h_vgw->rack_id;
        }
        while (h_vm && h_vm->rack_id == rack->id) {
            n_new = malloc(sizeof(node));
            if (!n_new) {
                logger(LOG_ERR, "Memory allocate failed\n");
                goto finish;
            }
            memset(n_new, 0, sizeof(node));
            n_new->element = h_vm;
            SLIST_INSERT_HEAD(&rack->vm_host, n_new, chain);
            rack->vm_cpu_total += h_vm->cpu_total;
            rack->vm_cpu_used += h_vm->cpu_used;
            rack->vm_mem_total += h_vm->mem_total;
            rack->vm_mem_used += h_vm->mem_used;
            if (!SLIST_EMPTY(&h_vm->storage)) {
                SLIST_FOREACH(n_h_strg, &h_vm->storage, chain) {
                    h_strg = (struct lc_storage *)n_h_strg->element;
                    if (!h_strg) {
                        continue;
                    }
                    if (h_strg->storage_type == LC_STORAGE_TYPE_LOCAL) {
                        rack->vm_disk_total += h_strg->disk_total;
                        rack->vm_disk_used += h_strg->disk_used;
                        continue;
                    } else if (h_strg->storage_type == LC_STORAGE_TYPE_SHARED) {
                        /* check duplicate */
                        r_strg = 0;
                        if (!SLIST_EMPTY(&rack->shared_storage)) {
                            SLIST_FOREACH(n_r_strg, &rack->shared_storage, chain) {
                                r_strg = (struct lc_storage *)n_r_strg->element;
                                if (!r_strg) {
                                    continue;
                                }
                                if (!strcmp(h_strg->name_label, r_strg->name_label)) {
                                    break;
                                }
                            }
                        }
                        if (r_strg && !strcmp(h_strg->name_label, r_strg->name_label)) {
                            continue;
                        }
                        /* new shared storage */
                        n_new = malloc(sizeof(node));
                        if (!n_new) {
                            logger(LOG_ERR, "Memory allocate failed\n");
                            goto finish;
                        }
                        memset(n_new, 0, sizeof(node));
                        n_new->element = h_strg;
                        SLIST_INSERT_HEAD(&rack->shared_storage, n_new, chain);
                        rack->vm_disk_total += h_strg->disk_total;
                        rack->vm_disk_used += h_strg->disk_used;
                    }
                }
            }
            n_vm = SLIST_NEXT(n_vm, chain);
            if (n_vm) {
                h_vm = (struct lc_host *)n_vm->element;
            } else {
                h_vm = 0;
            }
        }
        while (h_vgw && h_vgw->rack_id == rack->id) {
            n_new = malloc(sizeof(node));
            if (!n_new) {
                logger(LOG_ERR, "Memory allocate failed\n");
                goto finish;
            }
            memset(n_new, 0, sizeof(node));
            n_new->element = h_vgw;
            SLIST_INSERT_HEAD(&rack->vgw_host, n_new, chain);
            if (h_vgw->cpu_total - h_vgw->cpu_used > rack->max_vgw_cpu) {
                rack->max_vgw_cpu = h_vgw->cpu_total - h_vgw->cpu_used;
            }
            if (h_vgw->mem_total - h_vgw->mem_used > rack->max_vgw_mem) {
                rack->max_vgw_mem = h_vgw->mem_total - h_vgw->mem_used;
            }
            if (!SLIST_EMPTY(&h_vgw->storage)) {
                SLIST_FOREACH(n_h_strg, &h_vgw->storage, chain) {
                    h_strg = (struct lc_storage *)n_h_strg->element;
                    if (!h_strg) {
                        continue;
                    }
                    if (h_strg->storage_type == LC_STORAGE_TYPE_LOCAL) {
                        if (h_strg->disk_total - h_strg->disk_used > rack->max_vgw_disk) {
                            rack->max_vgw_disk = h_strg->disk_total - h_strg->disk_used;
                        }
                    }
                    /* Presume that xen hosts does not have shared storage */
                }
            }
            if (ip_count) {
                if (h_vgw->pool_ip_type == LC_POOL_IP_TYPE_GLOBAL) {
                    for (i = 0; i < pool_num; ++i) {
                        if (ip_count[i << 1] == 0) {
                            if (ip_count[(i << 1) + 1] > rack->max_ips) {
                                rack->max_ips = ip_count[(i << 1) + 1];
                            }
                            break;
                        }
                    }
                } else if (h_vgw->pool_ip_type == LC_POOL_IP_TYPE_EXCLUSIVE) {
                    for (i = 0; i < pool_num; ++i) {
                        if (ip_count[i << 1] == h_vgw->pool_id) {
                            if (ip_count[(i << 1) + 1] > rack->max_ips) {
                                rack->max_ips = ip_count[(i << 1) + 1];
                            }
                            break;
                        }
                    }
                }
            }
            n_vgw = SLIST_NEXT(n_vgw, chain);
            if (n_vgw) {
                h_vgw = (struct lc_host *)n_vgw->element;
            } else {
                h_vgw = 0;
            }
        }
    }

finish:

    if (ip_count) {
        free(ip_count);
    }
    return ret;
}

void racks_destruct(head *racks)
{
    node *n = 0;
    struct lc_rack *rack = 0;

    while (!SLIST_EMPTY(racks)) {
        n = SLIST_FIRST(racks);
        SLIST_REMOVE_HEAD(racks, chain);
        rack = 0;
        if (n) {
            rack = (struct lc_rack *)n->element;
            free(n);
            n = 0;
        }
        if (rack) {
            while (!SLIST_EMPTY(&rack->shared_storage)) {
                n = SLIST_FIRST(&rack->shared_storage);
                SLIST_REMOVE_HEAD(&rack->shared_storage, chain);
                if (n) {
                    free(n);
                    n = 0;
                }
            }
            while (!SLIST_EMPTY(&rack->vgw_host)) {
                n = SLIST_FIRST(&rack->vgw_host);
                SLIST_REMOVE_HEAD(&rack->vgw_host, chain);
                if (n) {
                    free(n);
                    n = 0;
                }
            }
            while (!SLIST_EMPTY(&rack->vm_host)) {
                n = SLIST_FIRST(&rack->vm_host);
                SLIST_REMOVE_HEAD(&rack->vm_host, chain);
                if (n) {
                    free(n);
                    n = 0;
                }
            }
            free(rack);
            rack = 0;
        }
    }
}

static int try_vgateways_in_rack(struct lc_head *vgws,
                                             struct lc_rack *rack)
{
    node *n_host = 0, *n_vgw = 0;
    struct lc_vgw *vgw = 0, *first_vgw = 0;
    struct lc_host *host = 0;
    int ok;
    char domain_ip[LC_EXCLUDED_SERVER_LEN];

    if (!vgws || !rack) {
        return -1;
    }

    if (SLIST_EMPTY(vgws)) {
        logger(LOG_INFO, "No vGWs to try\n");
        return -1;
    }

    if (SLIST_EMPTY(&rack->vgw_host)) {
        logger(LOG_INFO, "No vGATEWAY host in this rack\n");
        SLIST_FOREACH(n_vgw, vgws, chain) {
            if (!n_vgw) {
                continue;
            }
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                continue;
            }
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
        }
        return -1;
    }

    n_vgw = SLIST_FIRST(vgws);
    if (n_vgw) {
        vgw = (struct lc_vgw *)n_vgw->element;
    }
    if (!n_vgw || !vgw) {
        logger(LOG_INFO, "No vGATEWAYs to try\n");
        return -1;
    }
    sort_slist(&rack->vgw_host, cmprsn_host);
    n_host = SLIST_FIRST(&rack->vgw_host);
    while (1) {
        host = 0;
        if (n_host) {
            host = (struct lc_host *)n_host->element;
        }
        if (host) {
            break;
        }
        n_host = SLIST_NEXT(n_host, chain);
        if (!n_host) {
            logger(LOG_INFO, "No vGATEWAY host in this rack\n");
            SLIST_FOREACH(n_vgw, vgws, chain) {
                vgw = (struct lc_vgw *)n_vgw->element;
                if (!vgw) {
                    continue;
                }
                vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
            }
            return -1;
        }
    }
    /* assume all vgateways need same resource */
    SLIST_FOREACH(n_vgw, vgws, chain) {
        ok = 0;
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw) {
            continue;
        }
        if (!n_host || !host) {
            break;
        }
        logger(LOG_INFO, "Trying to allocate vGATEWAY %d\n", vgw->id);
        while (1) {
            memset(domain_ip, 0, sizeof domain_ip);
            snprintf(domain_ip, LC_EXCLUDED_SERVER_LEN, "%s:%s",
                    host->domain, host->address);
            if (strstr(vgw->excluded_server, domain_ip)) {
                logger(LOG_DEBUG, "Server %s in domain %s excluded for \
                        vGATEWAY %d\n",
                        host->address, host->domain, vgw->id);
                vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
                goto failed_in_host;
            }
            /* ok here */
            ok = 1;
            strncpy(vgw->server, host->address, LC_IPV4_LEN - 1);
            vgw->pool_id = host->pool_id;
            vgw->pool_ip_type = host->pool_ip_type;
            strncpy(vgw->domain, host->domain, LC_DOMAIN_LEN - 1);
            vgw->alloc_errno ^= vgw->alloc_errno;

            logger(LOG_INFO, "vGATEWAY %d suits in server %s in domain %s\n",
                    vgw->id, host->address, host->domain);

            break;

failed_in_host:

            n_host = SLIST_NEXT(n_host, chain);
            host = 0;
            if (n_host) {
                host = (struct lc_host *)n_host->element;
            }
            if (!n_host || !host) {
                break;
            }
        }

        if (!ok) {
            goto revert;
        }
        n_host = SLIST_NEXT(n_host, chain);
        host = 0;
        if (n_host) {
            host = (struct lc_host *)n_host->element;
        }
    }

    /* not all allocated */
    if (n_vgw) {
        logger(LOG_DEBUG, "Not all vGATEWAYs allocated, host not enough in rack\n");
        n_vgw = SLIST_FIRST(vgws);
        if (n_vgw) {
            first_vgw = (struct lc_vgw *)n_vgw->element;
        }
        if (first_vgw) {
            first_vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
        }
        goto revert;
    }

    n_vgw = SLIST_FIRST(vgws);
    if (n_vgw) {
        first_vgw = (struct lc_vgw *)n_vgw->element;
    }
    /* not happening */
    if (!n_vgw || !first_vgw) {
        goto revert;
    }

    SLIST_FOREACH(n_vgw, vgws, chain) {
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw || vgw == first_vgw) {
            continue;
        }
        vgw->alloc_errno = first_vgw->alloc_errno;
    }

    logger(LOG_INFO, "vGATEWAYs allocated\n");
    return 0;

revert:

    n_vgw = SLIST_FIRST(vgws);
    if (n_vgw) {
        first_vgw = (struct lc_vgw *)n_vgw->element;
    }
    /* not happening */
    if (!n_vgw || !first_vgw) {
        return -1;
    }
    SLIST_FOREACH(n_vgw, vgws, chain) {
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw || !vgw->server[0]) {
            continue;
        }
        SLIST_FOREACH(n_host, &rack->vgw_host, chain) {
            host = (struct lc_host *)n_host->element;
            if (!host) {
                continue;
            }
            if (!strcmp(host->address, vgw->server)) {
                break;
            }
        }
        if (!n_host || !host || strcmp(host->address, vgw->server)) {
            continue;
        }

        logger(LOG_DEBUG, "Remove vGATEWAY %d from server %s in domain %s\n",
                vgw->id, vgw->server, vgw->domain);
        memset(vgw->server, 0, LC_IPV4_LEN);
        vgw->pool_id = 0;
        memset(vgw->domain, 0, LC_DOMAIN_LEN);
        memset(vgw->ip, 0, LC_VGW_PUBINTF_NUM * LC_VGW_IPS_LEN);
        first_vgw->alloc_errno |= vgw->alloc_errno;
    }
    SLIST_FOREACH(n_vgw, vgws, chain) {
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw) {
            continue;
        }
        vgw->alloc_errno = first_vgw->alloc_errno;
    }

    return -1;
}


static int try_vgws_in_rack(struct lc_head *vgws, struct lc_rack *rack,
        struct lc_head *ips, int flag)
{
    node *n_host = 0, *n_storage = 0, *n_ip = 0, *n_ip_tmp = 0, *n_vgw = 0;
    struct lc_vgw *vgw = 0, *first_vgw = 0;
    struct lc_host *host = 0;
    struct lc_storage *storage = 0;
    struct lc_ip *ip = 0;
    int i, j, len, vgw_ips, ok;
    char domain_ip[LC_EXCLUDED_SERVER_LEN];

    if (!vgws || !rack) {
        return -1;
    }

    if (SLIST_EMPTY(vgws)) {
        logger(LOG_INFO, "No vGWs to try\n");
        return -1;
    }

    if (SLIST_EMPTY(&rack->vgw_host)) {
        logger(LOG_INFO, "No vGW host in this rack\n");
        SLIST_FOREACH(n_vgw, vgws, chain) {
            if (!n_vgw) {
                continue;
            }
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                continue;
            }
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
        }
        return -1;
    }

    n_vgw = SLIST_FIRST(vgws);
    if (n_vgw) {
        vgw = (struct lc_vgw *)n_vgw->element;
    }
    if (!n_vgw || !vgw) {
        logger(LOG_INFO, "No vGWs to try\n");
        return -1;
    }

    vgw_ips = 0;
    for (i = 0; i < LC_VGW_PUBINTF_NUM; ++i) {
        vgw_ips += vgw->ip_num[i];
    }
    if ((!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
             vgw->cpu_num > rack->max_vgw_cpu) ||
        vgw->mem_size > rack->max_vgw_mem ||
        vgw->sys_vdi_size > rack->max_vgw_disk ||
        vgw_ips > rack->max_ips) {

        if (!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                vgw->cpu_num > rack->max_vgw_cpu) {
            logger(LOG_DEBUG, "Insufficient CPU in rack %d\n", rack->id);
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_CPU;
        }
        if (vgw->mem_size > rack->max_vgw_mem) {
            logger(LOG_DEBUG, "Insufficient memory in rack %d\n", rack->id);
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_MEMORY;
        }
        if (vgw->sys_vdi_size > rack->max_vgw_disk) {
            logger(LOG_DEBUG, "Insufficient storage in rack %d\n", rack->id);
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_DISK;
        }
        if (vgw_ips > rack->max_ips) {
            logger(LOG_DEBUG, "Insufficient IP resource in pools in rack %d\n",
                    rack->id);
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_IP;
        }
        return -1;
    }

    if (flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) {
        sort_slist(&rack->vgw_host, cmprsn_host_no_cpu);
    } else {
        sort_slist(&rack->vgw_host, cmprsn_host);
    }
    n_host = SLIST_FIRST(&rack->vgw_host);
    while (1) {
        host = 0;
        if (n_host) {
            host = (struct lc_host *)n_host->element;
        }
        if (host) {
            break;
        }
        n_host = SLIST_NEXT(n_host, chain);
        if (!n_host) {
            logger(LOG_INFO, "No vGW host in this rack\n");
            SLIST_FOREACH(n_vgw, vgws, chain) {
                vgw = (struct lc_vgw *)n_vgw->element;
                if (!vgw) {
                    continue;
                }
                vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
            }
            return -1;
        }
    }
    /* assume all vgws need same resource */
    SLIST_FOREACH(n_vgw, vgws, chain) {
        ok = 0;
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw) {
            continue;
        }
        if (!n_host || !host) {
            break;
        }
        logger(LOG_INFO, "Trying to allocate vGW %d\n", vgw->id);
        while (1) {
            memset(domain_ip, 0, sizeof domain_ip);
            snprintf(domain_ip, LC_EXCLUDED_SERVER_LEN, "%s:%s",
                    host->domain, host->address);
            if (strstr(vgw->excluded_server, domain_ip)) {
                logger(LOG_DEBUG, "Server %s in domain %s excluded for vGW %d\n",
                        host->address, host->domain, vgw->id);
                vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
                goto failed_in_host;
            }
            if (SLIST_EMPTY(&host->storage)) {
                logger(LOG_DEBUG, "No storage in server %s in domain %s\n",
                        host->address, host->domain);
                vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_DISK;
                goto failed_in_host;
            }
            n_storage = SLIST_FIRST(&host->storage);
            storage = (struct lc_storage *)n_storage->element;
            if (!storage) {
                vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_DISK;
                goto failed_in_host;
            }
            if ((!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                     vgw->cpu_num + host->cpu_used > host->cpu_total) ||
                vgw->mem_size + host->mem_used > host->mem_total ||
                vgw->sys_vdi_size + storage->disk_used > storage->disk_total ||
                (host->pool_ip_type == LC_POOL_IP_TYPE_GLOBAL &&
                 vgw_ips > ip_num_in_pool(ips, 0)) ||
                (host->pool_ip_type == LC_POOL_IP_TYPE_EXCLUSIVE &&
                 vgw_ips > ip_num_in_pool(ips, host->pool_id))) {

                if (!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                        vgw->cpu_num + host->cpu_used > host->cpu_total) {
                    logger(LOG_DEBUG, "Insufficient CPU in server %s in domain %s\n", host->address, host->domain);
                    vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_CPU;
                }
                if (vgw->mem_size + host->mem_used > host->mem_total) {
                    logger(LOG_DEBUG, "Insufficient memory in server %s in domain %s\n", host->address, host->domain);
                    vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_MEMORY;
                }
                if (vgw->sys_vdi_size + storage->disk_used > storage->disk_total) {
                    logger(LOG_DEBUG, "Insufficient storage in server %s in domain %s\n", host->address, host->domain);
                    vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_DISK;
                }
                if ((host->pool_ip_type == LC_POOL_IP_TYPE_GLOBAL &&
                     vgw_ips > ip_num_in_pool(ips, 0)) ||
                    (host->pool_ip_type == LC_POOL_IP_TYPE_EXCLUSIVE &&
                     vgw_ips > ip_num_in_pool(ips, host->pool_id))) {
                    logger(LOG_DEBUG, "Insufficient IP resource in server %s in domain %s\n", host->address, host->domain);
                    vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_IP;
                }
                goto failed_in_host;
            }
            /* ok here */
            ok = 1;
            strncpy(vgw->sys_sr_name, storage->name_label, LC_NAMESIZE - 1);
            strncpy(vgw->server, host->address, LC_IPV4_LEN - 1);
            vgw->pool_id = host->pool_id;
            vgw->pool_ip_type = host->pool_ip_type;
            strncpy(vgw->domain, host->domain, LC_DOMAIN_LEN - 1);
            vgw->alloc_errno ^= vgw->alloc_errno;

            host->cpu_used += vgw->cpu_num;
            host->mem_used += vgw->mem_size;
            storage->disk_used += vgw->sys_vdi_size;
            logger(LOG_INFO, "vGW %d suits in server %s in domain %s\n",
                    vgw->id, host->address, host->domain);

            break;

failed_in_host:

            n_host = SLIST_NEXT(n_host, chain);
            host = 0;
            if (n_host) {
                host = (struct lc_host *)n_host->element;
            }
            if (!n_host || !host) {
                break;
            }
        }

        if (!ok) {
            goto revert;
        }
        n_host = SLIST_NEXT(n_host, chain);
        host = 0;
        if (n_host) {
            host = (struct lc_host *)n_host->element;
        }
    }

    /* not all allocated */
    if (n_vgw) {
        logger(LOG_DEBUG, "Not all vGWs allocated, host not enough in rack\n");
        n_vgw = SLIST_FIRST(vgws);
        if (n_vgw) {
            first_vgw = (struct lc_vgw *)n_vgw->element;
        }
        if (first_vgw) {
            first_vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
        }
        goto revert;
    }

    n_vgw = SLIST_FIRST(vgws);
    if (n_vgw) {
        first_vgw = (struct lc_vgw *)n_vgw->element;
    }
    /* not happening */
    if (!n_vgw || !first_vgw) {
        goto revert;
    }

    if (vgw_ips > 0 && !SLIST_EMPTY(ips)) {
        sort_slist(ips, cmprsn_group_ip_by_pool_id);
        n_ip = SLIST_FIRST(ips);
        ip = (struct lc_ip *)n_ip->element;
        while (n_ip && ip &&
               ((vgw->pool_ip_type == LC_POOL_IP_TYPE_GLOBAL &&
                 ip->pool_id != 0) ||
                (vgw->pool_ip_type == LC_POOL_IP_TYPE_EXCLUSIVE &&
                 ip->pool_id != first_vgw->pool_id))) {
            n_ip = SLIST_NEXT(n_ip, chain);
            if (n_ip) {
                ip = (struct lc_ip *)n_ip->element;
            }
        }
        n_ip_tmp = n_ip;
        for (i = 0; i < LC_VGW_PUBINTF_NUM; ++i) {
            n_ip = n_ip_tmp;
            ip = (struct lc_ip *)n_ip->element;
            j = 0;
            while (j < MIN(first_vgw->ip_num[i], LC_VGW_IP_NUM_PER_INTF)) {
                if (!n_ip || !ip ||
                    (vgw->pool_ip_type == LC_POOL_IP_TYPE_GLOBAL &&
                     ip->pool_id != 0) ||
                    (vgw->pool_ip_type == LC_POOL_IP_TYPE_EXCLUSIVE &&
                     ip->pool_id != first_vgw->pool_id)) {
                    logger(LOG_DEBUG, "Insufficient IP resource in pool %d\n",
                            first_vgw->pool_id);
                    first_vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_IP;
                    goto revert;
                }
                if (ip->isp == first_vgw->ip_isp[i]) {
                    if (j == 0) {
                        len = MIN(LC_VGW_IPS_LEN, LC_IPV4_LEN);
                        memset(first_vgw->ip[i], 0, LC_VGW_IPS_LEN);
                        strncpy(first_vgw->ip[i], ip->address, len - 1);
                    } else {
                        len = LC_VGW_IPS_LEN - strlen(first_vgw->ip[i]);
                        strncat(first_vgw->ip[i], "#", len - 1);
                        ++len;
                        len = MIN(len, LC_IPV4_LEN);
                        strncat(first_vgw->ip[i], ip->address, len - 1);
                    }
                    ++j;
                }
                n_ip = SLIST_NEXT(n_ip, chain);
                if (n_ip) {
                    ip = (struct lc_ip *)n_ip->element;
                }
            }
        }
    }

    SLIST_FOREACH(n_vgw, vgws, chain) {
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw || vgw == first_vgw) {
            continue;
        }
        for (i = 0; i < LC_VGW_PUBINTF_NUM; ++i) {
            memcpy(vgw->ip[i], first_vgw->ip[i], LC_VGW_IPS_LEN);
        }
        vgw->alloc_errno = first_vgw->alloc_errno;
    }

    logger(LOG_INFO, "vGWs allocated\n");
    return 0;

revert:

    n_vgw = SLIST_FIRST(vgws);
    if (n_vgw) {
        first_vgw = (struct lc_vgw *)n_vgw->element;
    }
    /* not happening */
    if (!n_vgw || !first_vgw) {
        return -1;
    }
    SLIST_FOREACH(n_vgw, vgws, chain) {
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw || !vgw->server[0]) {
            continue;
        }
        SLIST_FOREACH(n_host, &rack->vgw_host, chain) {
            host = (struct lc_host *)n_host->element;
            if (!host) {
                continue;
            }
            if (!strcmp(host->address, vgw->server)) {
                break;
            }
        }
        if (!n_host || !host || strcmp(host->address, vgw->server)) {
            continue;
        }

        if (SLIST_EMPTY(&host->storage)) {
            continue;
        }
        storage = 0;
        SLIST_FOREACH(n_storage, &host->storage, chain) {
            storage = (struct lc_storage *)n_storage->element;
            if (!storage) {
                continue;
            }
            if (!strcmp(storage->name_label, vgw->sys_sr_name)) {
                break;
            }
        }
        if (!storage || strcmp(storage->name_label, vgw->sys_sr_name)) {
            return -1;
        }

        logger(LOG_DEBUG, "Remove vGW %d from server %s in domain %s\n",
                vgw->id, vgw->server, vgw->domain);
        host->cpu_used -= vgw->cpu_num;
        host->mem_used -= vgw->mem_size;
        storage->disk_used -= vgw->sys_vdi_size;
        memset(vgw->sys_sr_name, 0, LC_NAMESIZE);
        memset(vgw->server, 0, LC_IPV4_LEN);
        vgw->pool_id = 0;
        memset(vgw->domain, 0, LC_DOMAIN_LEN);
        memset(vgw->ip, 0, LC_VGW_PUBINTF_NUM * LC_VGW_IPS_LEN);

        first_vgw->alloc_errno |= vgw->alloc_errno;
    }
    SLIST_FOREACH(n_vgw, vgws, chain) {
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw) {
            continue;
        }
        vgw->alloc_errno = first_vgw->alloc_errno;
    }

    return -1;
}

static int try_vms_in_rack(struct lc_head *vms, struct lc_rack *rack, int flag)
{
    node *n_vm = 0, *n_vm_tmp, *n_host = 0, *n_storage = 0, *n_s_storage = 0;
    struct lc_host *host = 0;
    struct lc_storage *storage = 0, *s_storage = 0;
    struct lc_vm *vm = 0;
    char domain_ip[LC_EXCLUDED_SERVER_LEN];
    int ok;

    if (!vms || !rack) {
        return -1;
    }

    if (SLIST_EMPTY(vms)) {
        logger(LOG_INFO, "No VMs to try\n");
        return 0;
    }

    if (SLIST_EMPTY(&rack->vm_host)) {
        logger(LOG_INFO, "No VM host in this rack\n");
        SLIST_FOREACH(n_vm, vms, chain) {
            if (!n_vm) {
                continue;
            }
            vm = (struct lc_vm *)n_vm->element;
            if (!vm) {
                continue;
            }
            vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_HOST;
        }
        return 0;
    }

    n_vm = SLIST_FIRST(vms);
    while (n_vm) {
        ok = 0;
        vm = (struct lc_vm *)n_vm->element;
        if (vm) {
            logger(LOG_DEBUG, "Trying to allocate VM %d\n", vm->id);
            if (!SLIST_EMPTY(&rack->vm_host)) {
                if (flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) {
                    sort_slist(&rack->vm_host, cmprsn_host_no_cpu);
                } else {
                    sort_slist(&rack->vm_host, cmprsn_host);
                }
                SLIST_FOREACH(n_host, &rack->vm_host, chain) {
                    host = (struct lc_host *)n_host->element;
                    if (!host) {
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_HOST;
                        continue;
                    }
                    if (SLIST_EMPTY(&host->storage)) {
                        logger(LOG_DEBUG, "No storage in server %s in domain %s\n", host->address, host->domain);
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                        continue;
                    }
                    n_storage = SLIST_FIRST(&host->storage);
                    storage = (struct lc_storage *)n_storage->element;
                    if (!storage) {
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                        continue;
                    }
                    memset(domain_ip, 0, sizeof domain_ip);
                    snprintf(domain_ip, LC_EXCLUDED_SERVER_LEN, "%s:%s", host->domain, host->address);
                    if (strstr(vm->excluded_server, domain_ip)) {
                        logger(LOG_DEBUG, "Server %s in domain %s excluded for VM %d\n", host->address, host->domain, vm->id);
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_HOST;
                        continue;
                    }
                    /* check shared */
                    if (storage->storage_type == LC_STORAGE_TYPE_SHARED) {
                        s_storage = 0;
                        SLIST_FOREACH(n_s_storage, &rack->shared_storage, chain) {
                            s_storage = (struct lc_storage *)n_s_storage->element;
                            if (!s_storage) {
                                continue;
                            }
                            if (!strcmp(s_storage->name_label, storage->name_label)) {
                                break;
                            }
                        }
                        if (s_storage && !strcmp(s_storage->name_label, storage->name_label)) {
                            storage = s_storage;
                        } else {
                            continue;
                        }
                    }
                    if ((!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                            vm->cpu_num + host->cpu_used > host->cpu_total) ||
                        vm->mem_size + host->mem_used > host->mem_total ||
                        vm->sys_vdi_size + vm->usr_vdi_size + storage->disk_used > storage->disk_total) {

                        if (!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                                vm->cpu_num + host->cpu_used > host->cpu_total) {
                            logger(LOG_DEBUG, "Insufficient CPU in server %s in domain %s\n", host->address, host->domain);
                            vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_CPU;
                        }
                        if (vm->mem_size + host->mem_used > host->mem_total) {
                            logger(LOG_DEBUG, "Insufficient memory in server %s in domain %s\n", host->address, host->domain);
                            vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_MEMORY;
                        }
                        if (vm->sys_vdi_size + vm->usr_vdi_size + storage->disk_used > storage->disk_total) {
                            logger(LOG_DEBUG, "Insufficient storage in server %s in domain %s\n", host->address, host->domain);
                            vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                        }
                        continue;
                    }
                    /* ok here */
                    strncpy(vm->sys_sr_name, storage->name_label, LC_NAMESIZE - 1);
                    if (vm->usr_vdi_size > 0) {
                        strncpy(vm->usr_sr_name, storage->name_label, LC_NAMESIZE - 1);
                    }
                    strncpy(vm->server, host->address, LC_IPV4_LEN - 1);
                    vm->pool_id = host->pool_id;
                    strncpy(vm->domain, host->domain, LC_DOMAIN_LEN - 1);
                    vm->alloc_errno ^= vm->alloc_errno;

                    host->cpu_used += vm->cpu_num;
                    host->mem_used += vm->mem_size;
                    storage->disk_used += vm->sys_vdi_size + vm->usr_vdi_size;

                    rack->vm_cpu_used += vm->cpu_num;
                    rack->vm_mem_used += vm->mem_size;
                    rack->vm_disk_used += vm->sys_vdi_size + vm->usr_vdi_size;

                    n_vm_tmp = n_vm;
                    n_vm = SLIST_NEXT(n_vm, chain);
                    SLIST_REMOVE(vms, n_vm_tmp, lc_node, chain);
                    SLIST_INSERT_HEAD(&rack->vm_allocated, n_vm_tmp, chain);

                    ++rack->vm_allocated_num;

                    ok = 1;
                    logger(LOG_DEBUG, "VM %d suits in server %s in domain %s\n", vm->id, host->address, host->domain);
                    break;
                }

                if (!ok) {
                    logger(LOG_INFO, "VM %d allocate failed\n", vm->id);
                    n_vm = SLIST_NEXT(n_vm, chain);
                }
            }
        }
    }

    return 0;
}

/* release resource allocated */
static int revert_vgws_in_rack(struct lc_head *vgws, struct lc_rack *rack)
{
    node *n_host = 0, *n_storage = 0, *n_vgw = 0;
    struct lc_host *host = 0;
    struct lc_storage *storage = 0;
    struct lc_vgw *vgw = 0;

    if (!vgws || !rack || SLIST_EMPTY(&rack->vgw_host)) {
        return -1;
    }
    if (!SLIST_EMPTY(vgws)) {
        return 0;
    }
    SLIST_FOREACH(n_vgw, vgws, chain) {
        vgw = (struct lc_vgw *)n_vgw->element;
        if (!vgw || !vgw->server[0]) {
            continue;
        }
        SLIST_FOREACH(n_host, &rack->vgw_host, chain) {
            host = (struct lc_host *)n_host->element;
            if (!host) {
                continue;
            }
            if (!strcmp(host->address, vgw->server)) {
                break;
            }
        }
        if (!n_host || !host || strcmp(host->address, vgw->server)) {
            continue;
        }

        if (SLIST_EMPTY(&host->storage)) {
            continue;
        }
        storage = 0;
        SLIST_FOREACH(n_storage, &host->storage, chain) {
            storage = (struct lc_storage *)n_storage->element;
            if (!storage) {
                continue;
            }
            if (!strcmp(storage->name_label, vgw->sys_sr_name)) {
                break;
            }
        }
        if (!storage || strcmp(storage->name_label, vgw->sys_sr_name)) {
            continue;
        }

        logger(LOG_DEBUG, "Remove vGW %d from server %s in domain %s\n", vgw->id, vgw->server, vgw->domain);
        host->cpu_used -= vgw->cpu_num;
        host->mem_used -= vgw->mem_size;
        storage->disk_used -= vgw->sys_vdi_size;
        memset(vgw->sys_sr_name, 0, LC_NAMESIZE);
        memset(vgw->server, 0, LC_IPV4_LEN);
        vgw->pool_id = 0;
        memset(vgw->domain, 0, LC_DOMAIN_LEN);
        memset(vgw->ip, 0, LC_VGW_PUBINTF_NUM * LC_VGW_IPS_LEN);
    }

    return 0;
}

static int revert_vms_in_rack(struct lc_head *vms, struct lc_rack *rack)
{
    node *n_vm = 0, *n_vm_tmp, *n_host = 0, *n_storage = 0, *n_s_storage = 0;
    struct lc_host *host = 0;
    struct lc_storage *storage = 0, *storage_sys = 0, *storage_usr = 0, *s_storage = 0;
    struct lc_vm *vm = 0;

    if (!rack || SLIST_EMPTY(&rack->vm_allocated)) {
        return 0;
    }
    if (!vms || SLIST_EMPTY(&rack->vm_host)) {
        return 0;
    }
    sort_slist(&rack->vm_host, cmprsn_group_host_by_address);
    sort_slist(&rack->vm_allocated, cmprsn_group_vm_by_server);

    n_host = SLIST_FIRST(&rack->vm_host);
    if (!n_host) {
        return -1;
    }
    host = (struct lc_host *)n_host->element;
    n_vm = SLIST_FIRST(&rack->vm_allocated);
    while (n_vm) {
        vm = (struct lc_vm *)n_vm->element;
        if (vm) {
            if (!n_host || !host) {
                return -1;
            }
            while (n_host && host && strcmp(host->address, vm->server)) {
                n_host = SLIST_NEXT(n_host, chain);
                if (n_host) {
                    host = (struct lc_host *)n_host->element;
                }
            }
            if (!n_host || !host || strcmp(host->address, vm->server)) {
                return -1;
            }

            if (SLIST_EMPTY(&host->storage)) {
                return -1;
            }
            storage_sys = 0;
            storage_usr = 0;
            SLIST_FOREACH(n_storage, &host->storage, chain) {
                storage = (struct lc_storage *)n_storage->element;
                if (!storage) {
                    continue;
                }
                if (!strcmp(storage->name_label, vm->sys_sr_name)) {
                    storage_sys = storage;
                }
                if (vm->usr_vdi_size > 0) {
                    if (!strcmp(storage->name_label, vm->usr_sr_name)) {
                        storage_usr = storage;
                    }
                }
                if (storage_sys && (vm->usr_vdi_size <= 0 || storage_usr)) {
                    break;
                }
            }
            if (!storage_sys || strcmp(storage_sys->name_label, vm->sys_sr_name)) {
                return -1;
            }
            if (vm->usr_vdi_size > 0) {
                if (!storage_usr || strcmp(storage_usr->name_label, vm->usr_sr_name)) {
                    return -1;
                }
            }
            if (storage_sys->storage_type == LC_STORAGE_TYPE_SHARED ||
                (vm->usr_vdi_size > 0 &&
                 storage_usr->storage_type == LC_STORAGE_TYPE_SHARED)) {

                s_storage = 0;
                SLIST_FOREACH(n_s_storage, &rack->shared_storage, chain) {
                    s_storage = (struct lc_storage *)n_s_storage->element;
                    if (!s_storage) {
                        continue;
                    }
                    if (!strcmp(s_storage->name_label, storage_sys->name_label)) {
                        storage_sys = s_storage;
                    }
                    if (vm->usr_vdi_size > 0) {
                        if (!strcmp(s_storage->name_label, storage_usr->name_label)) {
                            storage_usr = s_storage;
                        }
                    }
                }
            }
            if (!storage_sys || strcmp(storage_sys->name_label, vm->sys_sr_name)) {
                return -1;
            }
            if (vm->usr_vdi_size > 0) {
                if (!storage_usr || strcmp(storage_usr->name_label, vm->usr_sr_name)) {
                    return -1;
                }
            }

            logger(LOG_DEBUG, "Remove VM %d from server %s in domain %s\n", vm->id, vm->server, vm->domain);
            --rack->vm_allocated_num;
            n_vm_tmp = n_vm;
            n_vm = SLIST_NEXT(n_vm, chain);
            SLIST_REMOVE(&rack->vm_allocated, n_vm_tmp, lc_node, chain);
            SLIST_INSERT_HEAD(vms, n_vm_tmp, chain);

            host->cpu_used -= vm->cpu_num;
            host->mem_used -= vm->mem_size;
            storage_sys->disk_used -= vm->sys_vdi_size;
            if (vm->usr_vdi_size > 0) {
                storage_usr->disk_used -= vm->usr_vdi_size;
            }

            rack->vm_cpu_used -= vm->cpu_num;
            rack->vm_mem_used -= vm->mem_size;
            rack->vm_disk_used -= vm->sys_vdi_size + vm->usr_vdi_size;

            memset(vm->sys_sr_name, 0, LC_NAMESIZE);
            if (vm->usr_vdi_size > 0) {
                memset(vm->usr_sr_name, 0, LC_NAMESIZE);
            }
            memset(vm->server, 0, LC_IPV4_LEN);
            vm->pool_id = 0;
            memset(vm->domain, 0, LC_DOMAIN_LEN);
        }
    }

    return 0;
}

int lc_alloc_vgateway(struct lc_head *vgws, struct lc_head *vgw_host)
{
    head racks;
    node *n_rack = 0,*n_vgw = 0;
    struct lc_rack *rack = 0;
    struct lc_vgw *first_vgw = 0, *vgw = 0;
    int ret = LC_ALLOC_RET_OK, vgw_num;

    if (vgws && !SLIST_EMPTY(vgws) && (!vgw_host || SLIST_EMPTY(vgw_host))) {
        SLIST_FOREACH(n_vgw, vgws, chain) {
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                continue;
            }
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
        }
        return LC_ALLOC_RET_GATEWAY_FAILED;
    }

    if (vgws && !SLIST_EMPTY(vgws)) {
        SLIST_FOREACH(n_vgw, vgws, chain) {
            if (!n_vgw) {
                continue;
            }
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                continue;
            }
            vgw->alloc_errno ^= vgw->alloc_errno;
        }
    }

    vgw_num = 0;
    if (vgws && !SLIST_EMPTY(vgws)) {

        n_vgw = SLIST_FIRST(vgws);
        if (n_vgw) {
            first_vgw = (struct lc_vgw *)n_vgw->element;
        }
        if (!n_vgw || !first_vgw) {
            logger(LOG_INFO, "No vGATEWAY\n");
            return LC_ALLOC_RET_GATEWAY_FAILED;
        }
    }

    SLIST_INIT(&racks);
    if (racks_construct(&racks, NULL, vgw_host, NULL) || SLIST_EMPTY(&racks)) {
        ret |= LC_ALLOC_RET_FAILED;
        goto finish;
    }
    sort_slist(&racks, cmprsn_rack);

    /* try gateway */
    if (vgw) {
        SLIST_FOREACH(n_rack, &racks, chain) {
            rack = (struct lc_rack *)n_rack->element;
            if (!rack) {
                continue;
            }
            logger(LOG_DEBUG, "Try vGATEWAY in rack %d\n", rack->id);
            if (try_vgateways_in_rack(vgws, rack)) {
                continue;
            }

            ret |= LC_ALLOC_RET_OK;
            goto finish;
        }
        /* failed */
        ret |= LC_ALLOC_RET_GATEWAY_FAILED;
    }

finish:

    racks_destruct(&racks);
    if (ret & LC_ALLOC_RET_GATEWAY_FAILED) {
        if (!SLIST_EMPTY(vgws)) {
            SLIST_FOREACH(n_vgw, vgws, chain) {
                vgw = (struct lc_vgw *)n_vgw->element;
                if (!vgw) {
                    continue;
                }
                first_vgw->alloc_errno |= vgw->alloc_errno;
            }
            SLIST_FOREACH(n_vgw, vgws, chain) {
                vgw = (struct lc_vgw *)n_vgw->element;
                if (!vgw) {
                    continue;
                }
                vgw->alloc_errno = first_vgw->alloc_errno;
            }
        }
    }

    return ret;
}

int lc_alloc_vm(struct lc_head *vms, struct lc_head *vm_host,
                struct lc_head *vgws, struct lc_head *vgw_host,
                struct lc_head *ips, int flag)
{
    head racks, vm_to_allocate;
    node *n_vm = 0, *n_rack = 0, *vm_nodes = 0, *n_vgw = 0;
    struct lc_rack *rack = 0;
    struct lc_vgw *first_vgw = 0, *vgw = 0;
    struct lc_vm *vm = 0;
    int ret = LC_ALLOC_RET_OK, i, vm_num, vm_left, vgw_num;

    SLIST_INIT(&vm_to_allocate);
    if ((!vms || SLIST_EMPTY(vms)) && (!vgws || SLIST_EMPTY(vgws))) {
        return LC_ALLOC_RET_OK;
    }
    if (vms && !SLIST_EMPTY(vms) && (!vm_host || SLIST_EMPTY(vm_host))) {
        SLIST_FOREACH(n_vm, vms, chain) {
            vm = (struct lc_vm *)n_vm->element;
            if (!vm) {
                continue;
            }
            vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_HOST;
        }
        ret |= LC_ALLOC_RET_ALL_VM_FAILED;
    }
    if (vgws && !SLIST_EMPTY(vgws) && (!vgw_host || SLIST_EMPTY(vgw_host))) {
        SLIST_FOREACH(n_vgw, vgws, chain) {
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                continue;
            }
            vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_HOST;
        }
        return LC_ALLOC_RET_GATEWAY_FAILED;
    }

    /* clear error number */
    if (!(ret & LC_ALLOC_RET_ALL_VM_FAILED)) {
        if (vms && !SLIST_EMPTY(vms)) {
            SLIST_FOREACH(n_vm, vms, chain) {
                if (!n_vm) {
                    continue;
                }
                vm = (struct lc_vm *)n_vm->element;
                if (!vm) {
                    continue;
                }
                vm->alloc_errno ^= vm->alloc_errno;
            }
        }
    }
    if (vgws && !SLIST_EMPTY(vgws)) {
        SLIST_FOREACH(n_vgw, vgws, chain) {
            if (!n_vgw) {
                continue;
            }
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                continue;
            }
            vgw->alloc_errno ^= vgw->alloc_errno;
        }
    }

    vgw_num = 0;
    if (vgws && !SLIST_EMPTY(vgws)) {
        SLIST_FOREACH(n_vgw, vgws, chain) {
            ++vgw_num;
        }
        if (vgw_num > 2) {
            logger(LOG_ERR, "Too many vGWs\n");
            SLIST_FOREACH(n_vgw, vgws, chain) {
                vgw = (struct lc_vgw *)n_vgw->element;
                if (!vgw) {
                    continue;
                }
                vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_TOO_MANY_VGWS;
            }
            return LC_ALLOC_RET_GATEWAY_FAILED;
        }

        n_vgw = SLIST_FIRST(vgws);
        if (n_vgw) {
            first_vgw = (struct lc_vgw *)n_vgw->element;
        }
        if (!n_vgw || !first_vgw) {
            logger(LOG_INFO, "No vGW\n");
            return LC_ALLOC_RET_GATEWAY_FAILED;
        }
        SLIST_FOREACH(n_vgw, vgws, chain) {
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                continue;
            }
            for (i = 0; i < LC_VGW_PUBINTF_NUM; ++i) {
                if (vgw->ip_num[i] != first_vgw->ip_num[i]) {
                    logger(LOG_ERR, "vGW IP info mismatch\n")
                    SLIST_FOREACH(n_vgw, vgws, chain) {
                        vgw = (struct lc_vgw *)n_vgw->element;
                        if (!vgw) {
                            continue;
                        }
                        vgw->alloc_errno |=
                            LC_ALLOC_VGW_ERRNO_VGWS_INFO_MISMATCH;
                    }
                    return LC_ALLOC_RET_GATEWAY_FAILED;
                }
            }
            if (vgw->cpu_num != first_vgw->cpu_num ||
                vgw->mem_size != first_vgw->mem_size ||
                vgw->sys_vdi_size > first_vgw->sys_vdi_size) {

                if (vgw->cpu_num != first_vgw->cpu_num) {
                    logger(LOG_ERR, "vGW CPU info mismatch\n");
                }
                if (vgw->mem_size > first_vgw->mem_size) {
                    logger(LOG_ERR, "vGW memory info mismatch\n");
                }
                if (vgw->sys_vdi_size > first_vgw->sys_vdi_size) {
                    logger(LOG_ERR, "vGW storage info mismatch\n");
                }
                SLIST_FOREACH(n_vgw, vgws, chain) {
                    vgw = (struct lc_vgw *)n_vgw->element;
                    if (!vgw) {
                        continue;
                    }
                    vgw->alloc_errno |=
                        LC_ALLOC_VGW_ERRNO_VGWS_INFO_MISMATCH;
                }
                return LC_ALLOC_RET_GATEWAY_FAILED;
            }
        }
    }

    vm_num = 0;
    if (vms && !SLIST_EMPTY(vms)) {
        SLIST_FOREACH(n_vm, vms, chain) {
            ++vm_num;
        }
    }
    if (vm_num > 0) {
        vm_nodes = calloc(vm_num, sizeof(node));
        if (!vm_nodes) {
            logger(LOG_ERR, "Memory allocate failed\n");
            return LC_ALLOC_RET_FAILED;
        }
        SLIST_INIT(&vm_to_allocate);
        i = 0;
        SLIST_FOREACH(n_vm, vms, chain) {
            if (i < vm_num) {
                vm_nodes[i].element = n_vm->element;
                SLIST_INSERT_HEAD(&vm_to_allocate, vm_nodes + i, chain);
                ++i;
            } else {
                ret |= LC_ALLOC_RET_FAILED;
                goto finish;
            }
        }
    }

    SLIST_INIT(&racks);
    if (racks_construct(&racks, vm_host, vgw_host, ips) || SLIST_EMPTY(&racks)) {
        ret |= LC_ALLOC_RET_FAILED;
        goto finish;
    }
    if (flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) {
        sort_slist(&racks, cmprsn_rack_no_cpu);
    } else {
        sort_slist(&racks, cmprsn_rack);
    }

    /* try to allocate everything on one single rack */
    if (!(ret & LC_ALLOC_RET_ALL_VM_FAILED)) {
        logger(LOG_DEBUG, "Trying to allocate vGW and VM in one rack\n");
        SLIST_FOREACH(n_rack, &racks, chain) {
            rack = (struct lc_rack *)n_rack->element;
            if (!rack) {
                continue;
            }
            if (vgw) {
                logger(LOG_DEBUG, "Try vGW in rack %d\n", rack->id);
                if (try_vgws_in_rack(vgws, rack, ips, flag)) {
                    continue;
                }
            }

            if (!vms || SLIST_EMPTY(vms)) {
                ret |= LC_ALLOC_RET_OK;
                goto finish;
            }
            logger(LOG_DEBUG, "Try VM in rack %d\n", rack->id);
            if (try_vms_in_rack(&vm_to_allocate, rack, flag)) {
                ret |= LC_ALLOC_RET_FAILED;
                goto finish;
            }
            if (SLIST_EMPTY(&vm_to_allocate)) {
                ret |= LC_ALLOC_RET_OK;
                goto finish;
            }

            /* revert vgw and vm */
            logger(LOG_DEBUG, "Revert vGW in rack %d\n", rack->id);
            if (vgw) {
                revert_vgws_in_rack(vgws, rack);
            }
            logger(LOG_DEBUG, "Revert VM in rack %d\n", rack->id);
            revert_vms_in_rack(&vm_to_allocate, rack);
        }

        /* vgw failed */
        if (!vms || SLIST_EMPTY(vms)) {
            ret |= LC_ALLOC_RET_GATEWAY_FAILED;
            goto finish;
        }

        /* try vm first */
        logger(LOG_DEBUG, "Trying to allocate vGW and VM in different racks\n");
        SLIST_FOREACH(n_rack, &racks, chain) {
            rack = (struct lc_rack *)n_rack->element;
            if (!rack) {
                continue;
            }
            logger(LOG_DEBUG, "Try VM in rack %d\n", rack->id);
            if (try_vms_in_rack(&vm_to_allocate, rack, flag)) {
                ret |= LC_ALLOC_RET_FAILED;
                goto finish;
            }
            if (SLIST_EMPTY(&vm_to_allocate)) {
                break;
            }
        }
        if (!SLIST_EMPTY(&vm_to_allocate)) {
            vm_left = 0;
            if (!SLIST_EMPTY(&vm_to_allocate)) {
                SLIST_FOREACH(n_vm, &vm_to_allocate, chain) {
                    if (!n_vm) {
                        continue;
                    }
                    vm = (struct lc_vm *)n_vm->element;
                    if (!vm) {
                        continue;
                    }
                    vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_HOST;
                    ++vm_left;
                }
            }
            if (vm_left == vm_num) {
                ret |= LC_ALLOC_RET_ALL_VM_FAILED;
            } else {
                ret |= LC_ALLOC_RET_SOME_VM_FAILED;
            }
        }
    }

    /* try gateway */
    if (vgw) {
        sort_slist(&racks, cmprsn_rack_by_vm_allocated_num);
        SLIST_FOREACH(n_rack, &racks, chain) {
            rack = (struct lc_rack *)n_rack->element;
            if (!rack) {
                continue;
            }
            logger(LOG_DEBUG, "Try vGW in rack %d\n", rack->id);
            if (try_vgws_in_rack(vgws, rack, ips, flag)) {
                continue;
            }

            ret |= LC_ALLOC_RET_OK;
            goto finish;
        }
        /* failed */
        ret |= LC_ALLOC_RET_GATEWAY_FAILED;
    }

finish:

    racks_destruct(&racks);
    if (vm_nodes) {
        free(vm_nodes);
    }
    if (ret & LC_ALLOC_RET_GATEWAY_FAILED) {
        if (!SLIST_EMPTY(vgws)) {
            SLIST_FOREACH(n_vgw, vgws, chain) {
                vgw = (struct lc_vgw *)n_vgw->element;
                if (!vgw) {
                    continue;
                }
                first_vgw->alloc_errno |= vgw->alloc_errno;
            }
            SLIST_FOREACH(n_vgw, vgws, chain) {
                vgw = (struct lc_vgw *)n_vgw->element;
                if (!vgw) {
                    continue;
                }
                vgw->alloc_errno = first_vgw->alloc_errno;
            }
        }
    }

    return ret;
}

int lc_check_vm(struct lc_head *vms, struct lc_head *vm_host,
                struct lc_head *vgws, struct lc_head *vgw_host, int flag)
{
    node *n_host = 0, *n_storage = 0, *n_s_storage = 0, *n_vm = 0, *n_new = 0, *n_vgw = 0;
    struct lc_host *host = 0;
    struct lc_storage *storage = 0, *storage_sys = 0, *storage_usr = 0;
    struct lc_storage_with_rack *s_storage = 0;
    struct lc_head shared_storage;
    struct lc_vm *vm = 0;
    struct lc_vgw *vgw = 0;
    int ret = LC_ALLOC_RET_OK, vm_ok_exists, vm_failed_exists;

    /* check vgw */
    if (vgws && !SLIST_EMPTY(vgws)) {
        logger(LOG_DEBUG, "Check vGW\n");
        if (!vgw_host || SLIST_EMPTY(vgw_host)) {
            logger(LOG_ERR, "No vGW host\n");
        }
        SLIST_FOREACH(n_vgw, vgws, chain) {
            vgw = (struct lc_vgw *)n_vgw->element;
            if (!vgw) {
                ret |= LC_ALLOC_RET_GATEWAY_FAILED;
                break;
            }
            SLIST_FOREACH(n_host, vgw_host, chain) {
                host = (struct lc_host *)n_host->element;
                if (!host) {
                    continue;
                }
                if (!strcmp(host->address, vgw->server)) {
                    break;
                }
            }
            if (!n_host || !host || strcmp(host->address, vgw->server)) {
                logger(LOG_ERR, "No vGW host %s for vGW %d\n", vgw->server, vgw->id);
                ret |= LC_ALLOC_RET_GATEWAY_FAILED;
                break;
            }
            if (!host ||
                strcmp(vgw->server, host->address) != 0 ||
                vgw->pool_id != host->pool_id ||
                strcmp(vgw->domain, host->domain) != 0 ||
                SLIST_EMPTY(&host->storage) ||
                (!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                vgw->cpu_num + host->cpu_used > host->cpu_total) ||
                vgw->mem_size + host->mem_used > host->mem_total) {

                if (!host) {
                    logger(LOG_DEBUG, "Host empty\n");
                } else {
                    if (SLIST_EMPTY(&host->storage)) {
                        logger(LOG_DEBUG, "Host storage empty\n");
                    }
                    if (strcmp(vgw->server, host->address) != 0 ||
                        vgw->pool_id != host->pool_id ||
                        strcmp(vgw->domain, host->domain) != 0) {

                        logger(LOG_DEBUG, "vGW host info mismatch\n");
                    }
                    if (!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                            vgw->cpu_num + host->cpu_used > host->cpu_total) {
                        logger(LOG_DEBUG, "Insufficient CPU in server %s in domain %s\n", host->address, host->domain);
                        vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_CPU;
                    }
                    if (vgw->mem_size + host->mem_used > host->mem_total) {
                        logger(LOG_DEBUG, "Insufficient memory in server %s in domain %s\n", host->address, host->domain);
                        vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_DISK;
                    }
                }
                ret |= LC_ALLOC_RET_GATEWAY_FAILED;
                break;
            } else {
                SLIST_FOREACH(n_storage, &host->storage, chain) {
                    storage = (struct lc_storage *)n_storage->element;
                    if (!storage) {
                        continue;
                    }
                    if (strcmp(vgw->sys_sr_name, storage->name_label) == 0) {
                        break;
                    }
                }
                if (!n_storage || !storage ||
                    strcmp(vgw->sys_sr_name, storage->name_label) != 0 ||
                    vgw->sys_vdi_size + storage->disk_used > storage->disk_total) {

                    if (vgw->sys_vdi_size + storage->disk_used > storage->disk_total) {
                        logger(LOG_DEBUG, "Insufficient storage in server %s in domain %s\n", host->address, host->domain);
                        vgw->alloc_errno |= LC_ALLOC_VGW_ERRNO_NO_DISK;
                    }
                    ret |= LC_ALLOC_RET_GATEWAY_FAILED;
                    break;
                }
                host->cpu_used += vgw->cpu_num;
                host->mem_used += vgw->mem_size;
                storage->disk_used += vgw->sys_vdi_size;
            }
        }
    }

    /* check vm */
    SLIST_INIT(&shared_storage);
    vm_ok_exists = 0;
    vm_failed_exists = 0;
    if (vms && !SLIST_EMPTY(vms)) {
        logger(LOG_DEBUG, "Check VMs\n");
        if (!vm_host || SLIST_EMPTY(vm_host)) {
            ret |= LC_ALLOC_RET_ALL_VM_FAILED;
        } else {
            SLIST_FOREACH(n_vm, vms, chain) {
                vm = (struct lc_vm *)n_vm->element;
                if (!vm) {
                    continue;
                }
                SLIST_FOREACH(n_host, vm_host, chain) {
                    host = (struct lc_host *)n_host->element;
                    if (!host) {
                        continue;
                    }
                    if (strcmp(vm->server, host->address) == 0 &&
                        vm->pool_id == host->pool_id &&
                        strcmp(vm->domain, host->domain) == 0) {

                        break;
                    }
                }
                if (!host ||
                    strcmp(vm->server, host->address) != 0 ||
                    vm->pool_id != host->pool_id ||
                    strcmp(vm->domain, host->domain) != 0 ||
                    SLIST_EMPTY(&host->storage) ||
                    (!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                    vm->cpu_num > host->cpu_total - host->cpu_used) ||
                    vm->mem_size > host->mem_total - host->mem_used) {

                    if (!host) {
                        logger(LOG_DEBUG, "Host empty\n");
                    } else {
                        if (SLIST_EMPTY(&host->storage)) {
                            logger(LOG_DEBUG, "Host storage empty\n");
                        }
                        if (strcmp(vm->server, host->address) != 0 || vm->pool_id != host->pool_id ||
                            strcmp(vm->domain, host->domain) != 0) {

                            logger(LOG_DEBUG, "VM host info mismatch\n");
                        }
                        if (!(flag & LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION) &&
                                vm->cpu_num > host->cpu_total - host->cpu_used) {
                            logger(LOG_DEBUG, "Insufficient CPU in server %s in domain %s\n", host->address, host->domain);
                            vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_CPU;
                        }
                        if (vm->mem_size > host->mem_total - host->mem_used) {
                            logger(LOG_DEBUG, "Insufficient memory in server %s in domain %s\n", host->address, host->domain);
                            vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_MEMORY;
                        }
                    }

                    vm_failed_exists = 1;
                    memset(vm->sys_sr_name, 0, LC_NAMESIZE);
                    memset(vm->usr_sr_name, 0, LC_NAMESIZE);
                    memset(vm->server, 0, LC_IPV4_LEN);
                    vm->pool_id = 0;
                    memset(vm->domain, 0, LC_DOMAIN_LEN);
                    continue;
                }
                storage_sys = 0;
                storage_usr = 0;
                SLIST_FOREACH(n_storage, &host->storage, chain) {
                    storage = (struct lc_storage *)n_storage->element;
                    if (!storage) {
                        continue;
                    }
                    if (!strcmp(storage->name_label, vm->sys_sr_name)) {
                        storage_sys = storage;
                    }
                    if (vm->usr_vdi_size > 0) {
                        if (!strcmp(storage->name_label, vm->usr_sr_name)) {
                            storage_usr = storage;
                        }
                    }
                    if (storage_sys && (vm->usr_vdi_size <= 0 || storage_usr)) {
                        break;
                    }
                }
                if (!storage_sys ||
                    strcmp(storage_sys->name_label, vm->sys_sr_name)) {

                    logger(LOG_DEBUG, "No storage %s in server %s in domain %s\n", vm->sys_sr_name, host->address, host->domain);
                    vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                    vm_failed_exists = 1;
                    memset(vm->sys_sr_name, 0, LC_NAMESIZE);
                    memset(vm->usr_sr_name, 0, LC_NAMESIZE);
                    memset(vm->server, 0, LC_IPV4_LEN);
                    vm->pool_id = 0;
                    memset(vm->domain, 0, LC_DOMAIN_LEN);
                    continue;
                }
                if (vm->usr_vdi_size > 0) {
                    if (!storage_usr ||
                        strcmp(storage_usr->name_label, vm->usr_sr_name)) {

                        logger(LOG_DEBUG, "No storage %s in server %s in domain %s\n", vm->usr_sr_name, host->address, host->domain);
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                        vm_failed_exists = 1;
                        memset(vm->sys_sr_name, 0, LC_NAMESIZE);
                        memset(vm->usr_sr_name, 0, LC_NAMESIZE);
                        memset(vm->server, 0, LC_IPV4_LEN);
                        vm->pool_id = 0;
                        memset(vm->domain, 0, LC_DOMAIN_LEN);
                        continue;
                    }
                }
                if (storage_sys->storage_type == LC_STORAGE_TYPE_SHARED) {
                    n_s_storage = 0;
                    s_storage = 0;
                    if (!SLIST_EMPTY(&shared_storage)) {
                        SLIST_FOREACH(n_s_storage, &shared_storage, chain) {
                            s_storage = (struct lc_storage_with_rack *)n_s_storage->element;
                            if (!s_storage) {
                                continue;
                            }
                            if (s_storage->rack_id == host->rack_id &&
                                strcmp(s_storage->storage->name_label, storage_sys->name_label) == 0) {

                                storage_sys = s_storage->storage;
                                break;
                            }
                        }
                    }
                    if (!n_s_storage || !s_storage ||
                        storage_sys != s_storage->storage) {

                        n_new = malloc(sizeof(node));
                        s_storage = malloc(sizeof(struct lc_storage_with_rack));
                        if (!n_new || !s_storage) {
                            if (n_new) {
                                free(n_new);
                                n_new = 0;
                            }
                            if (s_storage) {
                                free(s_storage);
                                s_storage = 0;
                            }
                            while (!SLIST_EMPTY(&shared_storage)) {
                                n_new = SLIST_FIRST(&shared_storage);
                                SLIST_REMOVE_HEAD(&shared_storage, chain);
                                if (n_new) {
                                    if (n_new->element) {
                                        free(n_new->element);
                                    }
                                    free(n_new);
                                    n_new = 0;
                                }
                            }
                            /* memory error */
                            logger(LOG_ERR, "Memory allocate failed\n");
                            return LC_ALLOC_RET_FAILED;
                        }
                        memset(n_new, 0, sizeof(node));
                        memset(s_storage, 0, sizeof(struct lc_storage_with_rack));
                        n_new->element = s_storage;
                        s_storage->rack_id = host->rack_id;
                        s_storage->storage = storage_sys;
                        SLIST_INSERT_HEAD(&shared_storage, n_new, chain);
                    }
                }
                if (vm->usr_vdi_size > 0 &&
                    storage_usr->storage_type == LC_STORAGE_TYPE_SHARED) {
                    n_s_storage = 0;
                    s_storage = 0;
                    if (!SLIST_EMPTY(&shared_storage)) {
                        SLIST_FOREACH(n_s_storage, &shared_storage, chain) {
                            s_storage = (struct lc_storage_with_rack *)n_s_storage->element;
                            if (!s_storage) {
                                continue;
                            }
                            if (s_storage->rack_id == host->rack_id &&
                                strcmp(s_storage->storage->name_label, storage_usr->name_label) == 0) {

                                storage_usr = s_storage->storage;
                                break;
                            }
                        }
                    }
                    if (!n_s_storage || !s_storage ||
                        s_storage->rack_id != host->rack_id ||
                        storage_usr != s_storage->storage) {

                        n_new = malloc(sizeof(node));
                        s_storage = malloc(sizeof(struct lc_storage_with_rack));
                        if (!n_new || !s_storage) {
                            if (n_new) {
                                free(n_new);
                                n_new = 0;
                            }
                            if (s_storage) {
                                free(s_storage);
                                s_storage = 0;
                            }
                            while (!SLIST_EMPTY(&shared_storage)) {
                                n_new = SLIST_FIRST(&shared_storage);
                                SLIST_REMOVE_HEAD(&shared_storage, chain);
                                if (n_new) {
                                    if (n_new->element) {
                                        free(n_new->element);
                                    }
                                    free(n_new);
                                    n_new = 0;
                                }
                            }
                            /* memory error */
                            logger(LOG_ERR, "Memory allocate failed\n");
                            return LC_ALLOC_RET_FAILED;
                        }
                        memset(n_new, 0, sizeof(node));
                        memset(s_storage, 0, sizeof(struct lc_storage_with_rack));
                        n_new->element = s_storage;
                        s_storage->rack_id = host->rack_id;
                        s_storage->storage = storage_usr;
                        SLIST_INSERT_HEAD(&shared_storage, n_new, chain);
                    }
                }
                if (vm->sys_vdi_size >
                        storage_sys->disk_total - storage_sys->disk_used ||
                    (vm->usr_vdi_size > 0 &&
                     (vm->usr_vdi_size >
                          storage_usr->disk_total - storage_usr->disk_used ||
                      (storage_sys == storage_usr &&
                       vm->sys_vdi_size + vm->usr_vdi_size >
                           storage_sys->disk_total - storage_sys->disk_used)))) {

                    if (vm->sys_vdi_size >
                            storage_sys->disk_total - storage_sys->disk_used) {
                        logger(LOG_DEBUG, "Insufficient sys storage in SR %s in server %s in domain %s\n", vm->sys_sr_name, host->address, host->domain);
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                    }
                    if (vm->usr_vdi_size > 0 &&
                        vm->usr_vdi_size >
                            storage_usr->disk_total - storage_usr->disk_used) {
                        logger(LOG_DEBUG, "Insufficient user storage in SR %s in server %s in domain %s\n", vm->usr_sr_name, host->address, host->domain);
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                    }
                    if (vm->usr_vdi_size > 0 &&
                        storage_sys == storage_usr &&
                        vm->sys_vdi_size + vm->usr_vdi_size >
                            storage_sys->disk_total - storage_sys->disk_used) {
                        logger(LOG_DEBUG, "Insufficient sys and user storage in SR %s in server %s in domain %s\n", vm->usr_sr_name, host->address, host->domain);
                        vm->alloc_errno |= LC_ALLOC_VM_ERRNO_NO_DISK;
                    }
                    vm_failed_exists = 1;
                    memset(vm->sys_sr_name, 0, LC_NAMESIZE);
                    memset(vm->usr_sr_name, 0, LC_NAMESIZE);
                    memset(vm->server, 0, LC_IPV4_LEN);
                    vm->pool_id = 0;
                    memset(vm->domain, 0, LC_DOMAIN_LEN);
                    continue;
                }
                vm_ok_exists = 1;
                host->cpu_used += vm->cpu_num;
                host->mem_used += vm->mem_size;
                storage_sys->disk_used += vm->sys_vdi_size;
                if (vm->usr_vdi_size > 0) {
                    storage_usr->disk_used += vm->usr_vdi_size;
                }
            }
        }
    }

    if (vm_failed_exists) {
        if (vm_ok_exists) {
            ret |= LC_ALLOC_RET_SOME_VM_FAILED;
        } else {
            ret |= LC_ALLOC_RET_ALL_VM_FAILED;
        }
    }

    while (!SLIST_EMPTY(&shared_storage)) {
        n_new = SLIST_FIRST(&shared_storage);
        SLIST_REMOVE_HEAD(&shared_storage, chain);
        if (n_new) {
            if (n_new->element) {
                free(n_new->element);
            }
            free(n_new);
            n_new = 0;
        }
    }

    return ret;
}

#ifdef UNIT_TEST
#include <assert.h>
#include <stdio.h>

void dump_vgw(struct lc_vgw *vgw)
{
    int i;

    if (vgw) {
        printf("VGW #%02d\n", vgw->id);
        printf("cpus:    %d\n", vgw->cpu_num);
        printf("memory:  %d\n", vgw->mem_size);
        printf("disk:    %d\n", vgw->sys_vdi_size);
        printf("SR:      %s\n", vgw->sys_sr_name);
        printf("host:    %s\n", vgw->server);
        printf("exclude: %s\n", vgw->excluded_server);
        printf("pool:    %d\n", vgw->pool_id);
        printf("domain:  %s\n", vgw->domain);
        for (i = 0; i < LC_VGW_PUBINTF_NUM; ++i) {
            printf("ip%d(%d):  %s\n", i, vgw->ip_num[i], vgw->ip[i]);
        }
        printf("errno:   0x%08x\n", vgw->alloc_errno);
    }
}

void dump_vgw_slist(head *vgws)
{
    struct lc_vgw *vgw;
    node *n;

    if (!SLIST_EMPTY(vgws)) {
        SLIST_FOREACH(n, vgws, chain) {
            vgw = (struct lc_vgw *)n->element;
            dump_vgw(vgw);
            putchar('\n');
        }
    }
}

void dump_vm(struct lc_vm *vm)
{
    if (vm) {
        printf("VM #%02d\n", vm->id);
        printf("cpus:    %d\n", vm->cpu_num);
        printf("memory:  %d\n", vm->mem_size);
        printf("disk0:   %d\n", vm->sys_vdi_size);
        printf("SR0:     %s\n", vm->sys_sr_name);
        printf("disk1:   %d\n", vm->usr_vdi_size);
        printf("SR1:     %s\n", vm->usr_sr_name);
        printf("host:    %s\n", vm->server);
        printf("exclude: %s\n", vm->excluded_server);
        printf("pool:    %d\n", vm->pool_id);
        printf("domain:  %s\n", vm->domain);
        printf("errno:   0x%08x\n", vm->alloc_errno);
    }
}

void dump_vm_slist(head *vms)
{
    struct lc_vm *vm;
    node *n;

    if (!SLIST_EMPTY(vms)) {
        SLIST_FOREACH(n, vms, chain) {
            vm = (struct lc_vm *)n->element;
            dump_vm(vm);
            putchar('\n');
        }
    }
}

void dump_storage(struct lc_storage *s)
{
    if (s) {
        printf("%s, ", s->name_label);
        if (s->storage_type == LC_STORAGE_TYPE_LOCAL) {
            printf("local, ");
        } else if (s->storage_type == LC_STORAGE_TYPE_SHARED) {
            printf("shared, ");
        }
        printf("%d/%d used\n", s->disk_used, s->disk_total);
    }
}

void dump_storage_slist(head *storages)
{
    struct lc_storage *s;
    node *n;

    if (!SLIST_EMPTY(storages)) {
        SLIST_FOREACH(n, storages, chain) {
            s = (struct lc_storage *)n->element;
            dump_storage(s);
        }
    }
}

void dump_storage_with_rack(struct lc_storage_with_rack *s_r)
{
    struct lc_storage *s;
    if (s_r) {
        s = s_r->storage;
        if (s) {
            printf("rack %d, %s, ", s_r->rack_id, s->name_label);
            if (s->storage_type == LC_STORAGE_TYPE_LOCAL) {
                printf("local, ");
            } else if (s->storage_type == LC_STORAGE_TYPE_SHARED) {
                printf("shared, ");
            }
            printf("%d/%d used\n", s->disk_used, s->disk_total);
        }
    }
}

void dump_storage_with_rack_slist(head *storages)
{
    struct lc_storage_with_rack *s;
    node *n;

    if (!SLIST_EMPTY(storages)) {
        SLIST_FOREACH(n, storages, chain) {
            s = (struct lc_storage_with_rack *)n->element;
            dump_storage_with_rack(s);
        }
    }
}

void dump_host(struct lc_host *host)
{
    if (host) {
        printf("host #%02d\n", host->id);
        printf("address: %s\n", host->address);
        printf("pool:    %d\n", host->pool_id);
        printf("domain:  %s\n", host->domain);
        printf("rack:    %d\n", host->rack_id);
        printf("cpu:     %d/%d used\n", host->cpu_used, host->cpu_total);
        printf("memory:  %d/%d used\n", host->mem_used, host->mem_total);
        printf("storage:\n");
        dump_storage_slist(&host->storage);
    }
}

void dump_host_slist(head *hosts)
{
    struct lc_host *host;
    node *n;

    if (!SLIST_EMPTY(hosts)) {
        SLIST_FOREACH(n, hosts, chain) {
            host = (struct lc_host *)n->element;
            dump_host(host);
            putchar('\n');
        }
    }
}

void dump_rack(struct lc_rack *rack)
{
    if (rack) {
        printf("rack #%02d\n", rack->id);
        printf("vm host:\n");
        dump_host_slist(&rack->vm_host);
        printf("cpu:     %d/%d used\n", rack->vm_cpu_used, rack->vm_cpu_total);
        printf("memory:  %d/%d used\n", rack->vm_mem_used, rack->vm_mem_total);
        printf("disk:    %d/%d used\n", rack->vm_disk_used, rack->vm_disk_total);
        putchar('\n');
        printf("vgw host:\n");
        dump_host_slist(&rack->vgw_host);
        printf("cpu:     %d\n", rack->max_vgw_cpu);
        printf("memory:  %d\n", rack->max_vgw_mem);
        printf("disk:    %d\n", rack->max_vgw_disk);
        printf("ips:     %d\n", rack->max_ips);
        putchar('\n');
        printf("storage:\n");
        dump_storage_slist(&rack->shared_storage);
        printf("vms:     %d\n", rack->vm_allocated_num);
    }
}

void dump_rack_slist(head *racks)
{
    struct lc_rack *rack;
    node *n;

    if (!SLIST_EMPTY(racks)) {
        SLIST_FOREACH(n, racks, chain) {
            rack = (struct lc_rack *)n->element;
            dump_rack(rack);
            putchar('\n');
        }
    }
}

void dump_ip(struct lc_ip *ip)
{
    if (ip) {
        printf("ip:      %s\n", ip->address);
        printf("pool:    %d\n", ip->pool_id);
    }
}

void dump_ip_slist(head *ips)
{
    struct lc_ip *ip;
    node *n;

    if (!SLIST_EMPTY(ips)) {
        SLIST_FOREACH(n, ips, chain) {
            ip = (struct lc_ip *)n->element;
            dump_ip(ip);
            putchar('\n');
        }
    }
}

int main()
{
    struct lc_host h1 = {
        .id = 10,
        .address = "172.16.2.105",
        .domain = "aptx4849"
    };
    node n_h1 = {
        .element = &h1
    };
    struct lc_storage s1 = {
        .name_label = "SR_Local",
        .storage_type = LC_STORAGE_TYPE_LOCAL
    };
    node n_s1 = {
        .element = &s1
    };
    SLIST_INIT(&h1.storage);
    SLIST_INSERT_HEAD(&h1.storage, &n_s1, chain);
    struct lc_host h2 = {
        .id = 11,
        .address = "172.16.2.110",
        .domain = "aptx4849",
    };
    node n_h2 = {
        .element = &h2
    };
    struct lc_storage s2 = {
        .name_label = "SR_Local",
        .storage_type = LC_STORAGE_TYPE_LOCAL
    };
    node n_s2 = {
        .element = &s2
    };
    SLIST_INIT(&h2.storage);
    SLIST_INSERT_HEAD(&h2.storage, &n_s2, chain);
    head vgw_host;
    SLIST_INIT(&vgw_host);
    SLIST_INSERT_HEAD(&vgw_host, &n_h1, chain);
    SLIST_INSERT_HEAD(&vgw_host, &n_h2, chain);
    struct lc_host h3 = {
        .id = 12,
        .address = "172.16.2.107",
        .domain = "aptx4849"
    };
    node n_h3 = {
        .element = &h3
    };
    struct lc_storage s3 = {
        .name_label = "SR_Local",
        .storage_type = LC_STORAGE_TYPE_LOCAL
    };
    node n_s3 = {
        .element = &s3
    };
    SLIST_INIT(&h3.storage);
    SLIST_INSERT_HEAD(&h3.storage, &n_s3, chain);
    struct lc_host h4 = {
        .id = 13,
        .address = "172.16.2.108",
        .domain = "aptx4849",
    };
    node n_h4 = {
        .element = &h4
    };
    struct lc_storage s4 = {
        .name_label = "SR_Local",
        .storage_type = LC_STORAGE_TYPE_LOCAL
    };
    node n_s4 = {
        .element = &s4
    };
    SLIST_INIT(&h4.storage);
    SLIST_INSERT_HEAD(&h4.storage, &n_s4, chain);
    struct lc_host h5 = {
        .id = 14,
        .address = "172.16.2.104",
        .domain = "aptx4849"
    };
    node n_h5 = {
        .element = &h5
    };
    struct lc_storage s5 = {
        .name_label = "datastore",
        .storage_type = LC_STORAGE_TYPE_SHARED
    };
    node n_s5 = {
        .element = &s5
    };
    SLIST_INIT(&h5.storage);
    SLIST_INSERT_HEAD(&h5.storage, &n_s5, chain);
    struct lc_host h6 = {
        .id = 16,
        .address = "172.16.2.113",
        .domain = "aptx4849",
    };
    node n_h6 = {
        .element = &h6
    };
    struct lc_storage s6 = {
        .name_label = "datastore",
        .storage_type = LC_STORAGE_TYPE_SHARED
    };
    node n_s6 = {
        .element = &s6
    };
    SLIST_INIT(&h6.storage);
    SLIST_INSERT_HEAD(&h6.storage, &n_s6, chain);
    head vm_host;
    SLIST_INIT(&vm_host);
    SLIST_INSERT_HEAD(&vm_host, &n_h3, chain);
    SLIST_INSERT_HEAD(&vm_host, &n_h4, chain);
    SLIST_INSERT_HEAD(&vm_host, &n_h5, chain);
    SLIST_INSERT_HEAD(&vm_host, &n_h6, chain);

    struct lc_ip ip1 = {
        .address = "192.168.100.11",
        .isp = 1,
        .pool_id = 0
    };
    node n_ip1 = {
        .element = &ip1
    };
    struct lc_ip ip2 = {
        .address = "192.168.100.12",
        .isp = 2,
        .pool_id = 0
    };
    node n_ip2 = {
        .element = &ip2
    };
    struct lc_ip ip3 = {
        .address = "192.168.100.13",
        .isp = 3,
        .pool_id = 0
    };
    node n_ip3 = {
        .element = &ip3
    };
    struct lc_ip ip4 = {
        .address = "192.168.100.14",
        .isp = 1,
        .pool_id = 0
    };
    node n_ip4 = {
        .element = &ip4
    };
    struct lc_ip ip5 = {
        .address = "192.168.100.15",
        .isp = 3,
        .pool_id = 1
    };
    node n_ip5 = {
        .element = &ip5
    };
    struct lc_ip ip6 = {
        .address = "192.168.100.16",
        .isp = 2,
        .pool_id = 0
    };
    node n_ip6 = {
        .element = &ip6
    };
    struct lc_ip ip7 = {
        .address = "192.168.100.17",
        .isp = 1,
        .pool_id = 1
    };
    node n_ip7 = {
        .element = &ip7
    };
    struct lc_ip ip8 = {
        .address = "192.168.100.18",
        .isp = 2,
        .pool_id = 1
    };
    node n_ip8 = {
        .element = &ip8
    };
    struct lc_ip ip9 = {
        .address = "192.168.100.19",
        .isp = 3,
        .pool_id = 1
    };
    node n_ip9 = {
        .element = &ip9
    };
    struct lc_ip ip10 = {
        .address = "192.168.100.110",
        .isp = 1,
        .pool_id = 1
    };
    node n_ip10 = {
        .element = &ip10
    };
    struct lc_ip ip11 = {
        .address = "192.168.100.111",
        .isp = 2,
        .pool_id = 1
    };
    node n_ip11 = {
        .element = &ip11
    };
    head ips;
    SLIST_INIT(&ips);
    SLIST_INSERT_HEAD(&ips, &n_ip1, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip2, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip3, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip4, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip5, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip6, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip7, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip8, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip9, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip10, chain);
    SLIST_INSERT_HEAD(&ips, &n_ip11, chain);

    h1.pool_id = 1;
    h1.rack_id = 1;
    h1.cpu_total = 16;
    h1.cpu_used = 10;
    h1.mem_total = 16384;
    h1.mem_used = 1000;
    h1.pool_ip_type = LC_POOL_IP_TYPE_EXCLUSIVE;
    s1.disk_total = 1000;
    s1.disk_used = 20;
    h2.pool_id = 1;
    h2.rack_id = 1;
    h2.cpu_total = 16;
    h2.cpu_used = 10;
    h2.mem_total = 16384;
    h2.mem_used = 33;
    h2.pool_ip_type = LC_POOL_IP_TYPE_EXCLUSIVE;
    s2.disk_total = 1000;
    s2.disk_used = 100;
    h3.pool_id = 3;
    h3.rack_id = 3;
    h3.cpu_total = 16;
    h3.cpu_used = 16;
    h3.mem_total = 16384;
    h3.mem_used = 15000;
    s3.disk_total = 1000;
    s3.disk_used = 980;
    h4.pool_id = 3;
    h4.rack_id = 4;
    h4.cpu_total = 16;
    h4.cpu_used = 14;
    h4.mem_total = 16384;
    h4.mem_used = 15000;
    s4.disk_total = 1000;
    s4.disk_used = 100;
    h5.pool_id = 2;
    h5.rack_id = 5;
    h5.cpu_total = 16;
    h5.cpu_used = 16;
    h5.mem_total = 16384;
    h5.mem_used = 15000;
    s5.disk_total = 1000;
    s5.disk_used = 990;
    h6.pool_id = 2;
    h6.rack_id = 6;
    h6.cpu_total = 16;
    h6.cpu_used = 16;
    h6.mem_total = 16384;
    h6.mem_used = 16000;
    s6.disk_total = 1000;
    s6.disk_used = 860;
    struct lc_vgw vgw1 = {
        .id = 4,
        .cpu_num = 2,
        .mem_size = 1024,
        .sys_vdi_size = 30,
        .excluded_server = "",
        .ip_num = { 2, 2, 2 },
        .ip_isp = { 1, 2, 3 },
        .server = "172.16.2.105",
        .pool_id = 1,
        .domain = "aptx4849",
        .sys_sr_name = "SR_Local"
    };
    node n_vgw1 = {
        .element = &vgw1
    };
    struct lc_vgw vgw2 = {
        .id = 5,
        .cpu_num = 2,
        .mem_size = 1024,
        .sys_vdi_size = 30,
        .excluded_server = "",
        .ip_num = { 2, 2, 2 },
        .ip_isp = { 1, 2, 3 },
        .server = "172.16.2.110",
        .pool_id = 1,
        .domain = "aptx4849",
        .sys_sr_name = "SR_Local"
    };
    node n_vgw2 = {
        .element = &vgw2
    };
    struct lc_vgw vgw3 = {
        .id = 6,
        .cpu_num = 2,
        .mem_size = 1024,
        .sys_vdi_size = 30,
        .excluded_server = "",
        .ip_num = { 3, 3, 3 },
        .ip_isp = { 1, 2, 3 },
        .server = "172.16.2.105",
        .pool_id = 1,
        .domain = "aptx4849",
        .sys_sr_name = "SR_Local"
    };
    node n_vgw3 = {
        .element = &vgw3
    };
    struct lc_vm vm1 = {
        .id = 1,
        .cpu_num = 2,
        .mem_size = 1024,
        .sys_vdi_size = 30,
        .usr_vdi_size = 80,
#if 0
        .server = "172.16.2.104",
        .pool_id = 2,
        .domain = "aptx4849",
        .sys_sr_name = "datastore",
        .usr_sr_name = "datastore"
#endif
    };
    node n_vm1 = {
        .element = &vm1
    };
    struct lc_vm vm2 = {
        .id = 2,
        .cpu_num = 2,
        .mem_size = 1024,
        .sys_vdi_size = 30,
#if 0
        .server = "172.16.2.104",
        .pool_id = 2,
        .domain = "aptx4849",
        .sys_sr_name = "datastore"
#endif
    };
    node n_vm2 = {
        .element = &vm2
    };
    struct lc_vm vm3 = {
        .id = 3,
        .cpu_num = 2,
        .mem_size = 1024,
        .sys_vdi_size = 30,
#if 0
        .server = "172.16.2.113",
        .pool_id = 2,
        .domain = "aptx4849",
        .sys_sr_name = "datastore"
#endif
    };
    node n_vm3 = {
        .element = &vm3
    };
    head vms;
    SLIST_INIT(&vms);
    SLIST_INSERT_HEAD(&vms, &n_vm1, chain);
    SLIST_INSERT_HEAD(&vms, &n_vm2, chain);
    SLIST_INSERT_HEAD(&vms, &n_vm3, chain);
    head vgws;
    SLIST_INIT(&vgws);
    SLIST_INSERT_HEAD(&vgws, &n_vgw1, chain);
    SLIST_INSERT_HEAD(&vgws, &n_vgw2, chain);
    /* SLIST_INSERT_HEAD(&vgws, &n_vgw3, chain); */
    printf("%d\n", lc_alloc_vm(&vms, 0, &vgws, &vgw_host, &ips, LC_ALLOC_ALLOW_CPU_OVER_ALLOCATION));
    dump_vgw_slist(&vgws);

    return 0;
}

#endif
